#include <inttypes.h>
#include <rte_ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "alpha.h"
#include "al_log.h"
#include "al_vport.h"
#include "al_ip.h"
#include "al_dev.h"
#include "al_hash.h"
#include "al_router.h"
#include "al_mempool.h"
#include "common/al_nic.h"



void vport_init(uint8_t portid){

    al_dev* port=&g_vport_ary[portid];
    port->dev.vport.port.mtu = 1500;

    rte_rwlock_init(&port->dev.vport.port.ipv4_lock);
    INIT_HLIST_HEAD(&port->dev.vport.port.ipv4);
    rte_rwlock_init(&port->dev.vport.port.ipv6_lock);
    INIT_HLIST_HEAD(&port->dev.vport.port.ipv6);

}

int vport_has_ip(struct vport* vport, ipcs_t* ip, uint8_t iptype){
	
    ip_t *s;
    uint8_t rc;
    struct hlist_node *t;

    rc=0;
    if( iptype == AF_INET ){
        rte_rwlock_read_lock(&vport->ipv4_lock);
        hlist_for_each_entry_safe(s, t, &vport->ipv4, list){
            if(s->ip.ipv4==ip->ipv4){
                rc=1;
                break;
            }
        }
        rte_rwlock_read_unlock(&vport->ipv4_lock);
    }else{
        rte_rwlock_read_lock(&vport->ipv6_lock);
        hlist_for_each_entry_safe(s, t, &vport->ipv6, list){
            if(s->ip.ipv6==ip->ipv6){
                rc=1;
                break;
            }
        }
        rte_rwlock_read_unlock(&vport->ipv6_lock);
    }

    return rc;
}

int vport_primer_ip(struct vport* vport, ipcs_t* ip, uint8_t iptype){
    ip_t *s;
    uint8_t rc;
    struct hlist_node *t;
#if 0
    LOG(LOG_TRACE, LAYOUT_CONF, "%s Ip type %d  %d", __FUNCTION__, iptype, iptype == AF_INET ? hlist_empty(&vport->ipv4) : hlist_empty(&vport->ipv6));
#endif
    rc=0;
    if( iptype == AF_INET ){
        rte_rwlock_read_lock(&vport->ipv4_lock);
        hlist_for_each_entry_safe(s, t, &vport->ipv4, list){
            ip->ipv4=s->ip.ipv4;
            rc=1;
            break;
        }
        rte_rwlock_read_unlock(&vport->ipv4_lock);
    }else{
        rte_rwlock_read_lock(&vport->ipv6_lock);
        hlist_for_each_entry_safe(s, t, &vport->ipv6, list){
            ip->ipv6=s->ip.ipv6;
            rc=1;
            break;
        }
        rte_rwlock_read_unlock(&vport->ipv6_lock);
    }

    return rc;	
}

int vport_select_local_ip(struct vport* vport, ipcs_t* ip, uint8_t iptype, ipcs_t* rip){
    ip_t *s;
    uint8_t rc;
    struct hlist_node *t;
#if 0
    LOG(LOG_TRACE, LAYOUT_CONF, "%s Ip type %d  %d", __FUNCTION__, iptype, iptype == AF_INET ? hlist_empty(&vport->ipv4) : hlist_empty(&vport->ipv6));
#endif

    rc=-1;
    if( iptype == AF_INET ){
        rte_rwlock_read_lock(&vport->ipv4_lock);
        hlist_for_each_entry_safe(s, t, &vport->ipv4, list){
            if( is_same_subnet(&s->ip, rip, &s->mask, iptype)){
                rc=0;
                *ip=s->ip;
                break;
            }
        }
        rte_rwlock_read_unlock(&vport->ipv4_lock);    
    }else{
        rte_rwlock_read_lock(&vport->ipv6_lock);
        hlist_for_each_entry_safe(s, t, &vport->ipv6, list){
            if( is_same_subnet(&s->ip, rip, &s->mask, iptype)){
                *ip=s->ip;
                rc=0;
                break; 
            }
        }
        rte_rwlock_read_unlock(&vport->ipv6_lock);
    }

    return rc;	
}
#if 0
int  vport_add_ip(uint8_t portid, ip_t* ip){

    al_dev* port=&g_vport_ary[portid];
    struct vport* vport=&port->dev.vport.port;

    if( ip->iptype == AF_INET ){
        rte_rwlock_write_lock(&vport->ipv4_lock);
        hlist_add_head(&ip->list, &vport->ipv4);
        rte_rwlock_write_unlock(&vport->ipv4_lock);
    }else{
        rte_rwlock_write_lock(&vport->ipv6_lock);
        hlist_add_head(&ip->list, &vport->ipv6);
        rte_rwlock_write_unlock(&vport->ipv6_lock);
    }
    return 0;
}
#endif

int  al_add_ip(uint8_t ifindex, uint64_t ipid, uint32_t portid, al_addr_t* addr, al_addr_t* maddr, uint8_t ipclass){

    al_dev* port;
    struct vport* vport;
    ip_t* ip;
    char ipaddr[128];
    uint8_t prefix;

    int i;
    port=NULL;
    for(i=0; i<g_sys_port_counter; i++){
         if(g_vport_ary[i].id==portid){
             port=&g_vport_ary[i];
             break;
         }
    }

    if( port == NULL ){
        return -1;
    }else{
        vport=&port->dev.vport.port;
        ip=(ip_t*)al_malloc(sizeof(ip_t));
        if( ip == NULL ){
            return -1;
        }
    }

    ip->id=ipid;
    if( al_p_addr_af(addr) == AF_INET ){
        ip->ip.ipv4=al_addr_addr((*(al_addr4_t*)addr));
        ip->mask.ipv4=al_addr_addr((*(al_addr4_t*)maddr));
        prefix=__builtin_popcount(ip->mask.ipv4);
        ip->iptype=AF_INET;
        rte_atomic16_inc(&ip->ref);
        ip->asstype=ipclass;
        rte_rwlock_write_lock(&vport->ipv4_lock);
        hlist_add_head(&ip->list, &vport->ipv4);
        rte_rwlock_write_unlock(&vport->ipv4_lock);
    }else if( al_p_addr_af(addr) == AF_INET6){
        ip->ip.ipv6=al_addr_addr((*(al_addr6_t*)addr));
        ip->mask.ipv6=al_addr_addr((*(al_addr6_t*)maddr));
        ip->iptype=AF_INET6;
        rte_atomic16_inc(&ip->ref);
        prefix=__builtin_popcount(ip->mask.ipv6);
        ip->asstype=ipclass;
        rte_rwlock_write_lock(&vport->ipv6_lock);
        hlist_add_head(&ip->list, &vport->ipv6);
        rte_rwlock_write_unlock(&vport->ipv6_lock);
    }else{
        return -1;
    }
    
    return al_opt_ip(ifindex, 1, al_p_addr_af(addr), inet_ntop(al_p_addr_af(addr), &ip->ip, ipaddr, 128), prefix);
}

int  al_del_ip(uint8_t ifindex, uint64_t ipid, uint32_t portid){

    al_dev* port;
    struct vport* vport;
    ip_t *s;
    int r;
    struct hlist_node *t;
    uint8_t prefix;
    char ipaddr[128];
    ipcs_t ip;
    
    int i;
    port=NULL;
    for(i=0; i<g_sys_port_counter; i++){
         if(g_vport_ary[i].id==portid){
             port=&g_vport_ary[i];
             break;
         }
    }

    if( port == NULL ){
        return -1;
    }
    
    vport=&(port)->dev.vport.port;

    r=0;

    rte_rwlock_write_lock(&vport->ipv4_lock);
    hlist_for_each_entry_safe(s, t, &vport->ipv4, list){
        if( s->id == ipid ){
            hlist_del(&s->list);
            prefix=__builtin_popcount(s->mask.ipv4);
            ip=s->ip;
            al_free(s);
            r=1;
            break;
        }
    }
    rte_rwlock_write_unlock(&vport->ipv4_lock);

    if( r ){
        return al_opt_ip(ifindex, 0, AF_INET, inet_ntop(AF_INET, &ip, ipaddr, 128), prefix);
    }
        
    rte_rwlock_write_lock(&vport->ipv6_lock);
    hlist_for_each_entry_safe(s, t, &vport->ipv6, list){
        if( s->id == ipid ){
            hlist_del(&s->list);
            prefix=__builtin_popcount(s->mask.ipv6);
            ip=s->ip;
            al_free(s);
            r=1;
            break;
        }
    }
    rte_rwlock_write_unlock(&vport->ipv6_lock); 

    if( r ){
        return al_opt_ip(ifindex, 0, AF_INET6, inet_ntop(AF_INET6, &ip, ipaddr, 128), prefix);
    }

    return 0;
}

void  vport_release_res(uint8_t portid){

    ip_t *s;
    struct hlist_node *t;

    al_dev* port=&g_vport_ary[portid];
    struct vport* vport=&port->dev.vport.port;

    rte_rwlock_write_lock(&vport->ipv4_lock);
    hlist_for_each_entry_safe(s, t, &vport->ipv4, list){
        hlist_del(&s->list);
        ip_put(s);
    }
    rte_rwlock_write_unlock(&vport->ipv4_lock);

    rte_rwlock_write_lock(&vport->ipv6_lock);
    hlist_for_each_entry_safe(s, t, &vport->ipv6, list){
        hlist_del(&s->list);
        ip_put(s);
    }
    rte_rwlock_write_unlock(&vport->ipv6_lock);
}

struct vport* vport_get(uint8_t portid){
    al_dev* port=&g_vport_ary[portid];
    return &port->dev.vport.port;
}

void vport_out_ip(FILE* fp, char* pfx_tab, struct vport* vp){
	
    ip_t *s;
    struct hlist_node *t;

    rte_rwlock_write_lock(&vp->ipv4_lock);
    hlist_for_each_entry_safe(s, t, &vp->ipv4, list){
        ip_cmd_output(fp, pfx_tab, s);
    }
    rte_rwlock_write_unlock(&vp->ipv4_lock);

    rte_rwlock_write_lock(&vp->ipv6_lock);
    hlist_for_each_entry_safe(s, t, &vp->ipv6, list){
        ip_cmd_output(fp, pfx_tab, s);
    }
    rte_rwlock_write_unlock(&vp->ipv6_lock);

}

void al_dump_vport_ip_list(FILE* fp, struct vport* vp){
    ip_t *s;
    int sig;
    struct hlist_node *t;

    sig=0;
    rte_rwlock_write_lock(&vp->ipv4_lock);
    hlist_for_each_entry_safe(s, t, &vp->ipv4, list){
        if( sig == 0){
            sig=1;
        }else{
            fprintf(fp, ",\n");
        }
        al_dump_ip(fp, "    ", s);
    }
    rte_rwlock_write_unlock(&vp->ipv4_lock);

    sig=0;
    rte_rwlock_write_lock(&vp->ipv6_lock);
    hlist_for_each_entry_safe(s, t, &vp->ipv6, list){
        if( sig == 0){
            sig=1;
        }else{
            fprintf(fp, ",\n");
        }
        al_dump_ip(fp, "    ", s);
    }
    rte_rwlock_write_unlock(&vp->ipv6_lock);
}

void al_dump_vport_ip(FILE* fp){
    
    al_dev* port;
    struct vport* vport;
    int i;
    port=NULL;
    
    for(i=0; i<g_sys_port_counter; i++){
        port=&g_vport_ary[i];
        vport=&port->dev.vport.port;
        al_dump_vport_ip_list(fp, vport);        
    }
}

#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include "alpha.h"
#include "al_session.h"
#include "al_common.h"
#include "al_mempool.h"
#include "al_filter.h"
#include "al_to_api.h"
#include "al_member.h"
#include "al_arp.h"
#include "al_log.h"
#include "al_status.h"
#include "al_tuple5.h"
#include "al_router.h"
#include "al_service.h"
#include "al_vport.h"

static inline void session_mem_init(session_t* s, int isUp){
        s->stype = SESSION_NORMAL;
        s->is_ups = isUp;
        s->port_alloc=0;
        s->stout = SESSION_IDLE_TIMEOUT;
}

static inline int session_init_local(session_t* s, struct rte_mbuf* buf, void* hdr, void* nextptr){
    struct ether_hdr* eth;
    struct tcp_hdr* tcphdr;
    al_dev* dev;
    uint16_t etype;

    eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    etype = rte_be_to_cpu_16(eth->ether_type);

    s->to_haddr=eth->d_addr;
    s->from_haddr=eth->s_addr;
    
    dev=&g_vport_ary[buf->port];
    tcphdr=(struct tcp_hdr*)nextptr;

    if(likely(etype == ETHER_TYPE_IPv4)){
        s->remote_addr.ipv4 = ((struct ipv4_hdr*)hdr)->src_addr;
        s->local_addr.ipv4 = ((struct ipv4_hdr*)hdr)->dst_addr;
        s->in_type = 0;
        s->proto = ((struct ipv4_hdr*)hdr)->next_proto_id;
    }else if(etype == ETHER_TYPE_IPv6){
        s->remote_addr.address6 = *(uint128_t*)((struct ipv6_hdr*)hdr)->src_addr;
        s->local_addr.address6 = *(uint128_t*)((struct ipv6_hdr*)hdr)->dst_addr;
        s->in_type = 1;
        s->proto = ((struct ipv6_hdr*)hdr)->proto;
    }else{
        return -1;
    }

    s->local_port = tcphdr->dst_port;
    s->remote_port = tcphdr->src_port;
    s->portid = dev->port;

    return 0;
}

static inline int session_init_remote(session_t* c, session_t* u, uint32_t fid, uint32_t gid,  struct rte_mbuf* buf, void* hdr, void* nextptr){
    struct ether_hdr* eth;
    struct tcp_hdr* tcphdr;
    al_dev* dev;
    uint16_t etype;
    vport_t* vport;
    int portint;
    uint8_t oif;
    uint8_t* aptr;
    ipcs_t   nexthop;
    al_addr4_t* ip4;
    al_addr6_t* ip6;
    ipcs_t rip;
    al_margs margs;
    al_addr4_t* addr;
    al_addr6_t* addr6;
    
    oif=0xFF;
    if(al_member_lookup(fid, gid, &margs)){
        return -1;
    }else{
        u->mid=c->mid;
        if(al_addr_af(margs.addr)==AF_INET){
            ip4=(al_addr4_t*)&margs.addr;
            aptr=al_p_addr_addr_ptr(ip4);
        }else{
            ip6=(al_addr6_t*)&margs.addr;
            aptr=al_p_addr_addr_ptr(ip6);
        }
        
        if( al_router_find(NULL, aptr, al_addr_af(margs.addr), (uint8_t*)&nexthop, &oif)){
                return -1;
        }
    }
    
    eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    etype = rte_be_to_cpu_16(eth->ether_type);
    dev = &g_vport_ary[oif];
    vport = VLAN2VPORT(dev);
    tcphdr=(struct tcp_hdr*)nextptr;

    if(likely(etype == ETHER_TYPE_IPv4)){
        u->in_type = 0;
        u->proto = ((struct ipv4_hdr*)hdr)->next_proto_id;
        
        if( u->ftype == AL_FLT_SIMPLE_TRANS ){
            addr=(al_addr4_t*)&margs.addr;
            u->remote_addr.address4 = al_p_addr_addr(addr);
            u->remote_port = margs.port;
            rip.address4=u->remote_addr.ipv4;
            if(unlikely(vport_select_local_ip(vport, &u->local_addr,  AF_INET, &rip) != 0)){
                    return -1;
            }
            
            if(unlikely(al_alocate_new_port_with_session(u) != 0)){
                    return -1;
            }
        }else{
            u->remote_addr.ipv4 = ((struct ipv4_hdr*)hdr)->dst_addr;
            u->local_addr.ipv4 = ((struct ipv4_hdr*)hdr)->src_addr;
            u->local_port =  tcphdr->src_port;
            u->remote_port = tcphdr->dst_port;

            if(unlikely(al_port_insert(u) == -1)){
                return -1;
            }
        }

        if(unlikely((portint=arp_search(nexthop.address4, &u->from_haddr, dev)) < 0)){
            LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "File:%s:%d Arp search failed", __FILE__, __LINE__);
            return -1;
        }else{
            u->portid=portint&0xFF;
            u->to_haddr=vport->haddr;
        }
    }else if(etype == ETHER_TYPE_IPv6){
        u->in_type = 1;
        u->proto = ((struct ipv6_hdr*)hdr)->proto;

        if( u->ftype == AL_FLT_SIMPLE_TRANS ){
            addr6=(al_addr6_t*)&margs.addr;
            u->remote_addr.address6 = al_p_addr_addr(addr6);
            u->remote_port = margs.port;
            rip.address6=u->remote_addr.ipv6;
            if(unlikely(vport_select_local_ip(vport, &u->local_addr,  AF_INET6, &rip) != 0)){
                    return -1;
            }
            
            if(unlikely(al_alocate_new_port_with_session(u) != 0)){
                    return -1;
            }
        }else{
            u->remote_addr.address6 = *(uint128_t*)((struct ipv6_hdr*)hdr)->dst_addr;
            u->local_addr.address6 = *(uint128_t*)((struct ipv6_hdr*)hdr)->src_addr;
            u->local_port =  tcphdr->src_port;
            u->remote_port = tcphdr->dst_port;

            if(unlikely(al_port_insert(u) == -1)){
                return -1;
            }
        }

        if(unlikely((portint=ndp_search(nexthop.address6, &u->from_haddr, dev)) < 0)){
            LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "File:%s:%d Arp search failed", __FILE__, __LINE__);
            return -1;
        }else{
            u->portid=portint&0xFF;
            u->to_haddr=vport->haddr;
        }
        
    }else{
        return -1;
    }
    
    return 0;
}

void al_service(al_fargs* f, session_t* s, struct rte_mbuf* buf, void* hdr, void* nextptr){
    
    session_t *c, *u;
    
    if( unlikely(s!=NULL)){
        goto ENDF;
    }else{

        if(f->ftype == AL_FLT_GSLB){
            al_gslb_process(buf, hdr, nextptr);
            return;
        }
        
        /*1. create two session*/
        c = al_session_create();
        if (unlikely(c == NULL)) {
            LOG(LOG_ERROR, LAYOUT_APP, "Create session failed");
            goto ENDF;
        }else{
            c->ftype=f->ftype;
        }
        
        u = al_session_create();
        if (unlikely(u == NULL)) {
            LOG(LOG_ERROR, LAYOUT_APP, "Create session failed");
            al_session_free(c);
            goto ENDF;
        }else{
            u->ftype=f->ftype;
        }

        /* init c session*/
        session_mem_init(c, 0);
        /*init u session*/
        session_mem_init(u, 1);
    }

    if(session_init_local(c, buf, hdr, nextptr)){
        goto END;
    }else{
        u->fid=f->fid;
    }

    if(session_init_remote(c, u, f->fid, f->gid, buf, hdr, nextptr)){
        goto END;
    }else{
        c->fid=f->fid;
    }

    al_session_add(c);
    al_session_add(u);

    c->up=u;
    u->up=c;

    al_process(c, buf, hdr, nextptr);
    return;
    
END:
    al_session_free(c);
    al_session_free(u);
ENDF:
    rte_pktmbuf_free(buf);
    SS_INCR(create_session_failed);
}


#include <stdio.h>
#include <string.h>
#include <rte_memzone.h>
#include "al_log.h"
#include "al_vserver.h"
#include "al_ip.h"
#include "al_group.h"
#include "al_hash.h"
#include "al_common.h"
#include "al_to_api.h"
#include "al_errmsg.h"

static const struct rte_memzone* vserver_pool;
static al_vserver_t* vserver_addr;
static int vserver_counter=0;

int vserver_init(void){
   
    vserver_pool = rte_memzone_reserve_aligned("vserver_pool", AL_VSERVER_MAX_COUNTER*sizeof(al_vserver_t), SOCKET_ID_ANY, RTE_MEMZONE_SIZE_HINT_ONLY, CACHE_ALIGN);
    if( vserver_pool == NULL ){
        fprintf(stderr, "Allocate vserver_pool failed\n");
        return -1;
    }else{
        vserver_addr = (al_vserver_t*)vserver_pool->addr;
        memset(vserver_addr, '\0', AL_VSERVER_MAX_COUNTER*sizeof(al_vserver_t));
    }

    return 0;
}

int al_vserver_add(uint16_t id, uint8_t service_type, uint8_t iptype, uint32_t* start, uint32_t* end, uint16_t port_start, uint16_t port_end, uint16_t pool, const char* domain){

    int cnt, sig;
    al_vserver_t* vs;
    char* dst;

    sig = 0;
    vs = NULL;
    for(cnt=0; cnt<vserver_counter; cnt++){
        if(!IS_VSERVER_USED((vserver_addr+cnt))){
            vs=vserver_addr+cnt;
            vs->vs_id=cnt;
            sig = 1;
            break;
        }
    }

    if( vs == NULL){ 
        if((cnt == vserver_counter) && (cnt == AL_VSERVER_MAX_COUNTER)){
            log_core(LOG_EMERG, LAYOUT_CONF,  "There is no available space");
            return -2;
        }else{
            vs=vserver_addr+vserver_counter;
            vs->vs_id=vserver_counter;
        }
    }

    vs->id=id;
    if(iptype == AF_INET){
        IPV4_COPY(vs->addr_end, end);
        IPV4_COPY(vs->addr_start, start);
        SET_VS_IPV4(vs);
    }else{
        IPV6_COPY(vs->addr_end, end);
        IPV6_COPY(vs->addr_start, start);
    }
    vs->pool=pool;
    vs->port_start=htons(port_start);
    vs->port_end=htons(port_end);
    vs->srv_type=service_type;

    if( sig == 0 ){
        vserver_counter++;
    }

    if( service_type == AL_GSLB_DOMAIN ){
        if( domain == NULL ){
            log_core(LOG_EMERG, LAYOUT_CONF, "Domain is need");
            return -1;
        }else{
            if((dst=al_domain_name(domain, domain + strlen(domain), (char*)VS_DOMAIN_NAME(vs))) != NULL){
                *dst = '\0';
            }else{
                log_core(LOG_EMERG, LAYOUT_CONF, "Domain name invalid %s", domain);
                return -1;
            }
        }
    }else{
        SET_VSERVER(vs);
    }

    SET_VSERVER_USED(vs);
    
    return 0;
}

int al_vserver_del(uint16_t vsid){
    
    int i, rc;
    al_vserver_t* vs;

    rc=-1;
    for( i=0; i<vserver_counter; i++){
        vs = vserver_addr+i;
        if( vs->id == vsid ){
            rc=0;
            SET_VSERVER_UNUSED(vs);
        }
    }

    if( rc == -1 ){
        errno=AL_OBJECT_EEXIST;
    }

    return rc;
}

al_vserver_t* vserver_find(uint32_t* ip, uint16_t port, uint8_t iptype, uint8_t is_exclude_llb){

    int i;
    al_vserver_t* vs;
    
    for( i=0; i<vserver_counter; i++){
        vs = vserver_addr+i;

        if(is_exclude_llb && (vs->srv_type == AL_LLB)){
            continue;
        }

        if( unlikely(((iptype == AF_INET)&&(IPV4_IS_ANY(vs->addr_start))&&(IPV4_IS_ANY(vs->addr_end))) ||
                     ((iptype == AF_INET6)&&(IPV6_IS_ANY(vs->addr_start))&&(IPV6_IS_ANY(vs->addr_end))))){
            if(unlikely(port == 0)){
                continue;
            }
        }
        
        if(likely(IS_VSERVER_USED(vs) && IS_VSERVER(vs))){

#if 0
            LOG(LOG_INFO, LAYOUT_CONF, "start:"IPV4_IP_FORMART" start port:%d end:"IPV4_IP_FORMART" end port:%d %d-%d-%d, %d-%d %u %u %u", IPV4_DATA_READABLE(vs->addr_start), vs->port_start, IPV4_DATA_READABLE(vs->addr_end), vs->port_end, (iptype == AF_INET),  (IS_VS_IPV4(vs)), IPV4_BETWEEN(IPV4(search_ip), IPV4(vss_ip), IPV4(vse_ip)), IPV4(search_ip) >= IPV4(vss_ip), IPV4(search_ip) <= IPV4(vse_ip), IPV4(search_ip), IPV4(vss_ip), IPV4(vse_ip));
#endif

            if((iptype == AF_INET) && (IS_VS_IPV4(vs))){
                if(IPV4_IS_ANY(vs->addr_start) || IPV4_BETWEEN(IPV4(ip), IPV4(vs->addr_start), IPV4(vs->addr_end))){
                    if((vs->port_start == 0) || (port == 0) ||  PORT_BETWEEN(port, vs->port_start, vs->port_end)){
                        return vs;
                    }
                }
            }else if(iptype == AF_INET6){
                if(IPV6_IS_ANY(vs->addr_start)  ||  IPV6_BETWEEN(ip, (uint32_t*)&vs->addr_start, (uint32_t*)&vs->addr_end)){
                    if((vs->port_start == 0) || (port == 0) || PORT_BETWEEN(port, vs->port_start, vs->port_end)){
                        return vs;
                    }
                }
            }
        }
    }

    return NULL;
}

int al_vserver_find(uint32_t* ip, uint16_t port, uint8_t iptype, uint8_t is_exclude_llb, al_vserver_ctx_t* ctx, al_vserver_t* ovs){

    int i;
    al_vserver_t* vs;

    ipaddr_t   search_old_ip;
    ipaddr_t   search_ip;
    ipaddr_t   vss_ip;
    ipaddr_t   vse_ip;
    uint16_t   search_port;
    uint16_t   vss_port;
    uint16_t   vse_port;

    if(likely(ip != NULL)){
        if(iptype == AF_INET){
            IPV4_COPY(&search_old_ip, ip);
        }else{
            IPV6_COPY(&search_old_ip, ip);
        }
	    
        al_addr_ntoh(iptype, search_old_ip, search_ip);
        search_port = ntohs(port);
    }
    
    for( i=0; i<vserver_counter; i++){
        vs = vserver_addr+i;

        if(is_exclude_llb && (vs->srv_type == AL_LLB)){
            continue;
        }

        if( unlikely(ctx&&(ctx->type == AL_GSLB_DOMAIN))){
            if((vs->id == ctx->data.glsb.id)&&( vs->srv_type == AL_GSLB_DOMAIN)){

#if 0
                char buf[256];
                int i, len;
                uint8_t *ptr, *ptr1;
                
                ptr1=ctx->data.glsb.domain.data;
                for(i=0; i< ctx->data.glsb.domain.len; i++){
                    sprintf(&buf[i*2], "%02x", ptr1[i]);
                }
                buf[i*2+1]='\0';
                fprintf(stderr, "Domain 1:%s\n", buf);

                memset(buf, '\0', 256);
                ptr=VS_DOMAIN_NAME(vs);
                len=strlen((char*)ptr);
                for(i=0; i< len; i++){
                    sprintf(&buf[i*2], "%02x", ptr[i]);
                }
                buf[i*2+1]='\0';
                fprintf(stderr, "Domain 2:%s\n", buf);
                
#endif
                if(memcmp(ctx->data.glsb.domain.data, VS_DOMAIN_NAME(vs), ctx->data.glsb.domain.len)==0){
                    *ovs = *vs;
                    return 1;
                }    
            }
            continue;
        }else{
            if(unlikely(vs->srv_type == AL_GSLB_DOMAIN)){
                continue;
            }
        }
        
        if(likely(IS_VSERVER_USED(vs) && IS_VSERVER(vs))){
            
            al_addr_ntoh(iptype, vs->addr_start, vss_ip);
            al_addr_ntoh(iptype, vs->addr_end, vse_ip);
            vss_port=ntohs(vs->port_start);
            vse_port=ntohs(vs->port_end);
#if 0
            LOG(LOG_INFO, LAYOUT_CONF, "start:"IPV4_IP_FORMART" start port:%d end:"IPV4_IP_FORMART" end port:%d %d-%d-%d, %d-%d %u %u %u", IPV4_DATA_READABLE(vs->addr_start), vs->port_start, IPV4_DATA_READABLE(vs->addr_end), vs->port_end, (iptype == AF_INET),  (IS_VS_IPV4(vs)), IPV4_BETWEEN(IPV4(search_ip), IPV4(vss_ip), IPV4(vse_ip)), IPV4(search_ip) >= IPV4(vss_ip), IPV4(search_ip) <= IPV4(vse_ip), IPV4(search_ip), IPV4(vss_ip), IPV4(vse_ip));
#endif

            if((iptype == AF_INET) && (IS_VS_IPV4(vs))){
                if(IPV4_IS_ANY(vs->addr_start) || IPV4_BETWEEN(IPV4(search_ip), IPV4(vss_ip), IPV4(vse_ip))){
                    if((vs->port_start == 0) || (port == 0) ||  PORT_BETWEEN(search_port, vss_port, vse_port)){
                        *ovs = *vs;
						return 1;
                    }
                }
            }else if(iptype == AF_INET6){
                if(IPV6_IS_ANY(vs->addr_start)  ||  IPV6_BETWEEN(search_ip, (uint32_t*)&vss_ip, (uint32_t*)&vse_ip)){
                    if((vs->port_start == 0) || (port == 0) || PORT_BETWEEN(search_port, vss_port, vse_port)){
                        *ovs = *vs;
						return 1;
                    }
                }
            }
        }
    }

    return 0;
}


int vserver_find_rs(uint16_t vsid, ipaddr_t ip, uint8_t* iptype, uint16_t* port){

    al_vserver_t *vs = vserver_addr+vsid;
    al_member_t* mb=NULL;
    if( likely(IS_VSERVER_USED(vs)) ){
        mb = al_pool_find_rs(vs->pool, ip, iptype, port);
    }

    fprintf(stdout, "Find vserver %d member %d\n", vsid, mb == NULL ? -1 : mb->id);

    return (mb != NULL ? 1 : 0);
}

int vserver_is_llb_dr(uint16_t vid){
    al_vserver_t *vs = vserver_addr+vid;
    if( likely(IS_VSERVER_USED(vs)) ){
        return vs->is_llb_dr;
    }else{
        return 0;
    }
}

void al_dump_vserver(FILE* fp){
    int i, s;
    socklen_t sclen=256;
    char buf[256];
    al_vserver_t* vs;

    fprintf(fp, "[\n");
    s=0;
    for( i=0; i<vserver_counter; i++){
        vs = vserver_addr+i;

        if( ! IS_VSERVER_USED(vs)){
            continue;
        }

        if( s == 0 ){
            fprintf(fp, "{\n");
            s=1;
        }else{
            fprintf(fp, ",\n{");
        }

        fprintf(fp, "    \"id\":\"%u\",\n", vs->id);
        fprintf(fp, "    \"type\":\"%s\",\n", al_filter_type_descr(vs->srv_type));
        if( vs->srv_type == AL_LLB){
            if(vserver_is_llb_dr(vs->id)){
                fprintf(fp, "    \"is_dr\":\"%s\",\n","on");
            }else{
                fprintf(fp, "    \"is_dr\":\"%s\",\n","off");
            }
        }else if(vs->srv_type==AL_GSLB_DOMAIN){
            fprintf(fp, "    \"domain\":\"%s\",\n", (char*)VS_DOMAIN_NAME(vs)); 
        }
        fprintf(fp, "    \"group\":\"%u\",\n", vs->pool);
        fprintf(fp, "    \"port_start\":\"%u\",\n", ntohs(vs->port_start));
        fprintf(fp, "    \"port_end\":\"%u\",\n", ntohs(vs->port_end));
        fprintf(fp, "    \"addr_start\":\"%s\",\n", inet_ntop(IS_VS_IPV4(vs) ? AF_INET : AF_INET6, &vs->addr_start, buf, sclen));
        fprintf(fp, "    \"addr_end\":\"%s\",\n", inet_ntop(IS_VS_IPV4(vs) ? AF_INET : AF_INET6, &vs->addr_end, buf, sclen));
        
        fprintf(fp, "}");
    }
    fprintf(fp, "\n]\n");
}


static char* al_filter_type_table[20]={
    "llb",
    "slb",
    "tcp_proxy",
    "http_proxy",
    "gslb",
    "gslb_domain"
};


const char* al_filter_type_descr(uint8_t type){
      if( type < AL_PROC_END ){
          return al_filter_type_table[type];
      }else{
          return NULL;
      }
}

const int8_t al_filter_type(const char* type){
    int i;
    for(i=0; i<AL_PROC_END; i++){
        if( strcmp(al_filter_type_table[i], type) == 0 ){
            return i;
        }
    }

    return -1;
}


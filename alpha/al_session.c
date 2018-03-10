#include <rte_mempool.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include "alpha.h"
#include "al_common.h"
#include "al_hash.h"
#include "al_session.h"
#include "al_tuple5.h"
#include "statistics/statistics.h"
#include "al_ip.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_arp.h"

static __thread struct hash_table_nolock* session_table;

static struct rte_mempool* session_memory_pool=NULL;
static uint32_t            session_mask;

int session_init(void){

    session_memory_pool = rte_mempool_create("session_memory_pool", MAX_SESSION_COUNTER, sizeof(struct session), CACHE_ALIGN, 0, NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
    if ( session_memory_pool == NULL ){
        fprintf(stderr, "Allocate session_memory_pool failed");
        return -1;
    }

    session_mask = HASHSIZE(MAX_SESSION_COUNTER)-1;
	
    return 0;
}

int session_thread_init(void){
    
    int i;

    session_table = (struct hash_table_nolock*)rte_malloc(NULL, HASHSIZE(MAX_SESSION_COUNTER)*sizeof(struct hash_table_nolock), CACHE_ALIGN);
    if( session_table == NULL ){
       return -1;
    }
   
    //bound hash 
    for(i=0; i<HASHSIZE(MAX_SESSION_COUNTER); i++){
        INIT_HLIST_HEAD(&session_table[i].list);
    }

    return 0;
}

session_t* al_session_create(void){

    session_t* s;
    if(unlikely(rte_mempool_get(session_memory_pool, (void**)&s) != 0)){
        return NULL;
    }else{
        s->port_alloc=0;
        s->proto=0;
        s->is_dr=0;
        s->stout=SESSION_IDLE_TIMEOUT;
        return s;
    }
}

void al_session_free(session_t* s){

    int rc= s->in_type == 0 ? al_port_del_v4(s->remote_addr.address4, s->local_addr.address4, s->remote_port, s->local_port, s->proto) : al_port_del_v6(s->remote_addr.address6, s->local_addr.address6, s->remote_port, s->local_port, s->proto);
    if (unlikely(rc==1)){
        LOG(LOG_ERROR, LAYOUT_APP, "Bug, port cann't del");
    }
   rte_mempool_put(session_memory_pool, s);
}

session_t* tcp6_session_get(uint128_t rip, uint16_t rport, uint128_t lip, uint16_t lport, uint8_t proto_type){
          
    session_t*      pos;
    uint32_t key = pcb6_key(rip, rport, lip, lport,proto_type, session_mask);
    struct hash_table_nolock* table=&session_table[key & session_mask];

    hlist_for_each_entry(pos, &table->list, list){
        if( (pos->local_port == lport) && (pos->remote_port == rport) && (pos->local_addr.address6 == lip) && (pos->remote_addr.address6 == rip) && (pos->proto == proto_type)){
            return  pos;
        }
    }

    return NULL;
}

session_t* tcp4_session_get(uint32_t rip, uint16_t rport, uint32_t lip, uint16_t lport, uint8_t proto_type){
          
    session_t*      pos;
    uint32_t key = PCB_KEY(rip, rport, lip, lport,proto_type, session_mask);
    struct hash_table_nolock* table=&session_table[key & session_mask];

    hlist_for_each_entry(pos, &table->list, list){
        if( (pos->local_port == lport) && (pos->remote_port == rport) && (pos->local_addr.address4 == lip) && (pos->remote_addr.address4 == rip) && (pos->proto == proto_type)){
            return  pos;
        }
    }

    return NULL;
}


void al_session_add(session_t* s){

    uint32_t key;
    struct hash_table_nolock* table;

    if(s->in_type == 0){
        key = PCB_KEY(s->remote_addr.address4, s->remote_port, s->local_addr.address4, s->local_port, s->proto, session_mask);
    }else{
        key = pcb6_key(s->remote_addr.address6, s->remote_port, s->local_addr.address6, s->local_port, s->proto, session_mask);
    }
	
    table = &session_table[key & session_mask];
    
    hlist_add_head(&s->list, &table->list);
}

void al_output_session(void ){
    int i;
    for(i=0; i<g_work_thread_count; i++){
        g_thread_info[i].oss=1; 
    }
}

void al_output_thread_session(void){
    
    session_t *s,  *s1;
    struct hlist_node *t;
    int i;
    char buf[128];

#define MAX_IP_BUF 128

    char laddr[MAX_IP_BUF], raddr[MAX_IP_BUF];
    char laddr1[MAX_IP_BUF], raddr1[MAX_IP_BUF];    

    sprintf(buf, "/tmp/%u.txt", g_lcoreid);

    FILE* fp=fopen(buf, "w");
    if(fp == NULL){
        LOG(LOG_ERROR, LAYOUT_APP, "Open file %s failed, %s", buf, strerror(errno));
        return;
    }
    
    for( i=0; i<HASHSIZE(MAX_SESSION_COUNTER); i++){
        hlist_for_each_entry_safe(s, t, &session_table[i].list, list){
            if( s->is_ups ){

                if( s->in_type == 0 ) {
                    inet_ntop(AF_INET, &s->local_addr, laddr, MAX_IP_BUF);
                    inet_ntop(AF_INET, &s->remote_addr, raddr, MAX_IP_BUF);
                }else{
                    inet_ntop(AF_INET6, &s->local_addr, laddr, MAX_IP_BUF);
                    inet_ntop(AF_INET6, &s->remote_addr, raddr, MAX_IP_BUF);
                }
                
                s1=s->up;
                if( s1 ){
                    if( s1->in_type == 0 ) {
                        inet_ntop(AF_INET, &s1->local_addr, laddr1, MAX_IP_BUF);
                        inet_ntop(AF_INET, &s1->remote_addr, raddr1, MAX_IP_BUF);
                    }else{
                        inet_ntop(AF_INET6, &s1->local_addr, laddr1, MAX_IP_BUF);
                        inet_ntop(AF_INET6, &s1->remote_addr, raddr1, MAX_IP_BUF);
                    }
                    fprintf(fp, "From "ETHER_ADDR_FORMART" %s(%u) to "ETHER_ADDR_FORMART"  %s(%u) From "ETHER_ADDR_FORMART" %s(%u) to "ETHER_ADDR_FORMART" %s(%u)  ttl:%d\n", ETHER_ADDR(&s1->from_haddr), raddr1, rte_be_to_cpu_16(s1->remote_port), ETHER_ADDR(&s1->to_haddr), laddr1, rte_be_to_cpu_16(s1->local_port), ETHER_ADDR(&s->from_haddr), raddr, rte_be_to_cpu_16(s->remote_port), ETHER_ADDR(&s->to_haddr), laddr, rte_be_to_cpu_16(s->local_port), s->stout);
                }else{
                    fprintf(fp, "%s "ETHER_ADDR_FORMART" dst:%s(%u) src:%s(%u) "ETHER_ADDR_FORMART" ttl:%d\n", s->in_type == 0 ? "ipv4":"ipv6", ETHER_ADDR(&s->from_haddr), laddr, rte_be_to_cpu_16(s->local_port), raddr, rte_be_to_cpu_16(s->remote_port), ETHER_ADDR(&s->to_haddr), s->stout);
                }
            }
        }
    }

    fclose(fp);
    g_thread_info[g_lcoreid].oss = 0;
}

void al_session_timeout(void)
{

    session_t *s, *u;
    struct hlist_node *t;
    int ss, su, i;

    for( i=0; i<HASHSIZE(MAX_SESSION_COUNTER); i++){
        hlist_for_each_entry_safe(s, t, &session_table[i].list, list){
            u=s->up;
            ss=s->stout> 0 ? --s->stout : 0;
            su=u->stout> 0 ? --u->stout : 0;

            if( (ss==0) && (su==0)){
                hlist_del(&s->list);
                al_session_free(s);
                hlist_del(&u->list);
                al_session_free(u);
            }
	}
    }

}


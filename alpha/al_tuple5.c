#include <stdio.h>
#include <inttypes.h> 
#include <rte_mempool.h>
#include <rte_ether.h>
#include "alpha.h"
#include "al_common.h"
#include "al_session.h"
#include "al_log.h"
#include "al_hash.h"
#include "al_radix.h"
#include "al_tuple5.h"
#include "al_mempool.h"
#include "al_filter.h"

#define TCP_LOCAL_PORT_RANGE_START        0x0001
#define TCP_LOCAL_PORT_RANGE_END           0xea60
#define TCP_ENSURE_LOCAL_PORT_RANGE(port) (((port) & ~TCP_LOCAL_PORT_RANGE_START) + TCP_LOCAL_PORT_RANGE_START)

#define MAX_PORT_COUNTER    MAX_SESSION_COUNTER

static struct hash_table_lock port_table_v4[HASHSIZE(MAX_PORT_COUNTER)];
static struct hash_table_lock port_table_v6[HASHSIZE(MAX_PORT_COUNTER)];
static uint32_t port_mask;
static uint16_t port_start;

static struct rte_mempool* port_mem_pool=NULL;

static rte_spinlock_t g_port_alloc_lock_v4;
static rte_spinlock_t g_port_alloc_lock_v6;

int port_init(void){
    int i;
    port_mask=HASHSIZE(MAX_PORT_COUNTER)-1;   
    for(i=0; i<HASHSIZE(MAX_PORT_COUNTER); i++){
        rte_rwlock_init(&port_table_v4[i].lock);
        INIT_HLIST_HEAD(&port_table_v4[i].list);
    }
    rte_spinlock_init(&g_port_alloc_lock_v4);

    for(i=0; i<HASHSIZE(MAX_PORT_COUNTER); i++){
        rte_rwlock_init(&port_table_v6[i].lock);
        INIT_HLIST_HEAD(&port_table_v6[i].list);
    }
    rte_spinlock_init(&g_port_alloc_lock_v6);

    port_mem_pool = rte_mempool_create("port_mem_pool", MAX_SESSION_COUNTER, sizeof(port_t), CACHE_ALIGN, 0, NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
    if ( port_mem_pool == NULL ){
        fprintf(stderr, "Allocate session_memory_pool failed");
        return -1;
    }

    srand(time(NULL));
    port_start = TCP_ENSURE_LOCAL_PORT_RANGE(((uint16_t)rand()));
    return 0;
}

static port_t* port_create(void){

    port_t* s;

    rte_mempool_get(port_mem_pool, (void**)&s);
    if(unlikely(s==NULL)){
        errno = ENOMEM;
        return NULL;
    }else{
        return s;
    }
}

int al_port_insert(session_t* u){
    if( u->in_type == 0 ){
        if(unlikely(al_port_insert_v4(u->remote_addr.address4, u->local_addr.address4, u->remote_port, u->local_port, u->proto) == -1)){
            return -1;
        }
    }else{
        if(unlikely(al_port_insert_v6(u->remote_addr.address6, u->local_addr.address6, u->remote_port, u->local_port, u->proto) == -1)){
            return -1;
        }
    }
    return 0;
}

int al_port_del(session_t* u){
    if( u->in_type == 0 ){
        if(unlikely(al_port_del_v4(u->remote_addr.address4, u->local_addr.address4, u->remote_port, u->local_port, u->proto) == -1)){
            return -1;
        }
    }else{
        if(unlikely(al_port_insert_v6(u->remote_addr.address6, u->local_addr.address6, u->remote_port, u->local_port, u->proto) == -1)){
            return -1;
        }
    }
    return 0;
}

int al_port_insert_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){

    uint32_t key; 
    struct hash_table_lock* table;
    port_t* tp=port_create();
    if( tp == NULL ){
        return -1;
    }else{
        tp->rip.ipv4 = rip;
        tp->lip.ipv4 = lip;
	
        tp->rport=rport;
        tp->lport=lport;
        tp->proto_type = proto_type;
        tp->thread_id = g_lcoreid;
    }

    key=connect_session_key_v4(rip, rport, lip, lport, proto_type, port_mask);
    table = &port_table_v4[key];
    rte_rwlock_write_lock(&table->lock);
    hlist_add_head(&tp->list, &table->list);
    rte_rwlock_write_unlock(&table->lock);

#if 0
    LOG(LOG_TRACE, LAYOUT_APP, "Insert port lcoreid :%d "IPV4_IP_FORMART" port:%d to  ip "IPV4_IP_FORMART" port:%d proto:%d, key:%u", 
        g_lcoreid,IPV4_DATA_READABLE(rip), rport,
        IPV4_DATA_READABLE(lip), lport, proto_type, key);
#endif

    return 0;

}

int al_port_insert_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){

    uint32_t key; 
    struct hash_table_lock* table;
    port_t* tp=port_create();
    if( tp == NULL ){
        return -1;
    }else{
        tp->rip.ipv6 = rip;
        tp->lip.ipv6 = lip;
	
        tp->rport=rport;
        tp->lport=lport;
        tp->proto_type = proto_type;
        tp->thread_id = g_lcoreid;
    }

    key=connect_session_key_v6(rip, rport, lip, lport, proto_type, port_mask);
    table = &port_table_v6[key];
    rte_rwlock_write_lock(&table->lock);
    hlist_add_head(&tp->list, &table->list);
    rte_rwlock_write_unlock(&table->lock);

#if 0
    LOG(LOG_TRACE, LAYOUT_APP, "Insert port lcoreid :%d "IPV4_IP_FORMART" port:%d to  ip "IPV4_IP_FORMART" port:%d proto:%d, key:%u", 
        g_lcoreid,IPV4_DATA_READABLE(rip), rport,
        IPV4_DATA_READABLE(lip), lport, proto_type, key);
#endif
	
    return 0;

}

int al_port_del_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){

    uint32_t key=connect_session_key_v4(rip, rport, lip,  lport, proto_type, port_mask); 
    struct hash_table_lock* table = &port_table_v4[key]; 
    port_t* pos, *pos1=NULL;

    rte_rwlock_write_lock(&table->lock);

    hlist_for_each_entry(pos, &table->list, list){
        if((pos->proto_type == proto_type) &&(pos->rip.ipv4==rip) && (pos->lip.ipv4 == lip) && (pos->rport == rport) && 
            (pos->lport == lport) ){
            pos1=pos;
            hlist_del(&pos1->list);
            break;
        }
    } 
    rte_rwlock_write_unlock(&table->lock);
    
    if( pos1 != NULL ){
        rte_mempool_put(port_mem_pool, pos1);
        return 1;
    }else{
        return 0;
    }
}

int al_port_del_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){

    uint32_t key=connect_session_key_v6(rip, rport, lip,  lport, proto_type, port_mask); 
    struct hash_table_lock* table = &port_table_v6[key]; 
    port_t* pos, *pos1=NULL;

    rte_rwlock_write_lock(&table->lock);

    hlist_for_each_entry(pos, &table->list, list){
        if( (pos->proto_type == proto_type) &&(pos->rip.ipv6 == rip) && (pos->lip.ipv6 == lip) && (pos->rport == rport) && 
            (pos->lport == lport)){
            pos1=pos;
            hlist_del(&pos1->list);
            break;
        } 
    } 
    rte_rwlock_write_unlock(&table->lock);
    
    if( pos1 != NULL){
        rte_mempool_put(port_mem_pool, pos1);
        return 1;
    }else{
        return 0;
    }

}

int which_thread_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){
	
    uint32_t key=connect_session_key_v4(rip, rport, lip, lport, proto_type, port_mask); 
    struct hash_table_lock* table = &port_table_v4[key]; 
    port_t* pos;
    int rc=-1;

    rte_rwlock_read_lock(&table->lock);

    hlist_for_each_entry(pos, &table->list, list){
        if( (pos->rip.ipv4 == rip) && (pos->lip.ipv4 == lip) && (pos->rport == rport) && 
            (pos->lport == lport) && (pos->proto_type == proto_type)){
            rc=pos->thread_id;
            break;
        } 
    }

    rte_rwlock_read_unlock(&table->lock);

#if 0
    LOG(LOG_TRACE, LAYOUT_APP, "Search port lcoreid :%d "IPV4_IP_FORMART" port:%d to  ip "IPV4_IP_FORMART" port:%d proto:%d, key:%u, rc:%d", 
        g_lcoreid, IPV4_DATA_READABLE(&rip), rport,
        IPV4_DATA_READABLE(&lip), lport, proto_type, key, rc);
#endif

    return rc; 
}

int which_thread_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){
	
    uint32_t key=connect_session_key_v6(rip, rport, lip, lport, proto_type, port_mask); 
    struct hash_table_lock* table = &port_table_v6[key]; 
    port_t* pos;
    int rc=-1;

    rte_rwlock_read_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if(  (pos->rip.ipv6== rip) && (pos->lip.ipv6 == lip) && (pos->rport == rport) && 
            (pos->lport == lport) && (pos->proto_type == proto_type)){
            rc=pos->thread_id;
            break;
        } 
    }
    rte_rwlock_read_unlock(&table->lock);
    return rc; 
}

int is_connect_port_already_used_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){

    uint32_t key=connect_session_key_v4(rip, rport, lip, lport, proto_type, port_mask);
    struct hash_table_lock* table = &port_table_v4[key]; 
    port_t* pos, *pos1=NULL;

    rte_rwlock_read_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if( pos->lport == lport ){
            if( ((pos->rip.ipv4 == rip) || (rip==0)) && 
                ((pos->lip.ipv4 ==lip) || (lip==0)) && (pos->rport == rport) && ( pos->proto_type == proto_type)){
                pos1=pos; 
                break;
            } 
        }
    } 
    rte_rwlock_read_unlock(&table->lock);

    return pos1 == NULL ? 0 : 1;

}

int is_connect_port_already_used_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type){

    uint32_t key=connect_session_key_v6(rip, rport, lip, lport, proto_type, port_mask);
    struct hash_table_lock* table = &port_table_v6[key]; 
    port_t* pos, *pos1=NULL;

    rte_rwlock_read_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if( pos->lport == lport ){
            if( ((pos->rip.ipv6 == rip) || (rip==0)) && 
                ((pos->lip.ipv6 == lip) || (lip==0)) && (pos->rport == rport) && ( pos->proto_type == proto_type)){
                pos1=pos; 
                break;
            } 
        }
    } 
    rte_rwlock_read_unlock(&table->lock);

    return pos1 == NULL ? 0 : 1;

}

int al_new_port_with_session(session_t* s)
{

    int rc=0;
    uint16_t lport=s->local_port;
    uint8_t iptype=s->in_type == 0 ? AF_INET : AF_INET6;

    if( iptype == AF_INET ){

        /* Check all PCB lists. */
        if( ! al_filter4_search( s->local_addr.address4, lport,  s->proto, NULL)  ){
            rc=-1;
            goto end;
        }
    
        rte_spinlock_lock(&g_port_alloc_lock_v4);
        if( is_connect_port_already_used_v4(s->remote_addr.ipv4, s->local_addr.ipv4, s->remote_port, lport, s->proto) == 1 ){
            rc = -1;
            rte_spinlock_unlock(&g_port_alloc_lock_v4);
            goto end;
        }

        if(unlikely(al_port_insert_v4(s->remote_addr.ipv4, s->local_addr.ipv4, s->remote_port, lport, s->proto) == -1)){
            rc = -1;
            rte_spinlock_unlock(&g_port_alloc_lock_v4);
            goto end;
        }else{
            s->port_alloc=1;
            rc = 0;
            rte_spinlock_unlock(&g_port_alloc_lock_v4);
        }
    }else{

        /* Check all PCB lists. */
        if( ! al_filter6_search( s->local_addr.address6, lport, s->proto, NULL)  ){
            rc=-1;
            goto end;
        }
        
        rte_spinlock_lock(&g_port_alloc_lock_v6);
        if( is_connect_port_already_used_v6(s->remote_addr.ipv4, s->local_addr.ipv4, s->remote_port, lport, s->proto) == 1 ){
            rc = -1;
            rte_spinlock_unlock(&g_port_alloc_lock_v6);
            goto end;
        }

        if(unlikely(al_port_insert_v6(s->remote_addr.ipv4, s->local_addr.ipv4, s->remote_port, lport, s->proto) == -1)){
            rc = -1;
            rte_spinlock_unlock(&g_port_alloc_lock_v6);
            goto end;
        }else{
            s->port_alloc=1;
            rc = 0;
            rte_spinlock_unlock(&g_port_alloc_lock_v6);
        }        
    }

end:
    return rc;
}

int al_alocate_new_port_with_session(session_t* s)
{

    int rc=-1, is_port_used;
    uint16_t n;
    uint16_t lport;
    rte_spinlock_t* lock;
    uint8_t iptype;

    iptype=s->in_type ? AF_INET6 : AF_INET;

    lock=(iptype==AF_INET6) ? &g_port_alloc_lock_v6 : &g_port_alloc_lock_v4;
    rte_spinlock_lock(lock);

    n=0;
again:

    is_port_used=0;
    
    if (port_start++ == TCP_LOCAL_PORT_RANGE_END) {
        port_start = TCP_LOCAL_PORT_RANGE_START;
    }

    lport = rte_cpu_to_be_16(port_start);
    if(iptype==AF_INET){
        /* Check all PCB lists. */
        if( ! al_filter4_search( s->local_addr.ipv4, lport,  s->proto, NULL)  ){
            if (++n <= (TCP_LOCAL_PORT_RANGE_END - TCP_LOCAL_PORT_RANGE_START)) {
                goto again;
            }else{
                lport=0;
                goto end;
            }
        }else{
            is_port_used=is_connect_port_already_used_v4( s->remote_addr.address4, s->local_addr.address4, s->remote_port, lport, s->proto);
        }
    }else{
        /* Check all PCB lists. */
        if( ! al_filter4_search( s->local_addr.ipv6, lport,  s->proto, NULL)  ){
            if (++n <= (TCP_LOCAL_PORT_RANGE_END - TCP_LOCAL_PORT_RANGE_START)) {
                goto again;
            }else{
                lport=0;
                goto end;
            }
        }else{
            is_port_used=is_connect_port_already_used_v6( s->remote_addr.address6, s->local_addr.address6, s->remote_port, lport, s->proto);
        }
    }

    if( is_port_used == 1 ){
        if (++n <= (TCP_LOCAL_PORT_RANGE_END - TCP_LOCAL_PORT_RANGE_START)) {
            goto again; 
        }else{
            lport=0;
            goto end;
        }
    }

    s->local_port=lport;
    if(unlikely(al_port_insert(s) == -1)){
        lport=0;
    }else{
        rc=0;
    }

end:
    rte_spinlock_unlock(lock);
    return rc;
}


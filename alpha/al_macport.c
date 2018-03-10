#include <rte_ether.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_rwlock.h>
#include <rte_errno.h>
#include <time.h>
#include <inttypes.h>
#include "al_common.h"
#include "hlist.h"
#include "al_hash.h"
#include "al_macport.h"
#include "al_log.h"
#include "al_to_api.h"

#define MAX_MAC_UINTE    4096

static struct rte_mempool* al_macport_pool=NULL;
static struct hash_table_lock   al_macport_hash[HASHSIZE(MAX_MAC_UINTE)];
static uint32_t            al_macport_mask;


static int g_mac_live=60*5;

static inline uint32_t mac_hash(uint8_t* mac);

int init_macport(void){

    int i;

    al_macport_pool = rte_mempool_create("al_macport_pool", MAX_MAC_UINTE, sizeof(struct mac_port), CACHE_ALIGN, 0, NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
    if ( al_macport_pool == NULL){
        LOG(LOG_FETAL, LAYOUT_APP, "Allocate al_macport_pool failed, msg:%s", rte_strerror(rte_errno));
        return -1;
    }

    al_macport_mask = HASHSIZE(MAX_MAC_UINTE)-1;

    for(i=0; i<HASHSIZE(MAX_MAC_UINTE); i++){
        rte_rwlock_init(&al_macport_hash[i].lock);
        INIT_HLIST_HEAD(&al_macport_hash[i].list);
    }
	
    return 0;
}

uint8_t portid_lookup(struct ether_addr* mac){
    uint32_t hash=mac_hash(mac->addr_bytes);
	
    struct hash_table_lock *table;
    struct mac_port* pos;
    uint8_t          portid=0xFF;

    rte_rwlock_read_lock(&al_macport_hash[hash].lock);

    table=&al_macport_hash[hash];
    hlist_for_each_entry(pos, &table->list, list){
        if( is_same_ether_addr(&pos->addr, mac) ){
            portid=pos->port;
        }
    }
	
    rte_rwlock_read_unlock(&al_macport_hash[hash].lock);

    return portid;
}


void portid_update(struct ether_addr* mac, uint8_t portid){
    uint32_t hash=mac_hash(mac->addr_bytes);
	
    struct hash_table_lock *table;
    struct mac_port* pos, *obj;
    uint8_t  port=0xFF;

    rte_rwlock_write_lock(&al_macport_hash[hash].lock);

    table=&al_macport_hash[hash];
    hlist_for_each_entry(pos, &table->list, list){
        if( is_same_ether_addr(&pos->addr, mac) ){
            port=portid;
            pos->time=g_sys_time+g_mac_live;
            break;
        }
    }

    if( unlikely(port == 0xFF) ){
        if(unlikely(rte_mempool_get(al_macport_pool, (void**)&obj) != 0 )){
            LOG(LOG_EMERG, LAYOUT_SWITCH, "File:%s Line:%d Memory not enought", __FILE__, __LINE__);
        }else{
            ((struct mac_port*)obj)->addr=*mac;
            obj->port=portid;
            obj->time=g_sys_time+g_mac_live;
            hlist_add_head(&obj->list, &table->list);
        }
    }

    rte_rwlock_write_unlock(&al_macport_hash[hash].lock);
}


void portid_clear(void){

    struct hash_table_lock *table;
    struct mac_port* pos;
    struct hlist_node* obj;
    uint32_t      hash;

    static time_t  last=0;
    if( g_sys_time - last >= 5 ){
        last = g_sys_time;
    }else{
        return;
    }

    for(hash=0; hash < HASHSIZE(MAX_MAC_UINTE); hash++){ 
        rte_rwlock_write_lock(&al_macport_hash[hash].lock);

        table=&al_macport_hash[hash];
        hlist_for_each_entry_safe(pos, obj, &table->list, list){

            if(pos->time - g_sys_time < 0){
                hlist_del(&pos->list);
                rte_mempool_put(al_macport_pool, pos);
            }
        }
		
        rte_rwlock_write_unlock(&al_macport_hash[hash].lock);
    }
	
}

static inline uint32_t mac_hash(uint8_t* mac){
    uint16_t* p16=(uint16_t*)mac;
    uint16_t* p32=(uint16_t*)(mac+2);
    uint16_t* p48=(uint16_t*)(mac+4);

    return (*p16 ^ *p32 ^ *p48) & al_macport_mask;
}

void al_output_mac(FILE* fp){
    
    struct hash_table_lock *table;
    struct mac_port* pos;
    struct hlist_node* obj;
    uint32_t      hash;

    LOG(LOG_TRACE, LAYOUT_DATALINK, "mac output");

    fprintf(fp, "%5s  %s\n", "port", "MAC");
    for(hash=0; hash < HASHSIZE(MAX_MAC_UINTE); hash++){ 

        table=&al_macport_hash[hash];
        rte_rwlock_read_lock(&table->lock);
        hlist_for_each_entry_safe(pos, obj, &table->list, list){
            fprintf(fp,"%5d  "ETHER_ADDR_FORMART"\n", pos->port, ETHER_ADDR(&pos->addr));
        }
        rte_rwlock_read_unlock(&table->lock);
    }
}

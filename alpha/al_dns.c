#include <stdio.h>
#include <string.h>
#include <rte_memzone.h>
#include <rte_rwlock.h>
#include <rte_mempool.h>
#include "al_common.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_tree.h"
#include "al_errmsg.h"
#include "common/al_buf.h"
#include "al_to_api.h"
#include "hlist.h"
#include "al_mempool.h"
#include "al_member.h"
#include "al_dns.h"
#include "al_radix_tree.h"
#include "al_group.h"

static al_domain_tree g_dns;

#define g_dns_ptr &g_dns

static rte_rwlock_t g_dns_lock;

int al_dns_init(void){
    memset(&g_dns, '\0', sizeof(g_dns));
    rte_rwlock_init(&g_dns_lock);
    return 0;
}

int al_dns_insert(uint8_t* d, int dlen, uint32_t gid, uint32_t dnsid){
    al_dns_record* rptr;
    int rc;

    rc = -1;
    rptr=al_calloc(sizeof(al_dns_record));
    if( rptr == NULL ){
        return -1;
    }else{
        rptr->gid=gid;
        rptr->dnsid=dnsid;
    }
    
    rte_rwlock_write_lock(&g_dns_lock);
    //bug insert failed need process
    art_insert(g_dns_ptr, d, dlen, rptr);
    rte_rwlock_write_unlock(&g_dns_lock);

    return rc;
}

int al_dns_lookup(al_buf_t* dstr, al_dns_record** g){

    int rc=-1;
    al_dns_record* ptr;

    rte_rwlock_read_lock(&g_dns_lock);
    ptr=al_domain_search(g_dns_ptr, dstr->pos, al_buf_len(dstr));
    if( ptr ){
        rc=0;
        *g=ptr;
    }
    rte_rwlock_read_unlock(&g_dns_lock);
    
    return rc;
}

int al_dns_delete(uint8_t* d, int dlen){
    al_dns_record* ptr;
    int rc;
    
    rc=-1;
    rte_rwlock_write_lock(&g_dns_lock);
    ptr=art_delete(g_dns_ptr, d, dlen);
    if(!ptr){
        errno=AL_ENOOBJ;
        rc = -1;
    }else{
        //delete  group
        if(al_group_delete(ptr->dnsid, ptr->gid)){
            LOG(LOG_ERROR, LAYOUT_CONF, "Dns %u group %d delete failed", ptr->dnsid, ptr->gid);
        }
        al_free(ptr);    
    }
    rte_rwlock_write_unlock(&g_dns_lock);
    return rc;
}

int al_dns_dump(FILE* fp){
    return 0;    
}

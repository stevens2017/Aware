#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rte_rwlock.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include "alpha.h"
#include "al_common.h"
#include "al_hash.h"
#include "hlist.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_geoip.h"
#include "al_radix.h"
#include "al_mempool.h"
#include "al_dev.h"

enum rn_type{
    AL_RN_INET,
    AL_RN_INET6,
    AL_RN_EN
};

static struct radix_node_head* al_geoip[AL_RN_EN];
static int al_geoip_address_off[AL_RN_EN]={32,128};

#define al_geoip_table(rn_t) (rn_t == AF_INET ? &al_geoip[AL_RN_INET] : ( rn_t==AF_INET6 ? &al_geoip[AL_RN_INET6] : NULL))

#define al_geoi_ptr(rn_t) al_geoip_table(rn_t)

static int al_geoip_dump_info_v6(struct radix_node *rn, void *arg);
static int al_geoip_dump_info_v4(struct radix_node *rn, void *arg);
static void al_geoip_dump_table_v6(struct radix_node_head* rh, FILE* fp);
static void al_geoip_dump_table_v4(struct radix_node_head* rh, FILE* fp);

int al_geoip_init(void){

    struct radix_node_head** ptr=al_geoip_table(AF_INET);
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_geoip_address_off[AL_RN_INET]) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    ptr=al_geoip_table(AF_INET6);
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_geoip_address_off[AL_RN_INET6]) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    return 0;
}

int al_geoip_insert(uint32_t id, uint8_t* addr, uint8_t* prefix, uint8_t iptype){
    al_geoip_t* r4;
    al_geoip6_t* r6;
    struct radix_node_head* ptr;
    int rc;
    
#if 0
    LOG(LOG_INFO, LAYOUT_CONF, "Add geoip vlanid:%d geoip type:%d", vlanid, rtype);
#endif

    
    if( iptype == AF_INET ){
        r4=al_calloc(sizeof(al_geoip_t));
        if( r4 == NULL ){
            rc = -1;
            goto failed;
        }

        al_addr_len(r4->addr)=sizeof(al_addr4_t);
        al_addr_addr(r4->addr)=*(uint32_t*)addr;
        al_addr_af(r4->addr)=AF_INET;
        al_addr_len(r4->prefix)=sizeof(al_addr4_t);
        al_addr_addr(r4->prefix)=*(uint32_t*)prefix;
        al_addr_addr(r4->addr)&=al_addr_addr(r4->prefix);
        al_addr_af(r4->prefix)=AF_INET;
        
        ptr=*al_geoip_table(AF_INET);
        rte_rwlock_write_lock(&ptr->lock);
        if(!ptr->rnh_addaddr(&r4->addr, &r4->prefix, &ptr->rh, r4->nodes)){
            rte_rwlock_write_unlock(&ptr->lock);
            rc = -1;
            al_free(r4);
            goto failed;
        }else{
            rte_rwlock_write_unlock(&ptr->lock);
            r4->id=id;
        }
    }else if( iptype == AF_INET6 ){
        r6=al_calloc(sizeof(al_geoip6_t));
        if( r6 == NULL ){
            rc = -1;
            goto failed;
        }
        
        al_addr_len(r6->addr)=sizeof(al_addr6_t);
        al_addr_addr(r6->addr)=*(uint128_t*)addr;
        al_addr_af(r6->addr)=AF_INET6;
        al_addr_len(r6->prefix)=sizeof(al_addr6_t);
        al_addr_addr(r6->prefix)=*(uint128_t*)prefix;
        al_addr_af(r6->prefix)=AF_INET6;
        al_addr_addr(r6->addr)&=al_addr_addr(r6->prefix);        
        
        ptr=*al_geoip_table(AF_INET6);
        rte_rwlock_write_lock(&ptr->lock);
        if(!ptr->rnh_addaddr(&r6->addr, &r6->prefix, &ptr->rh, r6->nodes)){
            rte_rwlock_write_unlock(&ptr->lock);
            rc = -1;
            al_free(r6);            
            goto failed;
        }else{
            rte_rwlock_write_unlock(&ptr->lock);
            r6->id=id;
        }
    }else{
        rc = -1;
        goto failed;
    }

    rc=0;
failed:
    return rc;
}

int al_geoip_delete(uint8_t* subnet, uint8_t* prefix, uint8_t iptype){

    struct radix_node* rd;
    struct radix_node_head* ptr=*al_geoip_table(iptype);
    int rc;
    al_addr4_t addr4;
    al_addr4_t maddr4;
    al_addr6_t addr6;
    al_addr6_t maddr6;
    al_addr_t  *addr, *maddr;
	
    if( iptype == AF_INET ){
        al_addr_len(addr4)=sizeof(al_addr4_t);
        al_addr_addr(addr4)=*(uint32_t*)subnet;
        al_addr_af(addr4)=AF_INET;
        addr=(al_addr_t*)&addr4;
        al_addr_len(maddr4)=sizeof(al_addr4_t);
        al_addr_addr(maddr4)=*(uint32_t*)prefix;
        al_addr_af(maddr4)=AF_INET;
        al_addr_addr(addr4)&=al_addr_addr(maddr4);        
        maddr=(al_addr_t*)&maddr4;
    }else{
        al_addr_len(addr6)=sizeof(al_addr6_t);
        al_addr_addr(addr6)=*(uint128_t*)subnet;
        al_addr_af(addr6)=AF_INET6;
        addr=(al_addr_t*)&addr6;
        al_addr_len(maddr6)=sizeof(al_addr6_t);
        al_addr_addr(maddr6)=*(uint128_t*)prefix;
        al_addr_af(maddr6)=AF_INET6;
        al_addr_addr(addr6)&=al_addr_addr(maddr6);        
        maddr=(al_addr_t*)&maddr6;
    }
    
    rte_rwlock_write_lock(&ptr->lock);
    rd=ptr->rnh_deladdr(addr, maddr, &ptr->rh);
    if( rd != NULL ){
        rc=(iptype == AF_INET) ? ((al_geoip_t*)rd)->id : ((al_geoip6_t*)rd)->id;
        rte_rwlock_write_unlock(&ptr->lock);
        al_free((void*) rd);
        return rc;
    }else{
        rte_rwlock_write_unlock(&ptr->lock);
        return -1;
    }

}

int al_geoip_find(uint32_t src, uint32_t* id){

    al_geoip_t* r4;
    struct radix_node* rd;
    struct radix_node_head* ptr;

    al_addr4_t addr4;
    al_addr_t  *addr;


    al_addr_len(addr4)=sizeof(al_addr4_t);
    al_addr_addr(addr4)=src;
    al_addr_af(addr4)=AF_INET;
    addr=(al_addr_t*)&addr4;
    
    ptr=*al_geoip_table(AF_INET);
    rte_rwlock_read_lock(&ptr->lock);
    rd=ptr->rnh_matchaddr((void*)addr,  &ptr->rh);
    if( likely(rd != NULL) ){
        r4=(al_geoip_t*)rd;
        *id=r4->id;
        rte_rwlock_read_unlock(&ptr->lock);
        return 0;
    }
    
    rte_rwlock_read_unlock(&ptr->lock);
    return -1;
}

int al_geoip6_find(uint128_t src, uint32_t* id){

    al_geoip6_t* r6;
    struct radix_node* rd;
    struct radix_node_head* ptr;

    al_addr6_t addr6;
    al_addr_t  *addr;

    al_addr_len(addr6)=sizeof(al_addr6_t);
    if( src ){
        al_addr_addr(addr6)=src;    
    }
    al_addr_af(addr6)=AF_INET6;
    addr=(al_addr_t*)&addr6;
    
    ptr=*al_geoip_table(AF_INET6);
    rte_rwlock_read_lock(&ptr->lock);
    rd=ptr->rnh_matchaddr((void*)addr,  &ptr->rh);
    if( rd != NULL ){
        r6=(al_geoip6_t*)rd;
        *id=r6->id;
        rte_rwlock_read_unlock(&ptr->lock);
        return 0;
    }
    
    rte_rwlock_read_unlock(&ptr->lock);
    return -1;
}

int al_geoip_dump(FILE* fp){
    struct radix_node_head* rh;
    rh=*al_geoip_table(AF_INET);
    al_geoip_dump_table_v4(rh, fp);
    rh=*al_geoip_table(AF_INET6);
    al_geoip_dump_table_v6(rh, fp);
    return 0;
}

static void al_geoip_dump_table_v4(struct radix_node_head* rh, FILE* fp){
    rte_rwlock_read_lock(&rh->lock);
    rh->rnh_walktree(&rh->rh, al_geoip_dump_info_v4, fp);
    rte_rwlock_read_unlock(&rh->lock);
}

static void al_geoip_dump_table_v6(struct radix_node_head* rh, FILE* fp){
    rte_rwlock_read_lock(&rh->lock);
    rh->rnh_walktree(&rh->rh, al_geoip_dump_info_v6, fp);
    rte_rwlock_read_unlock(&rh->lock);
}

static int al_geoip_dump_info_v4(struct radix_node *rn, void *arg)
{
    FILE * fp = arg;
    int sclen=128;
    char buf[128];
    al_geoip_t* r=(al_geoip_t*)rn;
	
    fprintf(fp, "%u", r->id);
    fprintf(fp, "	 ipv4");

    fprintf(fp,"	%s", inet_ntop(AF_INET, al_addr_addr_ptr(r->addr), buf, sclen));
    fprintf(fp,"	%s", inet_ntop(AF_INET, al_addr_addr_ptr(r->prefix), buf, sclen));

    return (0);
}

static int al_geoip_dump_info_v6(struct radix_node *rn, void *arg)
{
    FILE * fp = arg;
    int sclen=128;
    char buf[128];
    al_geoip6_t* r=(al_geoip6_t*)rn;
	
    fprintf(fp, "%u", r->id);
    fprintf(fp, "	 ipv6");

    fprintf(fp,"	%s", inet_ntop(AF_INET6, al_addr_addr_ptr(r->addr), buf, sclen));
    fprintf(fp,"	%s", inet_ntop(AF_INET6, al_addr_addr_ptr(r->prefix), buf, sclen));

    return (0);
}


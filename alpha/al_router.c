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
#include "al_router.h"
#include "al_radix.h"
#include "al_mempool.h"
#include "al_dev.h"

enum rn_type{
    AL_RN_INET,
    AL_RN_INET6,
    AL_RN_EN
};

static struct radix_node_head* al_router[AL_RN_EN];
static struct radix_node_head* al_policy_router[AL_RN_EN];
static int al_route_address_off[AL_RN_EN]={32,128};

#define al_router_table(rn_t) (rn_t == AF_INET ? &al_router[AL_RN_INET] : ( rn_t==AF_INET6 ? &al_router[AL_RN_INET6] : NULL))
#define al_route_policy_table(rn_t) (rn_t == AF_INET ? &al_policy_router[AL_RN_INET] : ( rn_t==AF_INET6 ? &al_policy_router[AL_RN_INET6] : NULL))

#define al_router(type, rn_t) ( type == POLICY_TABL ?  al_route_policy_table(rn_t) :  al_router_table(rn_t))

static int al_router_dump_info_v6(struct radix_node *rn, void *arg);
static int al_router_dump_info_v4(struct radix_node *rn, void *arg);
static void al_router_dump_table_v6(struct radix_node_head* rh, FILE* fp);
static void al_router_dump_table_v4(struct radix_node_head* rh, FILE* fp);

int al_router_init(void){

    struct radix_node_head** ptr=al_router_table(AF_INET);
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_route_address_off[AL_RN_INET]) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    ptr=al_router_table(AF_INET6);
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_route_address_off[AL_RN_INET6]) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    ptr=al_route_policy_table(AF_INET);
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_route_address_off[AL_RN_INET]) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    ptr=al_route_policy_table(AF_INET6);
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_route_address_off[AL_RN_INET6]) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    return 0;
}

int al_router_insert(uint16_t id, uint8_t* subnet, uint8_t* prefix, uint8_t* nexthop, uint8_t iptype, uint8_t rtype, uint32_t oif){
    al_router_t* r4;
    al_router6_t* r6;
    struct radix_node_head* ptr;
    int rc;
    uint8_t index;
#if 0
    LOG(LOG_INFO, LAYOUT_CONF, "Add route vlanid:%d route type:%d", vlanid, rtype);
#endif

    if(al_get_port_index_byid(oif, &index)){
        LOG(LOG_ERROR, LAYOUT_CONF, "Invlanid portid:%d", oif);
        return 0;
    }
    
    if( iptype == AF_INET ){
        r4=al_calloc(sizeof(al_router_t));
        if( r4 == NULL ){
            rc = -1;
            goto failed;
        }

        al_addr_len(r4->addr)=sizeof(al_addr4_t);
        al_addr_addr(r4->addr)=*(uint32_t*)subnet;
        al_addr_af(r4->addr)=AF_INET;
        al_addr_len(r4->prefix)=sizeof(al_addr4_t);
        al_addr_addr(r4->prefix)=*(uint32_t*)prefix;
        al_addr_addr(r4->addr)&=al_addr_addr(r4->prefix);
        al_addr_af(r4->prefix)=AF_INET;
        al_addr_len(r4->nexthop)=sizeof(al_addr4_t);
        al_addr_addr(r4->nexthop)=(nexthop==NULL) ? 0 : *(uint32_t*)nexthop;
        al_addr_af(r4->nexthop)=AF_INET;
        
        ptr=*al_router(rtype, AF_INET);
        rte_rwlock_write_lock(&ptr->lock);
        if(!ptr->rnh_addaddr(&r4->addr, &r4->prefix, &ptr->rh, r4->nodes)){
            rte_rwlock_write_unlock(&ptr->lock);
            rc = -1;
            al_free(r4);
            goto failed;
        }else{
            rte_rwlock_write_unlock(&ptr->lock);
            r4->id=id;
            r4->rtype = rtype;
            r4->oif=index;
        }
    }else if( iptype == AF_INET6 ){
        r6=al_calloc(sizeof(al_router6_t));
        if( r6 == NULL ){
            rc = -1;
            goto failed;
        }
        
        al_addr_len(r6->addr)=sizeof(al_addr6_t);
        al_addr_addr(r6->addr)=*(uint128_t*)subnet;
        al_addr_af(r6->addr)=AF_INET6;
        al_addr_len(r6->prefix)=sizeof(al_addr6_t);
        al_addr_addr(r6->prefix)=*(uint128_t*)prefix;
        al_addr_af(r6->prefix)=AF_INET6;
        al_addr_addr(r6->addr)&=al_addr_addr(r6->prefix);        
        al_addr_len(r6->nexthop)=sizeof(al_addr6_t);
        al_addr_addr(r6->nexthop)=(nexthop == NULL) ? 0 : *(uint128_t*)nexthop;
        al_addr_af(r6->nexthop)=AF_INET6;
        
        ptr=*al_router(rtype, AF_INET6);
        rte_rwlock_write_lock(&ptr->lock);
        if(!ptr->rnh_addaddr(&r6->addr, &r6->prefix, &ptr->rh, r6->nodes)){
            rte_rwlock_write_unlock(&ptr->lock);
            rc = -1;
            al_free(r6);            
            goto failed;
        }else{
            rte_rwlock_write_unlock(&ptr->lock);
            r6->id=id;
            r6->rtype = rtype;
            r6->oif=index;
        }
    }else{
        rc = -1;
        goto failed;
    }

    rc=0;
failed:
    return rc;
}

int al_router_delete(uint8_t* subnet, uint8_t* prefix, uint8_t iptype, uint8_t type){

    struct radix_node* rd;
    struct radix_node_head* ptr=*al_router(type, iptype);
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
        rc=(iptype == AF_INET) ? ((al_router_t*)rd)->id : ((al_router6_t*)rd)->id;
        rte_rwlock_write_unlock(&ptr->lock);
        al_free((void*) rd);
        return rc;
    }else{
        rte_rwlock_write_unlock(&ptr->lock);
        return -1;
    }

}

int al_router_find(uint8_t* src, uint8_t* dst,  uint8_t iptype, uint8_t* nexthop, uint8_t* oif){

    al_router_t* r4;
    al_router6_t* r6;
    struct radix_node* rd;
    struct radix_node_head* ptr;

    al_addr4_t addr4;
    al_addr6_t addr6;
    al_addr_t  *addr;

    if( src ){
        ptr=*al_router(POLICY_TABL, iptype);
    }
    
    if( iptype == AF_INET ){
        al_addr_len(addr4)=sizeof(al_addr4_t);
        if( src ){
            al_addr_addr(addr4)=*(uint32_t*)src;    
        }
        al_addr_af(addr4)=AF_INET;
        addr=(al_addr_t*)&addr4;
    }else{
        al_addr_len(addr6)=sizeof(al_addr6_t);
        if( src ){
            al_addr_addr(addr6)=*(uint128_t*)src;    
        }
        al_addr_af(addr6)=AF_INET6;
        addr=(al_addr_t*)&addr6;
    }

    if( src ){
        rte_rwlock_read_lock(&ptr->lock);
        rd=ptr->rnh_matchaddr(addr, &ptr->rh);
        if( rd != NULL ){
            if( iptype == AF_INET ){
                r4=(al_router_t*)rd;
                *(uint32_t*)nexthop = (al_addr_addr(r4->nexthop) == 0) ? *(uint32_t*)dst : al_addr_addr(r4->nexthop);
                *oif=r4->oif;
                rte_rwlock_read_unlock(&ptr->lock);
                return 0;
            }else{
                r6=(al_router6_t*)rd;
                *(uint128_t*)nexthop = (al_addr_addr(r6->nexthop) == 0) ? *(uint128_t*)dst : al_addr_addr(r6->nexthop);
                *oif=r6->oif;
                rte_rwlock_read_unlock(&ptr->lock);
                return 0;
            }
        }
        rte_rwlock_read_unlock(&ptr->lock);
    }

    if( iptype == AF_INET ){
        al_addr_addr(addr4)=*(uint32_t*)dst;
    }else{
        al_addr_addr(addr6)=*(uint128_t*)dst;
    }
    
    ptr=*al_router(-1, iptype);
    rte_rwlock_read_lock(&ptr->lock);
    rd=ptr->rnh_matchaddr((void*)addr,  &ptr->rh);
    if( rd != NULL ){
        if( iptype == AF_INET ){
            r4=(al_router_t*)rd;
            *(uint32_t*)nexthop = (al_addr_addr(r4->nexthop) == 0) ? *(uint32_t*)dst : al_addr_addr(r4->nexthop);
            *oif=r4->oif;
            rte_rwlock_read_unlock(&ptr->lock);
            return 0;
        }else{
            r6=(al_router6_t*)rd;
            *(uint128_t*)nexthop = (al_addr_addr(r6->nexthop) == 0) ? *(uint128_t*)dst : al_addr_addr(r6->nexthop);
            *oif=r6->oif;
            rte_rwlock_read_unlock(&ptr->lock);
            return 0;
        }
    }
    
    rte_rwlock_read_unlock(&ptr->lock);
    return -1;
}

int al_dump_router(FILE* fp){
    struct radix_node_head* rh;
    rh=*al_router(POLICY_TABL, AF_INET);
    al_router_dump_table_v4(rh, fp);
    rh=*al_router(-1, AF_INET);
    al_router_dump_table_v4(rh, fp);
    rh=*al_router(POLICY_TABL, AF_INET6);
    al_router_dump_table_v6(rh, fp);
    rh=*al_router(-1, AF_INET6);
    al_router_dump_table_v6(rh, fp);
    return 0;
}

static void al_router_dump_table_v4(struct radix_node_head* rh, FILE* fp){
    rte_rwlock_read_lock(&rh->lock);
    rh->rnh_walktree(&rh->rh, al_router_dump_info_v4, fp);
    rte_rwlock_read_unlock(&rh->lock);
}

static void al_router_dump_table_v6(struct radix_node_head* rh, FILE* fp){
    rte_rwlock_read_lock(&rh->lock);
    rh->rnh_walktree(&rh->rh, al_router_dump_info_v6, fp);
    rte_rwlock_read_unlock(&rh->lock);
}

static int al_router_dump_info_v4(struct radix_node *rn, void *arg)
{
    FILE * fp = arg;
    int sclen=128;
    char buf[128];
    al_router_t* r=(al_router_t*)rn;
	
    fprintf(fp, "%u", r->id);
    fprintf(fp, "	 ipv4");

    fprintf(fp,"	%s", inet_ntop(AF_INET, al_addr_addr_ptr(r->addr), buf, sclen));
    fprintf(fp,"	%s", inet_ntop(AF_INET, al_addr_addr_ptr(r->prefix), buf, sclen));
    fprintf(fp,"	%s\n", inet_ntop(AF_INET, al_addr_addr_ptr(r->nexthop), buf, sclen));

    return (0);
}

static int al_router_dump_info_v6(struct radix_node *rn, void *arg)
{
    FILE * fp = arg;
    int sclen=128;
    char buf[128];
    al_router6_t* r=(al_router6_t*)rn;
	
    fprintf(fp, "%u", r->id);
    fprintf(fp, "	 ipv6");

    fprintf(fp,"	%s", inet_ntop(AF_INET6, al_addr_addr_ptr(r->addr), buf, sclen));
    fprintf(fp,"	%s", inet_ntop(AF_INET6, al_addr_addr_ptr(r->prefix), buf, sclen));
    fprintf(fp,"	%s\n", inet_ntop(AF_INET6, al_addr_addr_ptr(r->nexthop), buf, sclen));

    return (0);
}

int al_router_rtype(const char* name){
    if( strcmp(name, "policy") == 0){
        return POLICY_TABL;
    }else if( strcmp(name, "direct") == 0){
        return DIRECT_TABL;
    }else if( strcmp(name, "static") == 0){
        return STATIC_TABL;
    }else if( strcmp(name, "dynamic") == 0){
        return DYNAMIC_TABL;
    }else if( strcmp(name, "gateway") == 0){
        return GATEWAY_TABL;
    }else{
        return -1;
    }
}


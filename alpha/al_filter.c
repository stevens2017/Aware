#include <rte_rwlock.h>
#include "al_common.h"
#include "al_mempool.h"
#include "hlist.h"
#include "al_tree.h"
#include "al_to_api.h"
#include "al_filter.h"
#include "al_log.h"
#include "al_group.h"
#include "al_to_main.h"
#include "al_errmsg.h"
#include "common/al_string.h"
#include "al_radix.h"
#include "al_router.h"

typedef struct al_filter_radix{
    struct	radix_node nodes[2];
    al_addr4_t addr;
    al_addr4_t prefix;
    uint32_t    index;
}__attribute__((__packed__))al_filter_radix_t;

typedef struct al_filter6_radix{
    struct	radix_node nodes[2];
    al_addr6_t addr;
    al_addr6_t prefix;
    uint32_t    index;
}__attribute__((__packed__))al_filter6_radix_t;

typedef struct{
    al_rbtree_t            rbtree;
    al_rbtree_node_t  sentinel;
}al_filter_tree;

static al_filter_tree g_filters;
static rte_rwlock_t g_filters_lock;
static uint32_t g_index;

enum{
    AL_IPV4_FILTER=0,
    AL_IPV6_FILTER,
    AL_FILTER
};

static struct radix_node_head* al_filter_index[AL_FILTER];
static int filter_address_off[AL_FILTER]={32,128};

#define al_filter4()  al_filter_index[AL_IPV4_FILTER]
#define al_filter4_offset()  filter_address_off[AL_IPV4_FILTER]
#define al_filter6()  al_filter_index[AL_IPV6_FILTER]
#define al_filter6_offset()  filter_address_off[AL_IPV6_FILTER]

static int al_filter_dump_info_v6(struct radix_node *rn, void *arg);
static int al_filter_dump_info_v4(struct radix_node *rn, void *arg);
static void al_filter_dump_table_v6(struct radix_node_head* rh, FILE* fp);
static void al_filter_dump_table_v4(struct radix_node_head* rh, FILE* fp);


int al_filter_init(void){
    al_rbtree_init(&g_filters.rbtree, &g_filters.sentinel, al_rbtree_insert_value);
    rte_rwlock_init(&g_filters_lock);

    struct radix_node_head** ptr=&al_filter4();
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr,  al_filter4_offset())){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    ptr=&al_filter6();
    *ptr=NULL;
    if( !al_route_head_init((void**)ptr, al_filter6_offset()) ){
        return 0;
    }else{
        rte_rwlock_init(&(*ptr)->lock);
    }

    g_index=0;
    return 0;
 }

static uint32_t al_filter_radix_insert(al_addr_t* addr, al_addr_t* msk){

    struct radix_node_head* ptr;
    al_filter_radix_t  *f4;
    al_filter6_radix_t  *f6;
 
    if( al_p_addr_af(addr) == AF_INET ){
        f4=al_calloc(sizeof(al_filter_radix_t));
        if (f4 == NULL) {
            return 0;
        }else{
            f4->addr=*(al_addr4_t*)addr;
            f4->prefix=*(al_addr4_t*)msk;
            f4->index=ATOMIC_INC(&g_index);

            ptr=al_filter4();
            rte_rwlock_write_lock(&ptr->lock);
            if(!ptr->rnh_addaddr(&f4->addr, &f4->prefix, &ptr->rh, f4->nodes)){
                rte_rwlock_write_unlock(&ptr->lock);
                al_free(f4);
                return 0;
            }else{
                rte_rwlock_write_unlock(&ptr->lock);
            }
            return f4->index;
        }
    }else if( al_p_addr_af(addr) == AF_INET6 ){
        f6=al_calloc(sizeof(al_filter6_radix_t));
        if( f6 == NULL ){
            return 0;
        }else{
            f6->addr=*(al_addr6_t*)addr;
            f6->prefix=*(al_addr6_t*)msk;
            f6->index=ATOMIC_INC(&g_index);

            ptr=al_filter6();
            rte_rwlock_write_lock(&ptr->lock);
            if(!ptr->rnh_addaddr(&f6->addr, &f6->prefix, &ptr->rh, f6->nodes)){
                rte_rwlock_write_unlock(&ptr->lock);
                al_free(f6);
                return 0;
            }else{
                rte_rwlock_write_unlock(&ptr->lock);
            }
        }

        return f6->index;
    }else{
        return 0;
    }
}

static int al_filter_radix_del(al_addr_t* addr, al_addr_t* msk){

    struct radix_node_head* ptr;
    struct radix_node* rd;
    int rc;
    
    if( al_p_addr_af(addr) == AF_INET ){
        ptr=al_filter4();
    }else if(al_p_addr_af(addr) == AF_INET6){
        ptr=al_filter6();
    }else{
        return 0;
    }

    rte_rwlock_write_lock(&ptr->lock);
    rd=ptr->rnh_deladdr(addr, msk, &ptr->rh);
    if( rd != NULL ){
        rc=(al_p_addr_af(addr) == AF_INET) ? ((al_filter_radix_t*)rd)->index : ((al_filter6_radix_t*)rd)->index;
        al_free((void*) rd);
    }else{
        rc=0;
    }
    rte_rwlock_write_unlock(&ptr->lock);

    return rc;
}

static uint32_t al_filter_radix_lookup(al_addr_t* addr, al_addr_t* msk){

    struct radix_node_head* ptr;
    struct radix_node* rd;
    uint32_t rc;
    
    if( al_p_addr_af(addr) == AF_INET ){
        ptr=al_filter4();
    }else if(al_p_addr_af(addr) == AF_INET6){
        ptr=al_filter6();
    }else{
        return 0;
    }

    rte_rwlock_write_lock(&ptr->lock);
    rd=ptr->rnh_lookup(addr, msk, &ptr->rh);
    if( rd != NULL ){
        rc=(al_p_addr_af(addr) == AF_INET) ? ((al_filter_radix_t*)rd)->index : ((al_filter6_radix_t*)rd)->index;
    }else{
        rc=0;
    }
    rte_rwlock_write_unlock(&ptr->lock);

    return rc;
}


static int al_filter4_radix_lookup(uint32_t addr, uint32_t* index, uint32_t* msk){
    struct radix_node_head* ptr;
    struct radix_node* rd;
    al_filter_radix_t* f;
    int rc=-1;
    al_addr4_t dst;

    al_addr_len(dst)=sizeof(al_addr4_t);
    al_addr_addr(dst)=addr;
    al_addr_af(dst)=AF_INET;
        
    ptr=al_filter4();
    rte_rwlock_read_lock(&ptr->lock);
    rd=ptr->rnh_matchaddr(&dst, &ptr->rh);
    if( rd != NULL ){
        f=(al_filter_radix_t*)rd;
        *index=f->index;
        rc = 0;
        if(unlikely(msk != NULL)){
            *msk=al_addr_addr(f->prefix);
        }
    }
    rte_rwlock_read_unlock(&ptr->lock);

    
    return rc; 
}

static int al_filter6_radix_lookup(uint128_t addr, uint32_t* index, uint128_t* msk){
    struct radix_node_head* ptr;
    struct radix_node* rd;
    al_filter6_radix_t* f;
    int rc=-1;
    al_addr6_t dst;

    al_addr_len(dst)=sizeof(al_addr6_t);
    al_addr_addr(dst)=addr;
    al_addr_af(dst)=AF_INET6;

    ptr=al_filter6();
    rte_rwlock_read_lock(&ptr->lock);
    rd=ptr->rnh_matchaddr(&dst, &ptr->rh);
    if( rd != NULL ){
        f=(al_filter6_radix_t*)rd;
        *index=f->index;
        rc =0;
        if(unlikely(msk != NULL)){
            *msk=al_addr_addr(f->prefix);
        }        
    }
    rte_rwlock_read_unlock(&ptr->lock);
    
    return rc;
}

const int8_t al_filter_type(const char* t){
    if(strcmp(t, "pkg_filter") == 0){
        return AL_FLT_PKG;
    }else if(strcmp(t, "nat") == 0){
        return AL_FLT_NAT;
    }else if(strcmp(t, "llb") == 0){
        return AL_FLT_LLB;
    }else if(strcmp(t, "simple_trans") == 0){
        return AL_FLT_SIMPLE_TRANS;
    }else if(strcmp(t, "gslb") == 0){
        return AL_FLT_GSLB;
    }else if(strcmp(t, "tcp_proxy") == 0){
        return AL_FLT_TCP_PROXY;
    }else if(strcmp(t, "http_proxy") == 0){
        return AL_FLT_HTTP_PROXY;   
    }else{
        return -1;
    }
}

static al_filter_t* al_filter_lookup(al_filter_chain_t* c, uint16_t port, uint8_t proto){

    int i, j;
    al_filter_t* m;
    struct hlist_node* n;
    al_filter_chain_node_t* mc;
    
    for(i=0, j=0; i<c->cnt; j++){
        m=&c->m[j];
        if(m->is_used){
            i++;
            if((m->port == port)&&( (m->proto==proto)||(m->proto==IPPROTO_IP))){
                return m;
            }
        }
    }

    hlist_for_each_entry_safe(mc, n, &c->list, list){
        for(i=0, j=0; i<mc->cnt; j++){
            m=&mc->m[j];
            if(m->is_used){
                i++;
                if((m->port == port)&&( (m->proto==proto)||(m->proto==IPPROTO_IP))){                
                    return m;
                }
            }
        }
    }

    return NULL;
}


int al_filter4_search(uint32_t dst, uint16_t port, uint8_t proto, al_fargs* fargs){
    int rc=-1;
    al_rbtree_node_t* node;
    al_filter_chain_t* fc;
    al_filter_t* f;
    uint32_t index;

    if( al_filter4_radix_lookup(dst, &index, NULL) ){
        return -1;
    }

    rte_rwlock_read_lock(&g_filters_lock);
    node=al_rbtree_lookup(&g_filters.rbtree, index);
    if( node ){
        fc=(al_filter_chain_t*)((al_filter_node_t*)&node->color)->data;
        f = al_filter_lookup(fc, port, proto);
        if( f ){
            rc=0;
            if( fargs ){
                fargs->ftype=f->type;
                fargs->fid=f->fid;
                fargs->gid=f->gid;                    
            }
        }
    }else{
        errno=AL_ENOOBJ;
    }
    rte_rwlock_read_unlock(&g_filters_lock);

    return rc;
}

int al_filter6_search(uint128_t dst, uint16_t port, uint8_t proto, al_fargs* fargs){
    int rc=-1;
    al_rbtree_node_t* node;
    al_filter_chain_t* fc;
    al_filter_t* f;
    uint32_t index;

    if( al_filter6_radix_lookup(dst, &index, NULL) ){
        return -1;
    }
    
    rte_rwlock_read_lock(&g_filters_lock);
    node=al_rbtree_lookup(&g_filters.rbtree, index);
    if( node ){
        fc=(al_filter_chain_t*)((al_filter_node_t*)&node->color)->data;
        f = al_filter_lookup(fc, port, proto);
        if( f ){
            rc=0;
            if( fargs ){
                fargs->ftype=f->type;
                fargs->fid=f->fid;
                fargs->gid=f->gid;                    
            }
        }
    }else{
        errno=AL_ENOOBJ;
    }
    rte_rwlock_read_unlock(&g_filters_lock);

    return rc;
}

static int al_filter_delete(al_filter_chain_t* c, uint32_t fid){
    int i, s, j;
    al_filter_t* m;
    struct hlist_node* n;
    al_filter_chain_node_t* mc;
    
    for(i=0, j=0; i<c->cnt; j++){
        m=&c->m[j];
        if(m->is_used){
            if(m->fid == fid){
                m->is_used=0;
                c->cnt--;
                c->nums--;
                return 0;
            }
            i++;
        }
    }

    hlist_for_each_entry_safe(mc, n, &c->list, list){
        s=0;
        for(i=0, j=0; i<mc->cnt; j++){
            m=&mc->m[j];
            if(m->is_used){
                if(m->fid == fid){
                    m->is_used=0;
                    mc->cnt--;
                    c->nums--;
                    return 0;
                }
                s=1;
                i++;
            }
        }
        
        if( !s ){
            hlist_del(&mc->list);
            al_free(mc);
        }
    }

    return -1;
}

int al_filter_del(al_api_filter* f){

    al_rbtree_node_t* node;
    al_filter_chain_t* mc;
    int rc;
    uint32_t index;

    al_addr_t*  addr;
    al_addr_t*  mask;

    if( f->iptype == AF_INET ){
        addr=(al_addr_t*)&f->addr4;
        mask=(al_addr_t*)&f->mask4;
    }else if(f->iptype == AF_INET6){
        addr=(al_addr_t*)&f->addr6;
        mask=(al_addr_t*)&f->mask6;
    }

    index = al_filter_radix_lookup(addr, mask);
    if( index == 0 ){
        return -1;
    }
    
    rc=-1;
    rte_rwlock_write_lock(&g_filters_lock);
    node=al_rbtree_lookup(&g_filters.rbtree, index);
    if( node ){
        mc=(al_filter_chain_t*)((al_filter_node_t*)&node->color)->data;
        rc=al_filter_delete(mc, f->id);
        if( mc->nums == 0 ){
            if(al_filter_radix_del(addr, mask) == 0  ){
                LOG(LOG_ERROR, LAYOUT_APP, "delete filter radix failed");
                rc = -1;
            }
            al_rbtree_delete(&g_filters.rbtree, node);
            al_free(node);
        }
    }else{
        errno=AL_ENOOBJ;
    }
    rte_rwlock_write_unlock(&g_filters_lock);

    return rc;
}

static void al_set_filter(al_filter_t* m, al_api_filter* conf){
    m->fid=conf->id;
    m->gid=conf->gid;
    m->is_used=1;
    m->type=conf->filter_type;
    m->proto=conf->proto;
    m->port=htons(conf->port);
}

static int al_filter_add(void* cc, al_api_filter* conf){

    int i, rc;
    al_filter_t* m;
    struct hlist_node* n;
    al_filter_chain_node_t* mc;
    al_filter_chain_t* c=cc;

    /*load new group*/
    if((rc=al_group_load(conf->id, conf->gid))){
        return rc;
    }
    
    for(i=0; i<FILTER_CLUSTER_SIZE; i++){
        m=&c->m[i];
        if(!m->is_used){
            al_set_filter(m, conf);
            c->cnt++;
            c->nums++;
            return 0;
        }
    }

    hlist_for_each_entry_safe(mc, n, &c->list, list){
        
        for(i=0; i<mc->cnt; i++){
            m=&mc->m[i];
            if(!m->is_used){
                al_set_filter(m, conf);
                mc->cnt++;
                c->nums++;
                return 0;
            }
        }
    }

    mc=al_malloc(sizeof(al_filter_chain_node_t));
    if( mc == NULL){
        errno=ENOMEM;
        return -1;
    }
    INIT_FC(mc->m, FILTER_CLUSTER_SIZE, i);
    hlist_add_head(&mc->list, &c->list);

    al_set_filter(&mc->m[0], conf);
    mc->cnt=1;
    return 0;
}

int al_filter_insert(al_api_filter* f){

    al_rbtree_node_t* node;
    al_filter_chain_t*  mc;
    int rc, i, s;
    size_t size;
    uint32_t index;
    uint32_t msk4;
    uint128_t msk6;
    
    al_addr_t*  addr;
    al_addr_t*  mask;
    
    s = 1;
    if( f->iptype == AF_INET ){
        if(al_filter4_radix_lookup(f->ip4, &index, &msk4)==0){
           if( msk4 != f->msk4 ){
               s=0;
           }
        }else{
            s=0;
        }
        addr=(al_addr_t*)&f->addr4;
        mask=(al_addr_t*)&f->mask4;
    }else if(f->iptype == AF_INET6){
        if(al_filter6_radix_lookup(f->ip6, &index, &msk6)==0){
            if( msk6 != f->msk6 ){
                s=0;
            }
        }else{
            s=0;
        }
        addr=(al_addr_t*)&f->addr6;
        mask=(al_addr_t*)&f->mask6;
    }

    rc=-1;
    rte_rwlock_write_lock(&g_filters_lock);

    if( s ){
        node=al_rbtree_lookup(&g_filters.rbtree, index);
        mc=(al_filter_chain_t*)((al_filter_node_t*)&node->color)->data;
        rc=al_filter_add(mc, f);
        goto END;
    }else{
        index = al_filter_radix_insert(addr, mask);
        if( index == 0 ){
            goto END;
        }
        
        size = offsetof(al_rbtree_node_t, color) + sizeof(al_filter_node_t)+sizeof(al_filter_chain_t);
        node = al_malloc(size);
        if( node == NULL ){
            goto ENDF;
        }else{
            node->key = index;

            mc=(al_filter_chain_t*)((al_filter_node_t*)&node->color)->data;
            INIT_HLIST_HEAD(&mc->list);
            INIT_FC(mc->m, FILTER_CLUSTER_SIZE, i);
            mc->cnt=0;
            mc->nums=0;

            if(al_filter_add(mc, f)){
                al_filter_chain_release(mc);
                al_free(node);
                goto ENDF;
            }
            al_rbtree_insert(&g_filters.rbtree, node);
        }
    }
    rc=0;

    goto END;
    
ENDF:
    if(al_filter_radix_del(addr, mask) == -1){
        LOG(LOG_ERROR, LAYOUT_APP, "Del radix filter failed");
    }                
    
END:
    rte_rwlock_write_unlock(&g_filters_lock);

    return rc;
}

static void al_filter_tree_walk(al_rbtree_node_t* t, al_rbtree_node_t* sentinel, FILE* fp){
    al_filter_node_t* mn;
    al_filter_chain_t* mc;
    al_filter_chain_node_t* c;
    al_filter_t* m;
    struct hlist_node* n;
    int i, j, index;
    if( t == sentinel){
        return;
    }else{
        
        al_filter_tree_walk(t->left, sentinel, fp);
        al_filter_tree_walk(t->right, sentinel, fp);
        //output t
        mn=(al_filter_node_t*)&t->color;
        mc=(al_filter_chain_t*)mn->data;

        j=0;
        index=0;
        for(i=0, j=0; i<mc->cnt; j++){
            m=&mc->m[j];
            if( !m->is_used ){
                continue;
            }
            i++;
            fprintf(fp, "    %8d ", index++);
            fprintf(fp, "%8d", m->fid);
            fprintf(fp, "%8d", m->gid);            
            fprintf(fp, "%8d", ntohs(m->port));
            fprintf(fp, "%8d", m->type);
            fprintf(fp, " %"PRIuPTR"\n", t->key);
        }

        hlist_for_each_entry_safe(c, n, &mc->list, list){
            for(i=0, j=0; i<mc->cnt; j++){
                m=&c->m[j];
                if(!m->is_used){
                    continue;
                }
                i++;
                fprintf(fp, "    %8d ", index++);
                fprintf(fp, "%8d", m->fid);
                fprintf(fp, "%8d", m->gid);    
                fprintf(fp, "%8d", ntohs(m->port));
                fprintf(fp, "%8d", m->type);
                fprintf(fp, " %"PRIuPTR"\n", t->key);
            }
        }
    }
}

static void al_dump_filter(FILE* fp){
    struct radix_node_head* rh;
    rh=al_filter4();
    al_filter_dump_table_v4(rh, fp);
    rh=al_filter6();
    al_filter_dump_table_v6(rh, fp);
}

void al_dump_filter_api(FILE* fp){
    al_dump_filter(fp);
    rte_rwlock_read_lock(&g_filters_lock);
    al_filter_tree_walk(g_filters.rbtree.root, &g_filters.sentinel, fp);
    rte_rwlock_read_unlock(&g_filters_lock);
}

static void al_filter_dump_table_v4(struct radix_node_head* rh, FILE* fp){
    rte_rwlock_read_lock(&rh->lock);
    rh->rnh_walktree(&rh->rh, al_filter_dump_info_v4, fp);
    rte_rwlock_read_unlock(&rh->lock);
}

static void al_filter_dump_table_v6(struct radix_node_head* rh, FILE* fp){
    rte_rwlock_read_lock(&rh->lock);
    rh->rnh_walktree(&rh->rh, al_filter_dump_info_v6, fp);
    rte_rwlock_read_unlock(&rh->lock);
}

static int al_filter_dump_info_v4(struct radix_node *rn, void *arg)
{
    FILE * fp = arg;
    int sclen=128;
    char buf[128];
    al_filter_radix_t* r=(al_filter_radix_t*)rn;
	
    fprintf(fp, "%u", r->index);

    fprintf(fp,"	%s", inet_ntop(AF_INET, al_addr_addr_ptr(r->addr), buf, sclen));
    fprintf(fp,"	%s\n", inet_ntop(AF_INET, al_addr_addr_ptr(r->prefix), buf, sclen));

    return (0);
}

static int al_filter_dump_info_v6(struct radix_node *rn, void *arg)
{
    FILE * fp = arg;
    int sclen=128;
    char buf[128];
    al_filter6_radix_t* r=(al_filter6_radix_t*)rn;

    fprintf(fp, "%u", r->index);
    fprintf(fp, "	 ipv6");

    fprintf(fp,"	%s", inet_ntop(AF_INET6, al_addr_addr_ptr(r->addr), buf, sclen));
    fprintf(fp,"	%s\n", inet_ntop(AF_INET6, al_addr_addr_ptr(r->prefix), buf, sclen));

    return (0);
}

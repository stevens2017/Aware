#include <stdio.h>
#include <string.h>
#include <rte_memzone.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "al_ip.h"
#include "al_hash.h"
#include "al_mempool.h"
#include "al_tree.h"
#include "al_to_api.h"
#include "al_member.h"
#include "al_common.h"
#include "al_errmsg.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_log.h"
#include "al_gslb.h"

/*schedule algorithm*/
enum{
  AL_ALG_RR = 0,
  AL_ALG_WEIGHT,
  AL_ALG_RR_WEIGHT,
  AL_ALG_GEOIP,
  AL_ALG_END
};

typedef struct{
    al_rbtree_t           rbtree;
    al_rbtree_node_t  sentinel;
}al_member_tree;

static al_member_tree g_members;
static rte_rwlock_t g_members_lock;

static al_member_t* al_alg_rr(al_member_chain_t* c, void* args);
static al_member_t* al_alg_weight(al_member_chain_t* c, void* args);
static al_member_t* al_alg_rr_weight(al_member_chain_t* c, void* args);
static al_member_t* al_alg_geoip(al_member_chain_t* c, void* args);

static int al_member_del(al_member_chain_t* c, uint32_t mid);
static int al_member_insert(void* cc, al_member_conf_t* conf);
static void al_set_member(al_member_t* m, al_member_conf_t* conf);

static al_member_t* (*g_alg_fun[AL_ALG_END])(al_member_chain_t*, void*)={
    al_alg_rr, al_alg_weight, al_alg_rr_weight, al_alg_geoip
};


int al_members_init(void){
    memset(&g_members, '\0', sizeof(g_members));
    al_rbtree_init(&g_members.rbtree, &g_members.sentinel, al_rbtree_insert_value);
    rte_rwlock_init(&g_members_lock);
    return 0;
}

int al_member_lookup(uint32_t fid, uint32_t gid, al_margs* margs){
    
    al_rbtree_node_t* node;
    al_member_node_t* mn;
    al_member_chain_t* mc;
    al_member_t* m;
    int rc;
    
    rte_rwlock_read_lock(&g_members_lock);
    rc=-1;
    node=al_rbtree_lookup(&g_members.rbtree, al_make_key(fid,gid));
    if( likely(node!=NULL) ){
        //find it
        mn=(al_member_node_t*)&node->color;
        mc=(al_member_chain_t*)mn->data;
        if(likely((m=g_alg_fun[mn->schd](mc, margs)) != NULL)){
            rc=0;
            margs->mid=m->mid;
            margs->port=m->port;
            if(al_addr_af(m->mb_address) == AF_INET){
                *(al_addr4_t*)&margs->addr=m->mb_address4;
            }else{
                *(al_addr6_t*)&margs->addr=m->mb_address6;
            }
            margs->schd=mn->schd;
        }
    }
    rte_rwlock_read_unlock(&g_members_lock);
    return rc;
}

static int al_group_load_cb(uint32_t fid, uint32_t gid, al_group_conf_t* fun){
    return al_group_insert(fid, gid, fun->schd);
}

int al_group_load(uint32_t fid, uint32_t gid){
    return al_group_info(fid, gid, al_group_load_cb);
}

int al_group_insert(uint32_t fid, uint32_t gid, uint8_t sched){
    al_member_node_t* mn;
    al_member_chain_t* mc;
    size_t  size;
    int i;
    al_rbtree_node_t* node;

    rte_rwlock_read_lock(&g_members_lock);
    node=al_rbtree_lookup(&g_members.rbtree, al_make_key(fid,gid));
    if( likely(node == NULL) ){
        rte_rwlock_read_unlock(&g_members_lock);
    }else{
        errno=AL_GROUP_ALREADY_EEXIST;
        rte_rwlock_read_unlock(&g_members_lock);
        return -1;
    }

    size = offsetof(al_rbtree_node_t, color) + sizeof(al_member_node_t)+sizeof(al_member_chain_t);
    node = al_malloc(size);
    if( node == NULL ){
        return -1;
    }

    node->key = al_make_key(fid, gid);

    mn=(al_member_node_t*)&node->color;
    mn->schd = sched;

    mc=(al_member_chain_t*)mn->data;
    INIT_HLIST_HEAD(&mc->list);
    INIT_MC(mc->m, MEMBER_CLUSTER_SIZE, i);

    mn->fid=fid;
    mn->gid=gid;

    if(al_group_load_member(gid, (void*)mc, al_member_insert)){
        al_member_chain_release(mc);
        al_free(node);
        return -1;
    }
    
    rte_rwlock_write_lock(&g_members_lock);
    al_rbtree_insert(&g_members.rbtree, node);
    rte_rwlock_write_unlock(&g_members_lock);
    return 0;
}

int al_group_delete(uint32_t fid, uint32_t gid){
    al_rbtree_node_t* node;
    al_member_chain_t* mc;
    rte_rwlock_write_lock(&g_members_lock);
    node=al_rbtree_lookup(&g_members.rbtree, al_make_key(fid,gid));
    if( node ){
        al_rbtree_delete(&g_members.rbtree, node);
        mc=(al_member_chain_t*)((al_member_node_t*)&node->color)->data;
        al_member_chain_release(mc);
        al_free(node);
        rte_rwlock_write_unlock(&g_members_lock);
        return 0;
    }else{
        rte_rwlock_write_unlock(&g_members_lock);
        errno=AL_ENOOBJ;
        return -1;
    }
}

int al_member_delete(uint32_t fid, uint32_t gid, uint32_t mid){
    al_rbtree_node_t* node;
    al_member_chain_t* mc;
    int rc;

    rc=-1;
    rte_rwlock_write_lock(&g_members_lock);
    node=al_rbtree_lookup(&g_members.rbtree, al_make_key(fid,gid));
    if( node ){
        mc=(al_member_chain_t*)((al_member_node_t*)&node->color)->data;
        rc=al_member_del(mc, mid);
    }else{
        errno=AL_ENOOBJ;
    }
    rte_rwlock_write_unlock(&g_members_lock);

    return rc;
}

static int al_member_del(al_member_chain_t* c, uint32_t mid){
    int i, s, j;
    al_member_t* m;
    struct hlist_node* n;
    al_member_chain_node_t* mc;
    
    for(i=0, j=0; i<c->cnt; j++){
        m=&c->m[j];
        if(m->is_used){
            if(m->mid == mid){
                m->is_used=0;
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
                if(m->mid == mid){
                    m->is_used=0;
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

int al_member_add(uint32_t fid, uint32_t gid, al_member_conf_t* conf){

    al_rbtree_node_t* node;
    al_member_chain_t* mc;
    int rc;

    rc=-1;
    rte_rwlock_write_lock(&g_members_lock);
    node=al_rbtree_lookup(&g_members.rbtree, al_make_key(fid,gid));
    if( node ){
        mc=(al_member_chain_t*)((al_member_node_t*)&node->color)->data;
        rc=al_member_insert(mc, conf);
    }
    rte_rwlock_write_unlock(&g_members_lock);

    return rc;
}

static int al_member_insert(void* cc, al_member_conf_t* conf){

    int i;
    al_member_t* m;
    struct hlist_node* n;
    al_member_chain_node_t* mc;
    al_member_chain_t* c=cc;
    
    for(i=0; i<MEMBER_CLUSTER_SIZE;i++){
        m=&c->m[i];
        if(!m->is_used){
            al_set_member(m, conf);
            m->is_used=1;
            c->cnt++;
            return 0;
        }
    }

    hlist_for_each_entry_safe(mc, n, &c->list, list){
        for(i=0; i<MEMBER_CLUSTER_SIZE; i++){
            m=&mc->m[i];
            if(!m->is_used){
                al_set_member(m, conf);
                m->is_used=1;
                mc->cnt++;
                return 0;
            }
        }
    }

    mc=al_malloc(sizeof(al_member_chain_node_t));
    if( mc == NULL){
        errno=ENOMEM;
        return -1;
    }
    INIT_MC(mc->m, MEMBER_CLUSTER_SIZE, i);
    hlist_add_head(&mc->list, &c->list);

    al_set_member(&mc->m[0], conf);
    mc->cnt=1;
    mc->m[0].is_used=1;
    return 0;
}

static void al_set_member(al_member_t* m, al_member_conf_t* conf){
    m->port=htons(conf->port);
    m->mid=conf->mid;
    m->weight=conf->weight;
    m->current_weight=0;
    m->effective_weight=m->weight;
    m->is_up=1;
    if( al_addr_af(m->mb_address) == AF_INET ){
        m->mb_address4 = conf->addr.addr4;
    }else{
        m->mb_address6 = conf->addr.addr6;
    }
}

static al_member_t* al_alg_rr(al_member_chain_t* c, void* args){

    int i,  j;
    uint32_t total;
    al_member_t* m, *best;
    struct hlist_node* n;
    al_member_chain_node_t* mc;

    best=NULL;
    total=0;
    
    for(i=0, j=0; i<c->cnt; j++){
        m=&c->m[j];
        if(m->is_used){
            i++;
            if(unlikely(!m->is_up)){
                continue;
            }
            m->current_weight += m->effective_weight;
            total += m->effective_weight;

            if( m->effective_weight < m->weight ){
                m->effective_weight++;
            }

            if ((best == NULL) || (m->current_weight > best->current_weight)) {
                best = m;
            }
        }
    }

    hlist_for_each_entry_safe(mc, n, &c->list, list){
        
        for(i=0, j=0; i<mc->cnt; j++){
            m=&mc->m[j];
            if(m->is_used){
                i++;

                if( unlikely(!m->is_up) ){
                    continue;
                }
                m->current_weight += m->effective_weight;
                total += m->effective_weight;

                if( m->effective_weight < m->weight ){
                    m->effective_weight++;
                }

                if ((best == NULL) || (m->current_weight > best->current_weight)) {
                    best = m;
                }                
            }
        }
    }

    if( best != NULL ){
        best->current_weight -= total;
    }

    return best;
}

static al_member_t* al_alg_weight(al_member_chain_t* c, void* args){
    return al_alg_rr(c, args);
}

static al_member_t* al_alg_rr_weight(al_member_chain_t* c, void* args){
    return al_alg_rr(c, args);
}

static al_member_t* al_alg_geoip(al_member_chain_t* c, void* args){

    int i,  j;
    al_member_t* m, *best;
    struct hlist_node* n;
    al_member_chain_node_t* mc;
    al_margs* mptr=args;

    best=NULL;
    
    for(i=0, j=0; i<c->cnt; j++){
        m=&c->m[j];
        if(m->is_used){
            
            i++;
            
            if(unlikely(!m->is_up)){
                continue;
            }

            if( m->viewid == mptr->vid ){
                best=m;
                goto END;
            }

            if (unlikely(best == NULL)) {
                best = m;
            }
        }
    }

    hlist_for_each_entry_safe(mc, n, &c->list, list){
        
        for(i=0, j=0; i<mc->cnt; j++){
            m=&mc->m[j];
            if(m->is_used){
                i++;

                if( unlikely(!m->is_up) ){
                    continue;
                }

                if( m->viewid == mptr->vid ){
                    best=m;
                    goto END;
                }                
                
                if (unlikely(best == NULL)) {
                    best = m;
                }
            }
        }

    }

END:
    if( likely(best != NULL) ){
        mptr->query_type = best->intype;
        mptr->ttl=best->dnsttl;
        
        if( best->intype == DNS_Q_A ){
            mptr->data_len=4;
            *((uint32_t*)*mptr->data)=al_addr_addr(best->mb_address4);
        }else if( best->intype == DNS_Q_AAAA ){
            mptr->data_len=16;
            *((uint128_t*)*mptr->data)=al_addr_addr(best->mb_address6);
        }else if( (best->intype == DNS_Q_NS ) || ( best->intype == DNS_Q_CNAME ) ){
            mptr->data_len=best->ext[0];
            memcpy(*mptr->data, &best->ext[1], mptr->data_len);
        }else{
            LOG(LOG_INFO, LAYOUT_CONF, "Invalid memeber type, %d", best->intype);
        }
    }
    return best;    
}

static void al_tree_walk(al_rbtree_node_t* t, al_rbtree_node_t* sentinel, FILE* fp){
    al_member_node_t* mn;
    al_member_chain_t* mc;
    al_member_chain_node_t* c;
    al_member_t* m;
    struct hlist_node* n;
    char buf[128];
    int i, j;
    if( t == sentinel){
        return;
    }else{
        
        al_tree_walk(t->left, sentinel, fp);
        al_tree_walk(t->right, sentinel, fp);
        //output t
        mn=(al_member_node_t*)&t->color;
        fprintf(fp,"Filter id %d gid %d Schd:%d \n", mn->fid, mn->gid, mn->schd);
        mc=(al_member_chain_t*)mn->data;

        j=0;
        for(i=0, j=0; i<mc->cnt; j++){
            m=&mc->m[j];
            if( !m->is_used ){
                continue;
            }
            fprintf(fp, "    %8d ", i++);
            fprintf(fp, "%8d", m->mid);
            fprintf(fp, "%8d", m->is_used);
            fprintf(fp, "%8d", m->is_up);
            fprintf(fp, "%8d", ntohs(m->port));
            fprintf(fp, "%10d", m->weight);
            if(al_addr_af(m->mb_address)==AF_INET){
                fprintf(fp, " %s\n", inet_ntop(al_addr_af(m->mb_address), al_p_addr_addr_ptr((al_addr4_t*)&m->mb_address), buf, 128));    
            }else{
                fprintf(fp, " %s\n", inet_ntop(al_addr_af(m->mb_address), al_p_addr_addr_ptr((al_addr6_t*)&m->mb_address), buf, 128));
            }            
        }

        hlist_for_each_entry_safe(c, n, &mc->list, list){
            for(i=0, j=0; i<c->cnt; j++){
                m=&c->m[j];
                if(!m->is_used){
                    continue;
                }
                fprintf(fp, "    %8d ", i++);
                fprintf(fp, "%8d", m->mid);
                fprintf(fp, "%8d", m->is_used);
                fprintf(fp, "%8d", m->is_up);
                fprintf(fp, "%8d", ntohs(m->port));
                fprintf(fp, "%10d", m->weight);
                if(al_addr_af(m->mb_address)==AF_INET){
                    fprintf(fp, " %s\n", inet_ntop(al_addr_af(m->mb_address), al_p_addr_addr_ptr((al_addr4_t*)&m->mb_address), buf, 128));    
                }else{
                    fprintf(fp, " %s\n", inet_ntop(al_addr_af(m->mb_address), al_p_addr_addr_ptr((al_addr6_t*)&m->mb_address), buf, 128));
                }
            }
        }
    }
}

void al_dump_member_api(FILE* fp){
    rte_rwlock_read_lock(&g_members_lock);
    al_tree_walk(g_members.rbtree.root, &g_members.sentinel, fp);
    rte_rwlock_read_unlock(&g_members_lock);
}

const char* al_group_alg(uint8_t alg){
    switch(alg){
    case AL_ALG_RR:
        return "Round robin";
    case AL_ALG_WEIGHT:
        return "Weight";
    case AL_ALG_RR_WEIGHT:
        return "Round robin and wight";
    case AL_ALG_GEOIP:
        return "GeoIP";
    default:
        return NULL;
    }
}


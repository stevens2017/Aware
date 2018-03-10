#ifndef _AL_FILTER_H_
#define _AL_FILTER_H_

#define FILTER_CLUSTER_SIZE 8

typedef struct al_filter{
    uint32_t  fid;
    uint32_t  gid;
    uint16_t is_used:1;
    uint16_t type:8;
    uint16_t  :7;
    uint16_t port;
    uint8_t   proto;
}al_filter_t;

typedef struct al_filter_node{
    uint8_t   color;
    uint8_t    data[0];
}al_filter_node_t;

typedef struct al_filter_chain{
    struct hlist_head list;
    uint16_t             nums;
    uint16_t             cnt;
    al_filter_t          m[FILTER_CLUSTER_SIZE];
}__attribute__((__packed__))al_filter_chain_t;

typedef struct al_filter_chain_node{
    struct hlist_node list;
    uint16_t             cnt;
    al_filter_t          m[FILTER_CLUSTER_SIZE];
}__attribute__((__packed__))al_filter_chain_node_t;

typedef struct al_filter_args{
    uint32_t fid;
    uint32_t gid;
    uint8_t   ftype;
}al_fargs;

#define INIT_FC(f, s, i)                          \
    for(i=s;i>0; i--) f[i-1].is_used = 0

static inline void al_filter_chain_release(al_filter_chain_t* f){
    struct hlist_node* n;
    al_filter_chain_node_t* fc;
    hlist_for_each_entry_safe(fc, n, &f->list, list){
        al_free(fc);
    }
}

int al_filter_init(void);
int al_filter4_search(uint32_t dst, uint16_t port, uint8_t proto, al_fargs* fargs);
int al_filter6_search(uint128_t dst, uint16_t port, uint8_t proto, al_fargs* fargs);
#endif /*_AL_FILTER_H_*/


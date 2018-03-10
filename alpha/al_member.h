#ifndef _AL_MEMBER_H_
#define _AL_MEMBER_H_

#define MEMBER_CLUSTER_SIZE 8

typedef struct al_member al_member_t;

struct al_member{
    uint16_t is_used:1;
    uint16_t is_up:1;
    uint16_t  :14;
    uint16_t port;
    uint16_t weight;
    uint16_t current_weight;
    uint16_t effective_weight;
    uint32_t mid;
    /*geoip index*/
    union{
        struct{
            uint32_t vid;
            uint32_t ttl;
            /*cname, a, aaaa, ns*/
            uint8_t   type;
        }dns;
    }data;
    
    union{
        al_addr_t    addr;
        al_addr4_t   addr4;
        al_addr6_t   addr6;
    }addr;
    
    uint8_t    ext[0];
}__attribute__((__packed__));

#define viewid data.dns.vid
#define dnsttl  data.dns.ttl
#define intype data.dns.type
#define dnslen data.dns.dlen

#define mb_address addr.addr
#define mb_address4 addr.addr4
#define mb_address6 addr.addr6

typedef struct al_member_node{
    uint8_t   color;
    uint8_t   schd;
    uint32_t  fid;
    uint32_t  gid;
    uint8_t    data[0];
}al_member_node_t;

typedef struct al_member_chain{
    struct hlist_head list;
    uint16_t             cnt;
    al_member_t      m[MEMBER_CLUSTER_SIZE];
}__attribute__((__packed__))al_member_chain_t;

typedef struct al_member_chain_node{
    struct hlist_node list;
    uint16_t             cnt;
    al_member_t      m[MEMBER_CLUSTER_SIZE];
}__attribute__((__packed__))al_member_chain_node_t;

typedef struct al_member_margs{
    uint32_t mid;
    al_address_buf addr;
    uint16_t port;
    uint32_t vid;
    uint32_t ttl;
    uint8_t   query_type;
    uint8_t** data;
    uint8_t     data_len;
    uint8_t     schd;
}al_margs;

#define INIT_MC(m, s, i)                          \
    for(i=s;i>0; i--) m[i-1].is_used = 0

static inline void al_member_chain_release(al_member_chain_t* f){
    struct hlist_node* n;
    al_member_chain_node_t* mc;
    hlist_for_each_entry_safe(mc, n, &f->list, list){
        al_free(mc);
    }
}

int al_members_init(void);
int al_member_lookup(uint32_t fid, uint32_t gid, al_margs* margs);

#endif /*_AL_MEMBER_H_*/

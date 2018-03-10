#ifndef _ROUTE_H_
#define _ROUTE_H_

#include <sys/socket.h>

#include "hlist.h"
#include "al_ip.h"
#include "al_radix.h"
#include "al_common.h"
#include "al_to_api.h"

enum{
    LLB_NO=0,
    LLB_YES,
    LLB_EDN
};

static inline int is_same_subnet(ipcs_t* ip, ipcs_t* tip, ipcs_t* mask, uint8_t iptype){

    if( iptype == AF_INET ){
        return ((ip->ipv4&mask->ipv4)==(tip->ipv4&mask->ipv4) ? 1 : 0);
    }else{
        return ((ip->ipv6&mask->ipv6)==(tip->ipv6&mask->ipv6) ? 1 : 0);
    }
    return 1;
}

typedef struct al_router{
    struct	radix_node nodes[2];
    al_addr4_t addr;
    al_addr4_t prefix;
    al_addr4_t nexthop;

    uint64_t iptype:8;
    uint64_t rtype:8;/* 0 static, 1 dynamic*/
    uint64_t id:16;    
    uint64_t oif:8;
    uint64_t      :24;
}__attribute__((__packed__))al_router_t;

typedef struct al_router6{
    struct	radix_node nodes[2];
    al_addr6_t addr;
    al_addr6_t prefix;
    al_addr6_t nexthop;

    uint64_t iptype:8;
    uint64_t rtype:8;/* 0 static, 1 dynamic*/
    uint64_t id:16;    
    uint64_t oif:8;
    uint64_t      :24;
}__attribute__((__packed__))al_router6_t;

int al_router_init(void);
int al_router_find(uint8_t* src, uint8_t* dst,  uint8_t iptype, uint8_t* nexthop, uint8_t* oif);

#endif /*_ROUTE_H_*/


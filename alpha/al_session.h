#ifndef __SESSION_H__
#define __SESSION_H__

#include <rte_atomic.h>
#include <rte_ether.h>
#include "al_common.h"
#include "hlist.h"
#include "al_ip.h"
#include <rte_mbuf.h>

#define  MAX_SESSION_COUNTER   (1<<16)

#define SESSION_IDLE_TIMEOUT 60*5


typedef struct session{
    struct hlist_node       list;

    /*session type*/
    uint64_t               stype:3;
    /*proxy: is upstream*/
    uint64_t               is_ups:1;
    /*tcp port already allocate*/
    uint64_t               port_alloc:1;
    /*session inet type*/
    uint64_t               proto:8;
    /*is llb dr mod, 1 dr, 0 not  or is slb  gw mode, 1 gw mode, 0 is not*/
    uint64_t              is_dr:1;
    /*filter type*/
    uint64_t              ftype:8;
    /*Internet poto, AF_INET = 0, AF_INET6 = 1*/
    uint64_t               in_type:1;
    uint64_t               portid:8;
    uint64_t               stout:12;
    uint64_t                       :21;

    struct session*        up;

    ipcs_t local_addr;
    ipcs_t remote_addr;
    uint16_t local_port;
    uint16_t remote_port;

    struct ether_addr to_haddr;
    struct ether_addr from_haddr;
       
    uint32_t              fid;
    uint32_t              mid;
    /*unimplement*/
    uint32_t              arpid;

    /*session handler*/
    void (*handler)(struct session* s, struct rte_mbuf* buf, void* iphdr, void* tcphdr);
}__attribute__((__packed__))session_t;

#define SESSION_NORMAL   0x1
#define SESSION_PROXY    0x2

#define SESSION_MBUF_CACHE_LEN_MAX 10

int session_init(void);
int session_thread_init(void);
session_t* al_session_create(void);
void al_session_free(session_t* s);
void al_session_timeout(void);

session_t* tcp4_session_get(uint32_t rip, uint16_t rport, uint32_t lip, uint16_t lport, uint8_t proto_type);
session_t* tcp6_session_get(uint128_t rip, uint16_t rport, uint128_t lip, uint16_t lport, uint8_t proto_type);

void al_session_add(session_t* s);
void al_output_thread_session(void);

#define LISTEN_KEY(ip, port, proto, mask)  (((ip)^(port)^(proto)) & (mask))
#define PCB_KEY(rip, rport, lip, lport, proto, mask)    \
    pcb4_key(rip, rport, lip, lport, proto, mask)

static inline uint32_t pcb4_key(uint32_t rip, uint16_t rport, uint32_t lip, uint16_t lport, uint8_t proto, uint32_t mask){
    uint32_t rkey=(rip) ^ (rip>>16);
    uint32_t lkey=(lip) ^ (lip>>16);
    return ((((rkey)^(lkey))^((lport)^(rport))^(proto))&(mask));
}

static inline uint32_t pcb6_key(__uint128_t rip, uint16_t rport, __uint128_t lip, uint16_t lport, uint8_t proto, uint32_t mask){
    __uint128_t rkey=rip^lip;
    uint32_t* lkey=(uint32_t*)&rkey;
    return PCB_KEY(*lkey, rport, *(lkey+1), lport, proto, mask);
}


#endif /*__SESSION_H__*/



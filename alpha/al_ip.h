#ifndef _IP_H_
#define _IP_H_

#include <rte_atomic.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <rte_ip.h>
#include "hlist.h"
#include "al_common.h"

#define AL_IPV4_PREFIX32  0xffffffff

#define AL_IP_DEFAULT_TTL 64

#define AL_IP_HEADER_LEN_DEFAULT 5

#define AL_IP_V4_VERSION 0x0100
#define AL_IP_V6_VERSION 0x0110

#define AL_IP_SET_IPV4(header)  (header) |= (AL_IP_V4_VERSION << 4)
#define AL_IP_SET_IPV6(header)  (header) |= (AL_IP_V6_VERSION << 4)

#define AL_SET_TTL_IPV4(iphdr, ttl) (iphdr)->next_proto_id = ttl
#define AL_SET_TTL_IPV6(iphdr, ttl) (iphdr)->hop_limits = ttl
#define AL_SET_DEFAULT_TTL_IPV4(iphdr) (iphdr)->next_proto_id = AL_IP_DEFAULT_TTL
#define AL_SET_DEFAULT_TTL_IPV6(iphdr) (iphdr)->hop_limits = AL_IP_DEFAULT_TTL

typedef union address_t{
    __uint128_t ipv6;
    uint32_t      ipv4;
}ipcs_t;

#define address4  ipv4
#define address6  ipv6

typedef uint32_t ipaddr_t[4];

extern const ipaddr_t al_ip_any;

#define AL_IPADDR_ANY  al_ip_any

#define AL_ADDR_SET_ANY(addr) memset((uint32_t*)addr, '\0',  4)

#define ip6_addr_ismulticast(ip6addr) ((((uint32_t*)&ip6addr)[0] & rte_cpu_to_be_32(0xff000000UL)) == rte_cpu_to_be_32(0xff000000UL))
        
#define IPV6_IS_ANY(ip1) \
        (*(__uint128_t*)(ip1) == 0)

#define IPV4_IS_ANY(ip1) \
	*((uint32_t*)ip1) == 0

#define IPV4_UINT32(ip)  (*((uint32_t*)ip))

#define IPV4_UINT32_PTR(ip)  ((uint32_t*)ip)


#define IPV4_COPY(dst, src) \
    *((uint32_t*)(dst)) = *((uint32_t*)(src))
#define IPV6_COPY(dst, src) \
	memcpy((uint32_t*)dst, (uint32_t*)src, 4)

#define IPV4_NETMASK(prefix) \
	htonl(~(0xFFFFFFFF>>(prefix)))

typedef struct ipnode{
   struct hlist_node list;
   ipcs_t       ip;
   ipcs_t       mask;
   rte_atomic16_t ref;
    uint64_t       id;
   uint16_t       iptype:8; /*AF_INET ipv4, AF_INET6 ipv6*/
   uint16_t       asstype:3; /*0 static, 1 dhcp, 2 vip, 3 float*/
   uint16_t              :5;
}__attribute__((__packed__))ip_t;


#define IPV4(ip)  ip[0]
#define IPV6(ip)  ip

#define TOIPADDR(ip) ((uint32_t*)(ip))

#define AL_MAX_TTL  255

#define IP_KEY(ip, iptype, mask) \
	iptype == 0 ? (IPV4(ip) ^ (IPV4(ip)>>16)) & mask :  \
	  (((IPV6(ip)[0]^IPV6(ip)[1]^IPV6(ip)[2]^IPV6(ip)[3])) ^((IPV6(ip)[0]^IPV6(ip)[1]^IPV6(ip)[2]^IPV6(ip)[3]))>>16)) & mask;

ip_t* ip_get(ipcs_t* ip, const char* mask, uint8_t iptype);
ip_t* ip_get_with_mask(ipcs_t* ip, ipcs_t* mask, uint8_t iptype, uint8_t asstype);
void ip_put(ip_t* ip);
int  ip_pool_init(void);
int  ip_genmask(ipcs_t* ip, uint8_t iptype, uint8_t prefix);
void ip_cmd_output(FILE* fp, char* pfx_tab, ip_t* ip);
void al_dump_ip(FILE* fp, char* pfx_tab, ip_t* ip);

static inline void al_addr_ntoh(uint8_t iptype, const ipaddr_t src,  ipaddr_t dst)
{
    const uint32_t* sptr=src;
            uint32_t* dptr=dst;

    if( iptype == AF_INET ){
        dptr[0] = ntohl(sptr[0]);
    }else{
        dptr[3] = ntohl(sptr[0]);
        dptr[2] = ntohl(sptr[1]);
        dptr[1] = ntohl(sptr[2]);
        dptr[0] = ntohl(sptr[3]);        
    }
}

void al_tcp_checksum_v4(struct ipv4_hdr *pIph, uint16_t *payload);

static inline uint128_t tomask_ipv6(int prefix){
    uint128_t  data;
    uint8_t* ptr;
    int i, j;

    if( prefix == 0 ){
        return 0;
    }
    
    data=0;
    ptr=(uint8_t*)&data;
    
    for ( i = prefix, j = 0; i > 0; i -= 8, ++j)
        ptr[j] = i >= 8 ? 0xff : (uint64_t)(( 0xffU << ( 8 - i ) ) & 0xffU );
    return data;
}

static inline uint32_t tomask_ipv4(int prefix){
    return prefix == 0 ? 0 : htonl((~((uint32_t)0))<<(32-prefix));
}

#endif /*_IP_H_*/


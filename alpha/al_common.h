#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <netinet/in.h>

#define MAX_ETHER_COUTER 0x40
    
#define PROTO_TYPE_TCP IPPROTO_TCP
#define PROTO_TYPE_UDP IPPROTO_UDP

#define MAX_VLAN_CNT  0x0FFF
#define HADDR_LEN     6

#define AL_ON    1
#define AL_OFF   0

/* Total octets in ethernet header */
#define KNI_ENET_HEADER_SIZE    14
/* Total octets in the FCS */
#define KNI_ENET_FCS_SIZE       4

#define STRUCT_ALIGNED(n) __attribute__((aligned(n)))


#define CACHE_ALIGN  64

#define FLAG32(f)   ((uint32_t)(f))

extern time_t g_sys_time;
extern __thread uint8_t g_lcoreid;

#define HASHSIZE(x)   (1 << (31-__builtin_clz(x)))

#define ATOMIC_INC(x) __sync_add_and_fetch((x), 1)

#define MBUF_FLAG(m)  (m)->udata64

#define CAS __sync_bool_compare_and_swap

#define BIT_LOCK(val, c, v, locker) \
	do{ \
	    c=*(val) & (~(locker)); \
	    v=*(val) | (locker); \
		if(CAS((val), c, v)) break; \
	}while(1)

#define BIT_UNLOCK(val, c, v, locker) \
	do{ \
		c=*(val) & (~(locker)); \
	    v=*(val) | (locker); \
		if(CAS((val), v, c)) break; \
	}while(1)

#define AL_LOCK(lock, t) \
    do{   \
        t=(lock); \
        if(CAS(&(lock), t, t+1)) ; \
    }while(1)

#define AL_UNLOCK(lock, t) \
    do{   \
        t=(lock); \
        if(CAS(&(lock), t, t+1)) ; \
    }while(1)
    

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_##x
#else
#  define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

#define AL_UNUSED_ARG(x) (void)x

#define TRACE_INFO LOG(LOG_TRACE, LAYOUT_TCP, "File:%s Line:%d\n", __FILE__, __LINE__)


#define MAX_NAMELEN             32

#define RTE_AL_RX_DESC_DEFAULT 256
#define RTE_AL_TX_DESC_DEFAULT 512

#define ETHER_ADDR_FORMART  "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHER_ADDR(addr)   (addr)->addr_bytes[0],(addr)->addr_bytes[1], (addr)->addr_bytes[2], (addr)->addr_bytes[3], (addr)->addr_bytes[4], (addr)->addr_bytes[5]

#define IPV4_IP_FORMART "%u.%u.%u.%u"
#define IPV4_DATA_READABLE(ip)   ((uint8_t*)(ip))[0],((uint8_t*)(ip))[1],((uint8_t*)(ip))[2],((uint8_t*)(ip))[3]

#define IPV6_IP_FORMART "%x:%x:%x:%x:%x:%x:%x:%x"
#define IPV6_DATA_READABLE(ptr) (ntohl(*(uint32_t*)ptr) >> 16) & 0xffff, ntohl(*(uint32_t*)ptr) & 0xffff, \
                               (ntohl(*((uint32_t*)ptr+1)) >> 16) & 0xffff, ntohl(*((uint32_t*)ptr+1)) & 0xffff, \
                               (ntohl(*((uint32_t*)ptr+2)) >> 16) & 0xffff, ntohl(*((uint32_t*)ptr+2)) & 0xffff, \
                               (ntohl(*((uint32_t*)ptr+3)) >> 16) & 0xffff, ntohl(*((uint32_t*)ptr+3)) & 0xffff


#define CONSTRUCT_FUN __attribute__ ((constructor))

#define DEFAULT_CONFIG_FILE "/etc/aware_dev.conf"
#define DEFUALT_CONFIG_LB_FILE "/etc/aware_lb.conf"
#define DEFUALT_CONFIG_OBJLIST_FILE "/etc/aware_obj_list.conf"

#define PAGESIZE 4096

#define IPV6_HLEN 40

#define al_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define al_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

    extern int mbuf_size;

    typedef __uint128_t uint128_t;
    typedef __int128_t int128_t;

    typedef uint64_t al_uint_t;


#define SWAP_INT(a, b)  (a)=(a)^(b); (b)=(a)^(b); (a)=(a)^(b)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__COMMON_H__*/



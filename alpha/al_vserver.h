#ifndef _AL_VSERVER_H_
#define _AL_VSERVER_H_

#include <al_ip.h>
#include <common/al_string.h>
#include "al_gslb.h"

#define AL_VSERVER_MAX_COUNTER   1024


#define VSERVER_USED_FLAG   0x01
#define VSERVER_IPV4_FLAG   0x02
#define VSERVER_VSERVER_FLAG 0x04


typedef struct al_vserver_ctx al_vserver_ctx_t;

struct al_string;

struct al_vserver_ctx{
    uint8_t  type;
    union{
        struct{
            struct al_string domain;
            uint16_t          id;
        }glsb;
    }data;
};

typedef struct al_vserver al_vserver_t;

struct al_vserver{
    uint16_t          id;
    uint16_t          parent;
    uint16_t          flags:8;
    uint16_t          is_llb_dr:1;
    uint16_t          :7;
    uint8_t           srv_type;
    uint16_t          vsid;
    uint16_t          pool; 
    uint16_t          port_start;
    uint16_t          port_end;
    ipaddr_t          addr_start;
    ipaddr_t          addr_end;
    uint8_t            domain[AL_DOMAIN_NAME_LEN];
}__attribute__((__packed__));

#define VS_DOMAIN_NAME(vs) (vs)->domain
#define VS_GLSB_VSID(vs)   (vs)->port_start

#define IS_VSERVER_USED(vs)                     \
    ((vs)->flags & VSERVER_USED_FLAG)


#define SET_VSERVER_USED(vs)                  \
    (vs)->flags |= VSERVER_USED_FLAG

#define SET_VSERVER_UNUSED(vs)             \
    (vs)->flags &= ~VSERVER_USED_FLAG

#define IS_VS_IPV4(vs)                          \
    ((vs)->flags & VSERVER_IPV4_FLAG)


#define SET_VS_IPV4(vs)                         \
    (vs)->flags |= VSERVER_IPV4_FLAG

#define IS_VSERVER(vs)                          \
    ((vs)->flags & VSERVER_VSERVER_FLAG)

#define SET_VSERVER(vs)                         \
    ((vs)->flags |= VSERVER_VSERVER_FLAG)

int vserver_init(void);
al_vserver_t* vserver_find(uint32_t* ip, uint16_t port, uint8_t iptype, uint8_t is_exclude_llb);
int al_vserver_find(uint32_t* ip, uint16_t port, uint8_t iptype, uint8_t is_exclude_llb, al_vserver_ctx_t* ctx, al_vserver_t* ovs);
int vserver_find_rs(uint16_t vsid, ipaddr_t ip, uint8_t* iptype, uint16_t* port);
int vserver_is_llb_dr(uint16_t vid);

#endif /*_AL_VSERVER_H_*/


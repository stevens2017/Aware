#ifndef _GEOIP_H_
#define _GEOIP_H_

#include <sys/socket.h>

#include "hlist.h"
#include "al_ip.h"
#include "al_radix.h"
#include "al_common.h"
#include "al_to_api.h"

typedef struct al_geoip{
    struct	radix_node nodes[2];
    al_addr4_t addr;
    al_addr4_t prefix;
    uint32_t    id;    
}__attribute__((__packed__))al_geoip_t;

typedef struct al_geoip6{
    struct	radix_node nodes[2];
    al_addr6_t addr;
    al_addr6_t prefix;
    uint32_t    id;    
}__attribute__((__packed__))al_geoip6_t;

int al_geoip_init(void);
int al_geoip_find(uint32_t addr, uint32_t* id);
int al_geoip6_find(uint128_t addr, uint32_t* id);

#endif /*_GEOIP_H_*/


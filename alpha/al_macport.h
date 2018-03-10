#ifndef __AL_MAC_PORT_H__
#define __AL_MAC_PORT_H__

#include <stdio.h>
#include <rte_ether.h>
#include "hlist.h"

struct mac_port{
    struct hlist_node list;
    struct ether_addr addr;
    uint8_t           port;
    time_t            time;
}__attribute__((packed));

int init_macport(void);
void portid_clear(void);
uint8_t portid_lookup(struct ether_addr* mac);
void portid_update(struct ether_addr* mac, uint8_t portid);

#endif /*__AL_MAC_PORT_H__*/


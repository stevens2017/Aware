#ifndef __ARP_H__
#define __ARP_H__

#include <rte_ether.h>
#include "hlist.h"
#include "al_ip.h"
#include "al_dev.h"
#include "al_session.h"

#define HWTYPE_ETHERNET 1

typedef struct arp{
   struct hlist_node  list;
   struct ether_addr  haddr;
   ipcs_t               ipaddr;
   uint64_t           ttl:16;
   uint64_t           portid:16;
    /*ip type*/
   uint64_t           iptype:8;
   uint64_t           padding:24;
}__attribute__((__packed__)) arp_t;

#define HADDR_SET_MULTI_ADDR(addr) \
	(addr)->addr_bytes[0] = 0xff;\
	(addr)->addr_bytes[1] = 0xff;\
	(addr)->addr_bytes[2] = 0xff;\
	(addr)->addr_bytes[3] = 0xff;\
	(addr)->addr_bytes[4] = 0xff;\
	(addr)->addr_bytes[5] = 0xff

int arp_init(void);
int  al_arp_input(struct rte_mbuf *p);

void arp_input(uint8_t portid, struct rte_mbuf* p);
int arp_search(uint32_t addr, struct ether_addr* haddr, al_dev *dev);
int ndp_search(uint128_t addr, struct ether_addr* haddr, al_dev *dev);

int arp_add_entry(uint32_t ipaddr, struct ether_addr* haddr, uint16_t portid);
int ndp_add_entry(uint128_t ipaddr, struct ether_addr* haddr, uint16_t portid);

struct ether_addr * al_ether_aton(const char *a);
uint8_t etharp_request(al_dev *dev, uint32_t ipaddr);


#endif /*__ARP_H__*/

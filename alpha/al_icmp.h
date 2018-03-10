#ifndef __AL_ICMP_H__
#define __AL_ICMP_H__

#include <rte_ip.h>
#include <inttypes.h>
#include "al_dev.h"

#define ICMP_OPTION_LEN 8

struct icmp6_hdr {
	uint8_t	icmp6_type;	/* type field */
	uint8_t	icmp6_code;	/* code field */
	uint16_t	icmp6_cksum;	/* checksum field */
	union {
		uint32_t	icmp6_un_data32[1]; /* type-specific field */
		uint16_t	icmp6_un_data16[2]; /* type-specific field */
		uint8_t	    icmp6_un_data8[4];  /* type-specific field */
	} icmp6_dataun;
}__attribute__((__packed__));

struct ns_header {
  uint8_t type;
  uint8_t code;
  uint16_t chksum;
  uint32_t reserved;
  uint128_t target_address;
}__attribute__((__packed__));

struct na_header {
  uint8_t type;
  uint8_t code;
  uint16_t chksum;
  uint8_t flags;
  uint8_t reserved[3];
  uint128_t target_address;
}__attribute__((__packed__));

struct lladdr_option {
  uint8_t type;
  uint8_t length;
  struct ether_addr addr;
}__attribute__((__packed__));

#define NEIGHBOR_SOLICIT		135	/* neighbor solicitation */
#define NEIGHBOR_ADVERT		    136	/* neighbor advertisement */

void eth_nh_request(al_dev *dev, uint128_t ipaddr);
void al_icmp6_proc(struct rte_mbuf* buf);

#endif /*__AL_ICMP_H__*/


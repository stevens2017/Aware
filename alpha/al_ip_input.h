#ifndef __AL_IP_H__
#define __AL_IP_H__

#include <rte_mbuf.h>
#include <rte_ip.h>
#include "al_radix.h"
#include "al_session.h"
#include "al_filter.h"

struct tcp_addr {
	uint16_t src_port;  /**< TCP source port. */
	uint16_t dst_port;  /**< TCP destination port. */
}__attribute__((__packed__));

#define AL_PKT_FROM_NIC   0x1
#define AL_PKT_FROM_QUEUE 0x2


void al_ipv4_input(struct rte_mbuf* buf, struct ipv4_hdr* hdr, void* nextptr, uint8_t from, uint8_t type);
void al_ipv6_input(struct rte_mbuf* buf, struct ipv6_hdr* hdr, void* nextptr, uint8_t from, uint8_t type);
void al_llb_input(al_filter_t* f, session_t* s, struct rte_mbuf* buf, void* hdr, void* nextptr);

#endif /*__AL_IP_H__*/


#include <rte_common.h>
#include <rte_mbuf.h>
#include <assert.h>
#include <rte_ether.h>
#include <rte_arp.h>
#include <netinet/in.h>
#include "alpha.h"
#include "al_common.h"
#include "al_vport.h"
#include "al_hash.h"
#include "al_log.h"
#include "al_icmp.h"
#include "al_status.h"
#include "al_to_api.h"
#include "al_mempool.h"
#include "al_arp.h"

#define ETHTYPE_ARP       0x0806U
#define ETHTYPE_IP        0x0800U

/** ARP message types (opcodes) */
#define ARP_REQUEST 1
#define ARP_REPLY   2

#ifndef ETHARP_HWADDR_LEN
#define ETHARP_HWADDR_LEN     6
#endif

/** the ARP message, see RFC 826 ("Packet format") */
struct etharp_hdr {
   uint16_t hwtype;
   uint16_t proto;
   uint8_t  hwlen;
   uint8_t  protolen;
   uint16_t opcode;
   struct ether_addr shwaddr;
   uint32_t sipaddr;
   struct ether_addr dhwaddr;
   uint32_t dipaddr;
} __attribute__((packed));



#define ETH_PAD_SIZE 0
#define SIZEOF_ETH_HDR (14 + ETH_PAD_SIZE)
#define SIZEOF_ETHARP_HDR 28
#define SIZEOF_ETHARP_PACKET (SIZEOF_ETH_HDR + SIZEOF_ETHARP_HDR)


const struct ether_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
const struct ether_addr ethzero = {{0,0,0,0,0,0}};

#define ARP_TABLE_COUNTER         2048

static struct hash_table_lock arp_table[HASHSIZE(ARP_TABLE_COUNTER)];
static struct hash_table_lock ndp_table[HASHSIZE(ARP_TABLE_COUNTER)];

static uint16_t    arp_mask;

#define ARP_KEY_DATA(ipaddr) (((ipaddr) & 0xFFFF) ^ (((ipaddr) >> 16) & 0xFFFF))
#define ARP_KEY(ipaddr) (ARP_KEY_DATA(ipaddr) & arp_mask)
#define NDP_KEY(ipaddr) ((ARP_KEY_DATA(ipaddr&0xFFFFFFFF)^(ARP_KEY_DATA((ipaddr>>32)&0xFFFFFFFF)))&arp_mask)

static int etharp_update_arp_entry(uint16_t portid, uint32_t ipaddr, struct ether_addr *ethaddr);

int arp_init(void){

    int i=0;
	
    arp_mask = HASHSIZE(ARP_TABLE_COUNTER)-1;

    for(i=0; i<HASHSIZE(ARP_TABLE_COUNTER); i++){
        rte_rwlock_init(&arp_table[i].lock);
        INIT_HLIST_HEAD(&arp_table[i].list);
    }

    for(i=0; i<HASHSIZE(ARP_TABLE_COUNTER); i++){
        rte_rwlock_init(&ndp_table[i].lock);
        INIT_HLIST_HEAD(&ndp_table[i].list);
    }
    
    return 0;
}

int arp_search(uint32_t addr, struct ether_addr* haddr, al_dev *dev){

    uint16_t key;
    struct hash_table_lock* table;
    arp_t* pos;
    int rc = -1;

    key=ARP_KEY(addr);
    table=&arp_table[key];

    rte_rwlock_read_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if(addr == pos->ipaddr.address4 ){
            rc=pos->portid;
            ether_addr_copy(&pos->haddr, haddr);
            break;
        }

    } 
    rte_rwlock_read_unlock(&table->lock);

    if(unlikely(rc == -1)){
        if( dev != NULL ){
            etharp_request(dev, addr);
        }else{
            SS_INCR(arp_search_failed);
        }
    }
    
    return rc;
}

int ndp_search(uint128_t addr, struct ether_addr* haddr, al_dev *dev){

    uint16_t key;
    struct hash_table_lock* table;
    arp_t* pos;
    int rc = -1;

    key=NDP_KEY(addr);
    table=&ndp_table[key];

    rte_rwlock_read_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if(addr == pos->ipaddr.address6 ){
            rc=pos->portid;
            ether_addr_copy(&pos->haddr, haddr);
            break;
        }
    } 
    rte_rwlock_read_unlock(&table->lock);

    if(unlikely(rc == -1)){
        if( dev != NULL ){
            eth_nh_request(dev, addr);
        }else{
            SS_INCR(ndp_search_failed);
        }
    }

    return rc;
}

int arp_add_entry(uint32_t ipaddr, struct ether_addr* haddr, uint16_t portid){

    uint16_t key=ARP_KEY(ipaddr);
    struct hash_table_lock* table=&(arp_table[key]);
    arp_t* pos;
    int rc = 0;

    rte_rwlock_write_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if(ipaddr == pos->ipaddr.address4 ){
            rc=1;
            pos->portid = portid;
            ether_addr_copy(haddr, &pos->haddr);
            break;
        }
    }

    if(likely((rc != 1) && ((pos=al_malloc(sizeof(arp_t))) != NULL))){
        hlist_add_head(&pos->list, &table->list);
        pos->ipaddr.address4=ipaddr;
        pos->haddr = *haddr;
        pos->portid = portid;
        pos->iptype = AF_INET;
        rc =1;
    }
	
    rte_rwlock_write_unlock(&table->lock);
    return rc;
}

int ndp_add_entry(uint128_t ipaddr, struct ether_addr* haddr, uint16_t portid){

    uint16_t key=NDP_KEY(ipaddr);
    struct hash_table_lock* table=&(ndp_table[key]);
    arp_t* pos;
    int rc = 0;

    rte_rwlock_write_lock(&table->lock);
    hlist_for_each_entry(pos, &table->list, list){
        if(ipaddr == pos->ipaddr.address6 ){
            rc=1;
            pos->portid = portid;
            ether_addr_copy(haddr, &pos->haddr);
            break;
        }
    }

    if(likely((rc != 1) && ((pos=al_malloc(sizeof(arp_t))) != NULL))){
        hlist_add_head(&pos->list, &table->list);
        pos->ipaddr.address6=ipaddr;
        pos->haddr = *haddr;
        pos->portid = portid;
        pos->iptype = AF_INET;
        rc =1;
    }
	
    rte_rwlock_write_unlock(&table->lock);
    return rc;
}


void al_output_arp(FILE* fp){

    uint16_t i;
    struct hash_table_lock* table;
    arp_t* pos;

    for(i=0; i<HASHSIZE(ARP_TABLE_COUNTER); i++){
        table=&(arp_table[i]);
        rte_rwlock_read_lock(&table->lock);
        hlist_for_each_entry(pos, &table->list, list){
            fprintf(fp, ETHER_ADDR_FORMART" - "IPV4_IP_FORMART"\n", ETHER_ADDR(&pos->haddr), IPV4_DATA_READABLE(((uint8_t*)&pos->ipaddr)));
        } 
        rte_rwlock_read_unlock(&table->lock);
    }

    for(i=0; i<HASHSIZE(ARP_TABLE_COUNTER); i++){
        table=&(ndp_table[i]);
        rte_rwlock_read_lock(&table->lock);
        hlist_for_each_entry(pos, &table->list, list){
            fprintf(fp, ETHER_ADDR_FORMART" - "IPV6_IP_FORMART"\n", ETHER_ADDR(&pos->haddr), IPV6_DATA_READABLE(((uint8_t*)&pos->ipaddr)));
        } 
        rte_rwlock_read_unlock(&table->lock);
    }
    
}

int al_arp_input(struct rte_mbuf *p)
{
    struct arp_hdr   *hdr;
    uint32_t sipaddr, dipaddr;
    vport_t* netif=&g_vport_ary[p->port].dev.vport.port;

    /* drop short ARP packets: we have to check for p->len instead of p->tot_len here
       since a struct etharp_hdr is pointed to p->payload, so it musn't be chained! */
    if (p->data_len < SIZEOF_ETHARP_PACKET) {
        return -1;
    }

    hdr = rte_pktmbuf_mtod_offset(p, struct arp_hdr *, SIZEOF_ETH_HDR);
  
    if ((hdr->arp_hrd!= rte_cpu_to_be_16(HWTYPE_ETHERNET)) ||
        (hdr->arp_hln!= ETHARP_HWADDR_LEN) ||
        (hdr->arp_pln!= sizeof(uint32_t)) ||
        (hdr->arp_pro!= rte_cpu_to_be_16(ETHTYPE_IP)))  {
        LOG(LOG_ERROR, LAYOUT_ARP, "Invalid arp type");
        return -1;
    }

    sipaddr=hdr->arp_data.arp_sip;
    dipaddr=hdr->arp_data.arp_tip;

    if( sipaddr != dipaddr ){
        if( vport_has_ip(netif, (ipcs_t*)&sipaddr, AF_INET) ){
            SS_INCR(ip_conflict);
        }else{
            return etharp_update_arp_entry(p->port, sipaddr, (struct ether_addr*)&(hdr->arp_data.arp_sha));
        }
    }else{
        if( ! vport_has_ip(netif, (ipcs_t*)&sipaddr, AF_INET)){
            return etharp_update_arp_entry(p->port, sipaddr, (struct ether_addr*)&(hdr->arp_data.arp_sha));
        }
    }
  
    return 0;
}

static int etharp_update_arp_entry(uint16_t portid, uint32_t ipaddr, struct ether_addr *ethaddr)
{
    if (ipaddr == 0 ){
        return 0;
    }

    if( arp_add_entry( ipaddr, ethaddr, portid) != 1 ){
        return 0;
    }

    return 0;
}

struct ether_addr * al_ether_aton(const char *a)
{
	int i;
	char *end;
	unsigned long o[ETHER_ADDR_LEN];
	static struct ether_addr ether_addr;

	i = 0;
	do {
		errno = 0;
		o[i] = strtoul(a, &end, 16);
		if (errno != 0 || end == a || (end[0] != ':' && end[0] != 0)){
                    return NULL;    
                }
		a = end + 1;
	} while (++i != sizeof (o) / sizeof (o[0]) && end[0] != 0);

	if (end[0] != 0)
		return NULL;

	if (i == ETHER_ADDR_LEN) {
		while (i-- != 0) {
			if (o[i] > UINT8_MAX)
				return NULL;
			ether_addr.addr_bytes[i] = (uint8_t)o[i];
		}
	} else if (i == ETHER_ADDR_LEN / 2) {
		while (i-- != 0) {
			if (o[i] > UINT16_MAX)
				return NULL;
			ether_addr.addr_bytes[i * 2] = (uint8_t)(o[i] >> 8);
			ether_addr.addr_bytes[i * 2 + 1] = (uint8_t)(o[i] & 0xff);
		}
	} else
            return NULL;

	return (struct ether_addr *)&ether_addr;
}


uint8_t  etharp_raw(uint8_t port, const struct ether_addr *ethsrc_addr,
                   const struct ether_addr *ethdst_addr,
                   const struct ether_addr *hwsrc_addr, uint32_t ipsrc_addr,
                   const struct ether_addr *hwdst_addr, uint32_t ipdst_addr,
                   const uint16_t opcode){

        struct rte_mbuf *p;
        struct ether_hdr *ethhdr;
        struct etharp_hdr *hdr;


        /* allocate a pbuf for the outgoing ARP request packet */
        if(unlikely((p = rte_pktmbuf_alloc(al_pktmbuf_pool)) == NULL)){
            return -1;
        }else{
            if(rte_pktmbuf_append(p, sizeof(struct ether_hdr)+sizeof(struct etharp_hdr))==NULL){
                return -1;
            }
        }

        ethhdr = rte_pktmbuf_mtod(p, struct ether_hdr *);
        hdr = rte_pktmbuf_mtod_offset(p, struct etharp_hdr *, SIZEOF_ETH_HDR);
        hdr->opcode = rte_cpu_to_be_16(opcode);

        /* Write the ARP MAC-Addresses */
        hdr->shwaddr = *hwsrc_addr;
        hdr->dhwaddr = *hwdst_addr;
        ethhdr->d_addr = *ethdst_addr;
        ethhdr->s_addr = *ethsrc_addr;
        /* Copy struct ip_addr2 to aligned ip_addr, to support compilers without
         * structure packing. */ 
        hdr->sipaddr = ipsrc_addr;
        hdr->dipaddr = ipdst_addr;

        hdr->hwtype = rte_cpu_to_be_16(HWTYPE_ETHERNET);
        hdr->proto = rte_cpu_to_be_16(ETHTYPE_IP);
        /* set hwlen and protolen */
        hdr->hwlen = ETHARP_HWADDR_LEN;
        hdr->protolen = sizeof(uint32_t);

        ethhdr->ether_type= rte_cpu_to_be_16(ETHTYPE_ARP);
        /* send ARP query */
        send_to_switch(g_lcoreid, port, &p, 1, 0);

        return 0;
}

 uint8_t etharp_request(al_dev *dev, uint32_t ipaddr){

        vport_t* netif;
        ipcs_t rip, lip;

        rip.address4=ipaddr;
        netif = VLAN2VPORT(dev);
        if(vport_select_local_ip(netif, &lip, AF_INET, &rip)){
            return etharp_raw(dev->port, &netif->haddr, &ethbroadcast,
                              &netif->haddr, lip.address4, &ethzero,
                              ipaddr, ARP_REQUEST);
        }else{
            return etharp_raw(dev->port, &netif->haddr, &ethbroadcast,
                              &netif->haddr, 0, &ethzero,
                              ipaddr, ARP_REQUEST);            
        }

        return 0;
}


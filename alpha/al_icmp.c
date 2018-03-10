#include <rte_ether.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_rwlock.h>
#include <rte_icmp.h>
#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include "alpha.h"
#include "al_log.h"
#include "al_vport.h"
#include "al_ip.h"
#include "al_vserver.h"
#include "al_arp.h"
#include "hlist.h"
#include "al_common.h"
#include "al_icmp.h"
#include "al_mempool.h"
#include "al_status.h"
#include "al_dev.h"

void al_icmp6_proc(struct rte_mbuf* buf){

    struct ipv6_hdr*   iphdr=rte_pktmbuf_mtod_offset(buf, struct ipv6_hdr*, sizeof(struct ether_hdr));
    struct icmp6_hdr*   icmp=rte_pktmbuf_mtod_offset(buf, struct icmp6_hdr*, sizeof(struct ether_hdr)+IPV6_HLEN);

    uint128_t     addr, saddr;
    int           s1, len;
    al_dev*       dev = &g_vport_ary[buf->port];


    addr=*(uint128_t*)iphdr->dst_addr;
    saddr=*(uint128_t*)iphdr->src_addr;

    s1=sizeof(struct ether_hdr)+IPV6_HLEN;
    len=rte_pktmbuf_data_len(buf)-s1;

    /* Check that ns header fits in packet. */
    if (unlikely(len < sizeof(struct ns_header))) {
        SS_INCR(ll_bad_pkg_len);
        goto END;
    }
    
    if(icmp->icmp6_type == NEIGHBOR_SOLICIT){

        struct lladdr_option * lladdr_opt;

        lladdr_opt = NULL;
        if (likely(len >= (sizeof(struct ns_header) + sizeof(struct lladdr_option)))) {
            lladdr_opt = rte_pktmbuf_mtod_offset(buf, struct lladdr_option *,  s1+sizeof(struct ns_header));
        }else{
            SS_INCR(ll_bad_pkg_len);            
            goto END;
        }

        if (saddr == 0) {
            goto END;
        }else {
            ndp_add_entry(saddr, &lladdr_opt->addr, buf->port);
        }
    }else if(icmp->icmp6_type == NEIGHBOR_ADVERT){
        struct na_header * na_hdr;
        struct lladdr_option * lladdr_opt;

        na_hdr = rte_pktmbuf_mtod_offset(buf, struct na_header *, s1);

        if (len < (sizeof(struct na_header) + sizeof(struct lladdr_option))) {
            SS_INCR(ll_bad_pkg_len);
            goto END;
        }
        
        if (ip6_addr_ismulticast(addr)) {
            lladdr_opt = rte_pktmbuf_mtod_offset(buf, struct lladdr_option *, s1 + sizeof(struct na_header));
            addr=na_hdr->target_address;
            ndp_add_entry(addr, &lladdr_opt->addr, buf->port);
        }else {
            addr=na_hdr->target_address;
            lladdr_opt = rte_pktmbuf_mtod_offset(buf, struct lladdr_option *, s1 + sizeof(struct na_header));
            ndp_add_entry(addr, &lladdr_opt->addr, buf->port);
        }
    }

END:
    send_to_switch(g_lcoreid, dev->port, (void*)&buf,  1, 0);
}

void eth_nh_request(al_dev *dev, uint128_t ipaddr){

    struct ether_hdr         *ehd;
    struct ipv6_hdr          *iphdr;
    struct icmp6_hdr         *icmp;
    struct na_header   *na;
    uint16_t                 *ptr;
    vport_t                  *vport;
    uint8_t                  *opt;
    ipcs_t                    ipc;
	
    struct rte_mbuf* buf=GET_RTE_MBUF();
    if( buf ){
        LOG(LOG_TRACE, LAYOUT_APP, "Alloc mbuf failed");
        rte_pktmbuf_free(buf);
        return;
    }

    ehd   = rte_pktmbuf_mtod(buf, struct ether_hdr*);
    iphdr = rte_pktmbuf_mtod_offset(buf, struct ipv6_hdr*, sizeof(struct ether_hdr));
    icmp  = rte_pktmbuf_mtod_offset(buf, struct icmp6_hdr*, sizeof(struct ether_hdr)+IPV6_HLEN);
    na = (struct na_header*)icmp;

    ipaddr = *(uint128_t*)iphdr->dst_addr;
    ipc.address6=*(uint128_t*)iphdr->src_addr;
    if(!vport_primer_ip(VLAN2VPORT(dev), &ipc, AF_INET6)){
        LOG(LOG_TRACE, LAYOUT_APP, "No ip found for port %d", dev->port);
        rte_pktmbuf_free(buf);
        return;
    }

    vport = VLAN2VPORT(dev);

    ptr=(uint16_t*)&ehd->d_addr;
    *(ptr)=0xffff;
    *(ptr+1)=0xffff;
    *(ptr+2)=0xffff;

    ehd->s_addr=*((struct ether_addr*)&vport->haddr);
    ehd->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);

    iphdr->proto = IPPROTO_ICMPV6;
    iphdr->hop_limits = AL_MAX_TTL;

    icmp->icmp6_type = NEIGHBOR_SOLICIT;
    icmp->icmp6_code = 0;
    icmp->icmp6_cksum = 0;
    na->reserved[0] = 0;
    na->reserved[1] = 0;
    na->reserved[1] = 0;
        
    na->target_address = ipaddr; 

    opt=(uint8_t*)(na+1);
    *opt = 1;
    *(opt + 1 ) = ICMP_OPTION_LEN/8;
    memcpy(opt+2, vport->haddr.addr_bytes, EHTER_ADDR_LEN);

    iphdr->payload_len = sizeof(struct na_header) + ICMP_OPTION_LEN;
    rte_pktmbuf_append(buf, sizeof(struct ether_hdr) + sizeof(struct ipv6_hdr)+ sizeof(struct na_header) + ICMP_OPTION_LEN);

    send_to_switch(g_lcoreid, dev->port, (void*)&buf,  1, 0);
}


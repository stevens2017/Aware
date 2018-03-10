#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_icmp.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <netinet/in.h>
#include "alpha.h"
#include "al_common.h"
#include "al_ip.h"
#include "al_icmp.h"
#include "al_vport.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_arp.h"
#include "al_mempool.h"
#include "al_ip_input.h"
#include "al_status.h"

static __thread long l_last_timestamps=0;

int al_main_process(uint8_t lcoreid, uint8_t portid, struct rte_mbuf **mbuf, int cnt, uint8_t from, uint8_t type)
{
    struct ipv4_hdr *ipv4hdr;
    struct ipv6_hdr *ipv6hdr;

    int i, k;
    struct ether_hdr *eth;
    uint16_t ethertype;
    uint16_t iphdr_hlen;
    uint16_t iphdr_len;
    void* nextptr;
    struct rte_mbuf *outbuf[RTE_AL_RX_DESC_DEFAULT];

    k=0;
    for (i = 0; i < cnt; i++)
    {
        eth = rte_pktmbuf_mtod(mbuf[i], struct ether_hdr *);
        ethertype = rte_be_to_cpu_16(eth->ether_type);

#if 0
        LOG(LOG_DEBUG2, LAYOUT_DATALINK, "From %d ring counter %d Lcoreid %d port %u src:"ETHER_ADDR_FORMART" dst:"ETHER_ADDR_FORMART" type:%d", from, rte_ring_count(g_thread_ring[lcoreid]), lcoreid, portid,  ETHER_ADDR(&eth->s_addr), ETHER_ADDR(&eth->d_addr), ethertype);


        if (unlikely(ethertype == ETHER_TYPE_VLAN)) {
            LOG(LOG_INFO, LAYOUT_DPDK, "Ridicuous");
            SS_INCR(ll_have_vlan_info);
            rte_pktmbuf_free(mbuf[i]);
            continue;
        }

#endif
        
        if (likely(ethertype == ETHER_TYPE_IPv4)) {
            ipv4hdr = rte_pktmbuf_mtod_offset(mbuf[i], struct ipv4_hdr *, sizeof(struct ether_hdr));
            /* obtain IP header length in number of 32-bit words */
            iphdr_hlen = (ipv4hdr->version_ihl & 0xf) * 4;

            if(unlikely(iphdr_hlen > rte_pktmbuf_pkt_len(mbuf[i]))){
                SS_INCR(ll_bad_pkg_len);
                rte_pktmbuf_free(mbuf[i]);
                continue;
            }
			
            /* obtain ip length in bytes */
            iphdr_len = rte_be_to_cpu_16(ipv4hdr->total_length);
            if(unlikely(iphdr_len > rte_pktmbuf_pkt_len(mbuf[i]))){
                rte_pktmbuf_free(mbuf[i]);
                SS_INCR(ll_bad_pkg_len);
                continue;
            }
			
            if((ipv4hdr->next_proto_id == IPPROTO_TCP) || (ipv4hdr->next_proto_id == IPPROTO_UDP)){
                nextptr=rte_pktmbuf_mtod_offset(mbuf[i], void*, sizeof(struct ether_hdr)+iphdr_hlen);
#if 0
                LOG(LOG_TRACE, LAYOUT_DATALINK,"Session core %u input port %u from(%d) "ETHER_ADDR_FORMART"("IPV4_IP_FORMART") to "ETHER_ADDR_FORMART"("IPV4_IP_FORMART")", 
                    g_lcoreid, portid, from,  ETHER_ADDR(&eth->s_addr),	IPV4_DATA_READABLE(&ipv4hdr->src_addr), ETHER_ADDR(&eth->d_addr),  IPV4_DATA_READABLE(&ipv4hdr->dst_addr));
#endif
                mbuf[i]->port=portid;
                al_ipv4_input(mbuf[i], ipv4hdr, nextptr, from, type);
                continue;
            }
		    
        }else if (likely(ethertype == ETHER_TYPE_IPv6)){
            ipv6hdr = rte_pktmbuf_mtod_offset(mbuf[i], struct ipv6_hdr *, sizeof(struct ether_hdr));
            /* obtain IP header length in number of 32-bit words */
            iphdr_hlen = 40;
            if(unlikely(iphdr_hlen > rte_pktmbuf_pkt_len(mbuf[i]))){
                SS_INCR(ll_bad_pkg_len);
                rte_pktmbuf_free(mbuf[i]);
                continue;
            }
			
            /* obtain ip length in bytes */
            iphdr_len = rte_be_to_cpu_16(ipv6hdr->payload_len);
            if(unlikely(iphdr_len < rte_pktmbuf_pkt_len(mbuf[i]))){
                SS_INCR(ll_bad_pkg_len);
                rte_pktmbuf_free(mbuf[i]);
                continue;
            }
			
            //ip v6 check
            if(likely((ipv6hdr->proto == IPPROTO_TCP) || (ipv6hdr->proto == IPPROTO_UDP))){
                nextptr=rte_pktmbuf_mtod_offset(mbuf[i], void*, sizeof(struct ether_hdr)+40);
                mbuf[i]->port=portid;
                al_ipv6_input(mbuf[i], ipv6hdr, nextptr, from, type);
                continue;
            }else if(ipv6hdr->proto == IPPROTO_ICMPV6){
                al_icmp6_proc(mbuf[i]);
            }
        }else if (ethertype == ETHER_TYPE_ARP) {
            al_arp_input(mbuf[i]);
        }
        outbuf[k++]=mbuf[i];
    }

    send_to_switch(lcoreid, portid, outbuf, k, type == AL_PORT_TYPE_KNI ? 0 : 1);

    if(unlikely(g_now_micrsec - l_last_timestamps > 1000)){
        al_session_timeout();
        l_last_timestamps=g_now_micrsec;
    }

    if(unlikely(g_thread_info[g_lcoreid].oss)){
        al_output_thread_session();
    }

    return 0;
}



#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include "alpha.h"
#include "al_common.h"
#include "al_session.h"
#include "al_to_api.h"
#include "al_dev.h"
#include "al_chksum.h"

void al_process(struct session* s, struct rte_mbuf* buf, void* iphdr, void* tcphdr){

    struct tcp_hdr*   hdr;
    struct ipv4_hdr* ip4hdr;
    struct ipv6_hdr* ip6hdr;
    
    session_t* up=s->up;
    struct ether_hdr* hwd=rte_pktmbuf_mtod(buf, struct ether_hdr*); 
    s->stout = SESSION_IDLE_TIMEOUT;

    hwd->d_addr = up->from_haddr;
  
    if(!s->is_dr){
        /*llb dr mode*/
        hwd->s_addr = up->to_haddr;    
    }

    if(s->ftype == AL_FLT_LLB){
        goto END;
    }

    /*tcp checksum ip checksum*/
    hdr=(struct tcp_hdr*)tcphdr;
    hdr->src_port=up->local_port;
    hdr->dst_port=up->remote_port;
    if( up->in_type == 0){
        /*ip v4*/
        ip4hdr=(struct ipv4_hdr*)iphdr;
        ip4hdr->src_addr=up->local_addr.address4;
        ip4hdr->dst_addr=up->remote_addr.address4;
        hdr->cksum=0;
        hdr->cksum=inet_chksum_pseudo(buf, (uint8_t*)hdr - (uint8_t*)hwd, ip4hdr->src_addr, ip4hdr->dst_addr, ip4hdr->next_proto_id, rte_be_to_cpu_16(ip4hdr->total_length) - ((ip4hdr->version_ihl & 0xf)*4));
        ip4hdr->hdr_checksum=0;
        ip4hdr->hdr_checksum=inet_chksum((uint8_t*)ip4hdr, ((ip4hdr->version_ihl & 0xf)*4));
    }else{
        /*ip v6*/
        ip6hdr=(struct ipv6_hdr*)iphdr;
        *(uint128_t*)&ip6hdr->src_addr=up->local_addr.address6;
        *(uint128_t*)&ip6hdr->dst_addr=up->remote_addr.address6;
        hdr->cksum=0;
        hdr->cksum = inet6_chksum_pseudo(buf, (uint8_t*)hdr - (uint8_t*)hwd, (uint16_t*)ip6hdr->src_addr, (uint16_t*)ip6hdr->dst_addr, ip6hdr->proto, rte_be_to_cpu_16(ip6hdr->payload_len)-sizeof(struct ipv6_hdr));        
    }

END:
    send_to_switch(g_lcoreid, up->portid, &buf, 1, 0);
}

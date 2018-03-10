#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include "al_session.h"
#include "al_common.h"
#include "al_vserver.h"
#include "al_ip_input.h"
#include "al_router.h"
#include "al_arp.h"
#include "al_log.h"
#include "al_vport.h"
#include "al_port.h"
#include "lwip/inet_chksum.h"
#include "alpha.h"

static void slb_handler(struct session* s, struct rte_mbuf* buf, void* iphdr, void* tcphdr);

void al_slb_input(al_vserver_t* vss, struct rte_mbuf* buf, void* hdr, void* nextptr){

    struct ether_hdr* eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    uint16_t ethertype = rte_be_to_cpu_16(eth->ether_type);

    session_t *vs, *rs;

    ipaddr_t* src;
    ipaddr_t* dst;
    ipaddr_t  route;
    uint8_t    iptype;
    uint16_t   vlanid=0;
	
    uint16_t   rsport;
    ipaddr_t  raddr;
    al_dev* dev;
    vport_t* vport;

    struct tcp_hdr* tcphdr=(struct tcp_hdr*)nextptr;
 
    vs = session_create();
    if (vs == NULL) {
        LOG(LOG_ERROR, LAYOUT_APP, "Create session failed");
        rte_pktmbuf_free(buf);
        return;
    }else{
        vs->stype = SESSION_NORMAL;
        vs->is_ups = 1;
        vs->cache_mbuf_cnt = 0;
        vs->con_closed = 0;
        vs->port_alloc=0;
        vs->valid = 1;
        vs->t_pkt = NULL;
        vs->stout = SESSION_IDLE_TIMEOUT;
    }

    if(likely(ethertype == ETHER_TYPE_IPv4)){
        src = (ipaddr_t*)&((struct ipv4_hdr*)hdr)->src_addr;
        dst = (ipaddr_t*)&((struct ipv4_hdr*)hdr)->dst_addr;
        iptype = AF_INET;
        vs->proto = ((struct ipv4_hdr*)hdr)->next_proto_id;
    }else if(ethertype == ETHER_TYPE_IPv6){
        src = (ipaddr_t*)((struct ipv6_hdr*)hdr)->src_addr;
        dst = (ipaddr_t*)((struct ipv6_hdr*)hdr)->dst_addr;
        iptype = AF_INET6;
        vs->proto = ((struct ipv6_hdr*)hdr)->proto;
    }else{
        rte_pktmbuf_free(buf);
        session_free(vs);
        LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "File:%s:%d Arp search failed", __FILE__, __LINE__);
        return;
    }

    vs->in_type = (iptype == AF_INET) ? 0 : 1;

    if(al_router_find((uint8_t*)dst, (uint8_t*)src, iptype, (uint8_t*)route, &vlanid ) != 0){

        if(iptype == AF_INET ){
            LOG(LOG_ERROR, LAYOUT_APP, "No route for"IPV4_IP_FORMART, IPV4_DATA_READABLE(dst));
        }else{
            LOG(LOG_ERROR, LAYOUT_APP, "No route for"IPV6_IP_FORMART, IPV6_DATA_READABLE(dst));
        }
	   
        session_free(vs);
        rte_pktmbuf_free(buf);
        return;
    }else{

        vs->vlanid = vlanid;
        dev=al_get_dev_by_vlanid(vlanid);
        if(al_arp_search((ipaddr_t*)&route, iptype, &vs->from_haddr, dev)  == 0){
            session_free(vs);
            rte_pktmbuf_free(buf);
            LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "File:%s:%d Arp search failed "IPV4_IP_FORMART, __FILE__, __LINE__, IPV4_DATA_READABLE(route));
            return;
        }else{
            if(iptype == AF_INET ){
                IPV4_COPY(vs->route_addr, &route);
                IPV4_COPY(vs->local_addr, dst);
                IPV4_COPY(vs->remote_addr, src);
            }else{
                IPV6_COPY(vs->route_addr,  &route);
                IPV6_COPY(vs->local_addr,  dst);
                IPV6_COPY(vs->remote_addr, src);
            }
            vs->local_port = tcphdr->dst_port;
            vs->remote_port = tcphdr->src_port;
            vs->vsid=vss->vs_id;

            vport = al_get_vport_by_vlanid(vlanid);
            ether_addr_copy((struct ether_addr*)vport->haddr, &vs->to_haddr);

            dev = al_get_dev_by_vlanid(vlanid);
            vs->portid = dev->port;
        }
    }

    if(iptype == AF_INET ){
        LOG(LOG_TRACE, LAYOUT_APP, "VS Session create frome haddr "ETHER_ADDR_FORMART" ip "IPV4_IP_FORMART" port:%d to haddr "ETHER_ADDR_FORMART" ip "IPV4_IP_FORMART" port:%d in_type:%d", 
            ETHER_ADDR(&vs->to_haddr), IPV4_DATA_READABLE(vs->remote_addr), vs->remote_port,
            ETHER_ADDR(&vs->from_haddr),   IPV4_DATA_READABLE(vs->local_addr), vs->local_port, vs->in_type);
    }else{
        LOG(LOG_TRACE, LAYOUT_APP, "VS Session create frome haddr "ETHER_ADDR_FORMART" ip "IPV6_IP_FORMART" port:%d to haddr "ETHER_ADDR_FORMART" ip "IPV6_IP_FORMART" port:%d", 
            ETHER_ADDR(&vs->to_haddr), IPV6_DATA_READABLE(vs->remote_addr), vs->remote_port,
            ETHER_ADDR(&vs->from_haddr),   IPV6_DATA_READABLE(vs->local_addr), vs->local_port);
    }

    //TODO: set handler
    //vs->handler = NULL;

    rs = session_create();
    if (rs == NULL) {
        LOG(LOG_ERROR, LAYOUT_APP, "Create session failed");
        session_free(vs);
        rte_pktmbuf_free(buf);
        return;
    }else{
        rs->stype = SESSION_NORMAL;
        rs->is_ups = 0;
        rs->cache_mbuf_cnt = 0;
        rs->con_closed = 0;
        rs->port_alloc=0;
        rs->valid = 1;
        rs->t_pkt = NULL;
        rs->proto = vs->proto;
        rs->stout = SESSION_IDLE_TIMEOUT;
        rs->in_type = (iptype == AF_INET) ? 0 : 1;
    }

                
    /*
     * fetch realserver
     */
    iptype=0;
    if(!vserver_find_rs(vs->vsid, raddr, &iptype, &rsport)){
        LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "Can't find property rs for vs :%d", vs->vsid);
        session_free(vs);
        session_free(rs);
        rte_pktmbuf_free(buf);
        return;
    }else{
        if(iptype == AF_INET ){
            IPV4_COPY(rs->remote_addr, raddr);
        }else{
            IPV6_COPY(rs->remote_addr, raddr);
        }
        rs->remote_port = rsport;
    }

    /*
     * find rs router
     */
    if(al_router_find(NULL, (uint8_t*)raddr, iptype, (uint8_t*)route, &vlanid) != 0){
        if(iptype == AF_INET ){
            LOG(LOG_ERROR, LAYOUT_APP, "No route for"IPV4_IP_FORMART, IPV4_DATA_READABLE(raddr));
        }else{
            LOG(LOG_ERROR, LAYOUT_APP, "No route for"IPV6_IP_FORMART, IPV6_DATA_READABLE(raddr));
        }
        session_free(vs);
        session_free(rs);
        rte_pktmbuf_free(buf);
        return;
    }else{
        if(iptype == AF_INET ){
            IPV4_COPY(rs->route_addr, route);
        }else{
            IPV6_COPY(rs->route_addr,  route);
        }
        dev = al_get_dev_by_vlanid(vlanid);
    }

    if(al_arp_search((ipaddr_t*)&route, iptype, &rs->from_haddr, dev)  == 0){
        session_free(vs);
        session_free(rs);
        rte_pktmbuf_free(buf);
        LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "File:%s:%d Arp search failed "IPV4_IP_FORMART, __FILE__, __LINE__, IPV4_DATA_READABLE(route));
        return;
    }

    rs->vlanid = vlanid;
    vport = al_get_vport_by_vlanid(vlanid);
    if(unlikely(vport == NULL)){
        LOG(LOG_ERROR, LAYOUT_APP, "Vlan[%u] port disable", vlanid);
        session_free(vs);
        session_free(rs);
        rte_pktmbuf_free(buf);
        return;
    }

    /*
      * fetch local ip and port
      */
    if(unlikely(vport_select_local_ip(vport, &rs->local_addr, iptype, &route) != 1) ){
        LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "Vlan no primery ip %d", vlanid);
        session_free(vs);
        session_free(rs);
        rte_pktmbuf_free(buf);
        return;
    }

    ether_addr_copy((struct ether_addr*)vport->haddr, &rs->to_haddr);

    rs->local_port = al_new_port(rs);
    if (rs->local_port == 0) {
        LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "No valid port");
        session_free(vs);
        session_free(rs);
        rte_pktmbuf_free(buf);
        return;
    }
    
    ether_addr_copy((struct ether_addr*)vport->haddr, &rs->to_haddr);
    dev = al_get_dev_by_vlanid(vlanid);
    rs->portid = dev->port;

    vs->up = rs;
    rs->up = vs;
    al_session_add(vs);
    al_session_add(rs);

    //handler
    vs->handler=slb_handler;
    rs->handler=slb_handler;

    if(iptype == AF_INET ){
        LOG(LOG_TRACE, LAYOUT_APP, "UPS Session create frome haddr "ETHER_ADDR_FORMART" ip "IPV4_IP_FORMART" port:%d to haddr "ETHER_ADDR_FORMART" ip "IPV4_IP_FORMART" port:%d in_type:%d", 
            ETHER_ADDR(&rs->from_haddr), IPV4_DATA_READABLE(rs->remote_addr), rs->remote_port,
            ETHER_ADDR(&rs->to_haddr),   IPV4_DATA_READABLE(rs->local_addr), rs->local_port, rs->in_type);
    }else{
        LOG(LOG_TRACE, LAYOUT_APP, "UPS VS %d session create frome haddr "ETHER_ADDR_FORMART" ip "IPV6_IP_FORMART" port:%d to haddr "ETHER_ADDR_FORMART" ip "IPV6_IP_FORMART" port:%d", 
            ETHER_ADDR(&rs->from_haddr), IPV6_DATA_READABLE(rs->remote_addr), rs->remote_port,
            ETHER_ADDR(&rs->to_haddr),   IPV6_DATA_READABLE(rs->local_addr), rs->local_port);
    }
    vs->handler(vs, buf, hdr, nextptr);
}

static void slb_handler(struct session* s, struct rte_mbuf* buf, void* iphdr, void* tcphdr){

    session_t* up=s->up;
    struct tcp_hdr*   hdr=(struct tcp_hdr*)tcphdr;
    struct ipv4_hdr* ip4hdr;
    struct ipv6_hdr* ip6hdr;
    struct ether_hdr* hwd=rte_pktmbuf_mtod(buf, struct ether_hdr*); 

    s->stout = SESSION_IDLE_TIMEOUT;
    hdr->src_port = up->local_port;
    hdr->dst_port = up->remote_port;
    
    if(s->in_type == 0){
        ip4hdr=(struct ipv4_hdr*)iphdr;
        ip4hdr->src_addr=IPV4(up->local_addr);
        ip4hdr->dst_addr=IPV4(up->remote_addr);
        /*tcp chksum*/
        hdr->cksum = 0;
        ip4hdr->hdr_checksum=0;
        hdr->cksum =  inet_chksum_pseudo(buf, (uint8_t*)hdr - (uint8_t*)hwd, ip4hdr->src_addr, ip4hdr->dst_addr, ip4hdr->next_proto_id, ntohs(ip4hdr->total_length) - ((ip4hdr->version_ihl & 0xf)*4));
        ip4hdr->hdr_checksum=0;
        ip4hdr->hdr_checksum=inet_chksum((uint8_t*)ip4hdr, ((ip4hdr->version_ihl & 0xf)*4));
    }else{
        ip6hdr = (struct ipv6_hdr*)hdr;
        IPV6_COPY(ip6hdr->src_addr, up->local_addr);
        IPV6_COPY(ip6hdr->dst_addr, up->remote_addr);
        /*tcp chksum*/
        hdr->cksum = 0;
        hdr->cksum = inet6_chksum_pseudo(buf, (uint8_t*)hdr - (uint8_t*)hwd, (uint16_t*)ip6hdr->src_addr, (uint16_t*)ip6hdr->dst_addr, ip6hdr->proto, ntohs(ip6hdr->payload_len)-sizeof(struct ipv6_hdr));
    }

    hwd->d_addr = up->from_haddr;
    hwd->s_addr = up->to_haddr;

    send_to_switch(g_lcoreid, up->portid, &buf, 1);
}


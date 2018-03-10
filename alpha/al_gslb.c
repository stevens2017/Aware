#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <inttypes.h>
#include <rte_udp.h>
#include "lwip/inet_chksum.h"
#include "al_router.h"
#include "al_arp.h"
#include "al_log.h"
#include "al_gslb.h"
#include "al_vserver.h"
#include "al_ip.h"
#include "al_dev.h"
#include "alpha.h"
#include "al_to_api.h"
#include "al_errmsg.h"

struct dns_query {
    uint16_t type;
    uint16_t cls;
}__attribute__((__packed__));

struct R_DATA {
    uint16_t type; 
    uint16_t class; 
    uint32_t ttl;
    uint16_t data_len; 
}__attribute__((__packed__));

static unsigned char * dns_parse_name(unsigned char *query);
static uint16_t dns_answer_pkg_make(struct dns_hdr* dnshdr, uint8_t* dptr, uint8_t dlen, uint8_t* ans_ptr, ipaddr_t addr, uint8_t iptype);

void al_glsb_input(al_vserver_t* vs, struct rte_mbuf* buf, void* hdr, void* nextptr){
    
    struct ether_hdr* eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    uint16_t ethertype = rte_be_to_cpu_16(eth->ether_type);
    
    //struct ether_addr  eaddr;
    uint8_t* uptr, *qstr1, *qstr2;
    ipaddr_t* src;
    ipaddr_t* dst;
    ipaddr_t  route;
    ipaddr_t  taddr;
    uint8_t    iptype;
    uint8_t    portid;
    uint8_t    ip_hlen;
    uint16_t   vlanid=0, pkt_len=0;
    uint16_t   port, dport, sport, dnslen, qid;
    int        implm = 1, dlen;
    unsigned char*      ptr, *ans_ptr;
    al_vserver_ctx_t ctx;
    al_vserver_t     ovs;
    struct dns_query* qry;
 
    al_dev*  dev;
    vport_t* vport;
    struct dns_flag* fptr;

    struct udp_hdr* udphdr=(struct udp_hdr*)nextptr;
    struct dns_hdr* dnshdr=(struct dns_hdr*)(udphdr+1);

    dnshdr->flag = ntohs(dnshdr->flag);
    qid=dnshdr->qid;
    fptr = (struct dns_flag*)&dnshdr->flag;
    if( unlikely((fptr->opcode != 0) || (ntohs(dnshdr->qdcount) != 1))){
        implm = 0;
    }else{
        qstr1 = ptr = (unsigned char*)(dnshdr+1);
        dlen = dns_parse_name(ptr) - ptr;
        qry = (struct dns_query*)(ptr + dlen);
        qstr2 = ans_ptr = (uint8_t*)(qry+1);
        if(ntohs(qry->type) != DNS_Q_A){
            implm = 0;
        }else{
            if(ntohs(qry->cls) != DNS_Q_CLASS_IN ){
                implm = 0;
            }
        }
    }

    if(likely(ethertype == ETHER_TYPE_IPv4)){
        src = (ipaddr_t*)&((struct ipv4_hdr*)hdr)->src_addr;
        dst = (ipaddr_t*)&((struct ipv4_hdr*)hdr)->dst_addr;
        iptype = AF_INET;

        IPV4_COPY(taddr, dst);
        IPV4_COPY(dst, src);
        IPV4_COPY(src, taddr);

        AL_SET_DEFAULT_TTL_IPV4((struct ipv4_hdr*)hdr);
        ip_hlen=sizeof(struct ipv4_hdr)/4 + ((sizeof(struct ipv4_hdr)%4 == 0)? 0 : 1);
        uptr = (uint8_t*)hdr + ip_hlen*4;
        memset((uint8_t*)hdr+sizeof(struct ipv4_hdr),  '\0',  ip_hlen*4-sizeof(struct ipv4_hdr));
        AL_IP_SET_IPV4(((struct ipv4_hdr*)hdr)->version_ihl);
    }else if(ethertype == ETHER_TYPE_IPv6){
        src = (ipaddr_t*)((struct ipv6_hdr*)hdr)->src_addr;
        dst = (ipaddr_t*)((struct ipv6_hdr*)hdr)->dst_addr;
        iptype = AF_INET6;

        IPV6_COPY(taddr, dst);
        IPV6_COPY(dst, src);
        IPV6_COPY(src, taddr);

        AL_SET_DEFAULT_TTL_IPV6((struct ipv6_hdr*)hdr);
        ip_hlen=sizeof(struct ipv6_hdr);
        uptr = (uint8_t*)hdr+ip_hlen;
    }else{
        rte_pktmbuf_free(buf);
        return;
    }

    if(al_router_find( (uint8_t*)src, (uint8_t*)dst,  iptype, (uint8_t*)route, &vlanid) != 0){
        rte_pktmbuf_free(buf);
        return;
    }else{
        dev=al_get_dev_by_vlanid(vlanid);
        if(al_arp_search((ipaddr_t*)&route, iptype, &eth->d_addr, dev)  == 0){
            rte_pktmbuf_free(buf);
            return;
        }else{
            vport = al_get_vport_by_vlanid(vlanid);
            ether_addr_copy((struct ether_addr*)vport->haddr, (struct ether_addr*)&eth->s_addr);
            dev = al_get_dev_by_vlanid(vlanid);
            portid = dev->port;
        }
    }
    
    dport=udphdr->dst_port;
    sport=udphdr->src_port;
    
    udphdr = (struct udp_hdr*)uptr;
    dnshdr = (struct dns_hdr*)(udphdr+1);

    //copy query question
    ans_ptr = (uint8_t*)(dnshdr+1);
    memcpy(ans_ptr, qstr1, qstr2-qstr1);
    ans_ptr+= (qstr2-qstr1);
    
    udphdr->dst_port = sport;
    udphdr->src_port = dport;
    dnshdr->qid = qid;
    dnshdr->flag = 0;
    fptr = (struct dns_flag*)&dnshdr->flag;
    fptr->qr = 1;
    fptr->aa = 1;
    
    if( likely(implm) ){
        // match dns
        ctx.type = AL_GSLB_DOMAIN;
        ctx.data.glsb.domain.data = ptr;
        ctx.data.glsb.domain.len = dlen;
        ctx.data.glsb.id = vs->id;
        
        if(al_vserver_find(NULL, 0, iptype, g_has_llb, &ctx, &ovs)  != 1){
            LOG(LOG_ERROR, LAYOUT_TCP_PROXY, "Can't find property glsb domain for glsb vs :%d", vs->vs_id);
            rte_pktmbuf_free(buf);
            return;
        }else{
            iptype=0;
            if( vserver_find_rs(ovs.vs_id, taddr, &iptype, &port) ){
                //return dns ansername
                dnslen = dns_answer_pkg_make(dnshdr, ptr, dlen, ans_ptr, taddr, iptype)+sizeof(struct udp_hdr);
                fptr->rcode = 0;
                dnshdr->flag = htons(dnshdr->flag);
                udphdr->dgram_len=htons(dnslen);
            }else{
                //return cname
                LOG(LOG_ERROR, LAYOUT_APP, "No rs for id %u id:%u", ovs.vs_id, ctx.data.glsb.id);
                rte_pktmbuf_free(buf);
                return;
            }
        }
    }else{
        // unimplement
        dnshdr->ancount = htons(1);
        dnshdr->nscount = 0;
        dnshdr->arcount = 0;

        fptr->rcode = 4;
        dnshdr->flag = dnshdr->flag;
        udphdr->dgram_len = sizeof(struct dns_hdr);
    }

    if(likely(ethertype == ETHER_TYPE_IPv4)){

        AL_SET_DEFAULT_TTL_IPV4((struct ipv4_hdr*)hdr);

        ((struct ipv4_hdr*)hdr)->hdr_checksum = 0;
        udphdr->dgram_cksum = 0;
        ((struct ipv4_hdr*)hdr)->version_ihl &= 0xf0;
        ((struct ipv4_hdr*)hdr)->version_ihl |= (ip_hlen&0xf);
        ((struct ipv4_hdr*)hdr)->next_proto_id = IPPROTO_UDP;
        ((struct ipv4_hdr*)hdr)->total_length =  htons(dnslen + ip_hlen*4);
        ((struct ipv4_hdr*)hdr)->hdr_checksum = inet_chksum((uint8_t*)hdr, ip_hlen*4);

        pkt_len=dnslen + ip_hlen*4+sizeof(struct ether_hdr);
    }else if(ethertype == ETHER_TYPE_IPv6){
        AL_SET_DEFAULT_TTL_IPV6((struct ipv6_hdr*)hdr);
    }

    if(pkt_len < rte_pktmbuf_pkt_len(buf)){
        rte_pktmbuf_trim(buf, pkt_len - rte_pktmbuf_pkt_len(buf));
    }else{
        rte_pktmbuf_append(buf, pkt_len - rte_pktmbuf_pkt_len(buf));
    }
    
    send_to_switch(g_lcoreid, portid, &buf, 1);
	
}

char* al_domain_name(const char* start, const char* end, char* dst){

    char* ptr=dst;
    const char*sptr;

    dst += 1;
    sptr=start;
	
    for(;start < end; start++){
        if( *start == '.' ){
            *ptr = start - sptr;
            break;
        }else{
            *dst++ = *start;
        }
    }

    if( start == end ){
        *ptr=start - sptr;
        return dst;
    }else{
        if( *ptr == 0 ){
            return NULL;
        }else{
            return al_domain_name(++start, end, dst);
        }
    }
	
}

int al_domain_name_len(uint8_t* ptr, int steps){
    int  len, rc;

    steps++;
    if( steps > 20 ){
        return -1;
    }
    
    if(likely((*ptr >= '0' )&&( *ptr <= '9'))){
        len=*ptr - '0';
        if(len == 0 ){
            return 0;
        }else{
            ptr+=(len+1);
            rc = al_domain_name_len(ptr, steps);
            if( rc == -1 ){
                return -1;
            }else{
                return len + rc;
            }
        }
    }else{
        return -1;
    }
	
}

static unsigned char *
dns_parse_name(unsigned char *query)
{
    unsigned char n;

    do {
        n = *query++;
        /** @see RFC 1035 - 4.1.4. Message compression */
        if ((n & 0xc0) == 0xc0) {
            /* Compressed name */
            break;
        } else {
            /* Not compressed name */
            while (n > 0) {
                ++query;
                --n;
            };
        }
    } while (*query != 0);

    return query + 1;
}

/**
 * Compare the "dotted" name "query" with the encoded name "response"
 * to make sure an answer from the DNS server matches the current dns_table
 * entry (otherwise, answers might arrive late for hostname not on the list
 * any more).
 *
 * @param query hostname (not encoded) from the dns_table
 * @param response encoded hostname in the DNS response
 * @return 0: names equal; 1: names differ
 */
u8_t
al_dns_compare_name(unsigned char *addr_dotted, unsigned char *addr_encoded)
{

    unsigned char *query = addr_dotted;
    unsigned char *response = addr_encoded;
    unsigned char n;

    do {
        n = *response++;
        /** @see RFC 1035 - 4.1.4. Message compression */
        if ((n & 0xc0) == 0xc0) {
            /* Compressed name */
            break;
        } else {
            /* Not compressed name */
            while (n > 0) {
                if ((*query) != (*response)) {
                    return 1;
                }
                ++response;
                ++query;
                --n;
            };
            ++query;
        }
    } while (*response != 0);

    return 0;
}

static uint16_t dns_answer_pkg_make(struct dns_hdr* dnshdr, uint8_t* dptr, uint8_t dlen, uint8_t* ans_ptr, ipaddr_t addr, uint8_t iptype){

    struct R_DATA* rd;
    uint8_t* ptr;
   
    dnshdr->ancount = htons(1);
    dnshdr->nscount = 0;
    dnshdr->arcount = 0;

    memcpy(ans_ptr, dptr, dlen);

    rd = (struct R_DATA*)(ans_ptr + dlen);
    rd->ttl = htonl(86400);
    rd->class = htons(DNS_Q_CLASS_IN);
    ptr=(uint8_t*)(rd+1);

    if( iptype == AF_INET ){
        IPV4_COPY(ptr, addr);
        rd->type = htons(DNS_Q_A);
        ptr+=4;
        rd->data_len = htons(4);
    }else{
        IPV6_COPY(ptr, addr);
        rd->type = htons(DNS_Q_AAAA);
        ptr+=16;
        rd->data_len = htons(16);
    }

    return ptr - (uint8_t*)dnshdr;
}


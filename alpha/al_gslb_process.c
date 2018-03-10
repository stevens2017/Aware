#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rte_rwlock.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include "alpha.h"
#include "al_common.h"
#include "common/al_buf.h"
#include "al_hash.h"
#include "hlist.h"
#include "al_log.h"
#include "al_to_api.h"
#include "al_geoip.h"
#include "al_radix.h"
#include "al_mempool.h"
#include "al_dev.h"
#include "al_chksum.h"
#include "al_gslb.h"
#include "al_dns.h"
#include "al_member.h"

#define AL_DOMAIN_NAME_LEN 63


struct dns_hdr {
    uint16_t qid;
    uint16_t flag;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
}__attribute__((__packed__)); 

struct dns_flag{
    uint16_t rcode:4;
    uint16_t zore:3;
    uint16_t ra:1;
    uint16_t rd:1;
    uint16_t tc:1;
    uint16_t aa:1;
    uint16_t opcode:4;
    uint16_t qr:1;
}__attribute__((__packed__));

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

static uint8_t* dns_parse_name(uint8_t *query, al_buf_t * b);

void al_gslb_process(struct rte_mbuf* buf, void* iphdr, void* nextptr){

    struct ether_hdr* eth;
    struct ether_addr  ehaddr;
    struct ipv4_hdr* ipv4hdr;
    struct ipv6_hdr* ipv6hdr;
    struct udp_hdr* udphdr;
    struct dns_hdr* dnshdr;
    struct dns_flag* fptr;
    struct R_DATA* anptr;
    
    al_margs margs;
    al_buf_t d;
    al_dns_record* dr;
    uint8_t *qstr1, *qstr2, *ptr;
    int is_tokni=1;
    int implm;
    uint32_t pkt_len;
    uint8_t dm_buf[DOMAIN_MAX_LEN];
    uint16_t etype;
    struct dns_query* qry;
    int dlen;
    int ip_hlen;

    eth = rte_pktmbuf_mtod(buf, struct ether_hdr *);
    etype = rte_be_to_cpu_16(eth->ether_type);
    
    udphdr = (struct udp_hdr*)nextptr;
    dnshdr=(struct dns_hdr*)(udphdr+1);

    dnshdr->flag = ntohs(dnshdr->flag);
    
    fptr = (struct dns_flag*)&dnshdr->flag;
    if(unlikely(fptr->qr == 0)){
        goto END;
    }

    implm=1;
    if(unlikely(dnshdr->qdcount != 1)){
        LOG(LOG_INFO, LAYOUT_APP, "Query counter more than 1");
        implm = 0;
    }else if( unlikely(fptr->opcode != 0)){
        implm = 0;
    }else{
        qstr1 = ptr = (unsigned char*)(dnshdr+1);
        al_init_buf(&d, dm_buf, DOMAIN_MAX_LEN);
        if((qstr2=dns_parse_name(ptr, &d))==NULL){
            goto END;
        }else{
            dlen = qstr2 - ptr;
        }
        
        qry = (struct dns_query*)(ptr + dlen);
        qstr2 = (uint8_t*)(qry+1);

        margs.query_type=ntohs(qry->type);
        if(unlikely((margs.query_type != DNS_Q_A)
                    &&(margs.query_type != DNS_Q_AAAA))){
            implm = 0;
        }else{
            if(unlikely(margs.query_type != DNS_Q_CLASS_IN)){
                implm = 0;
            }
        }
    }

    if( unlikely(implm == 0)){
        dnshdr->ancount=0;    
    }else{
        
        if( al_dns_lookup(&d, &dr) ){
            //failed
            goto END;
        }
        
        if(likely(etype == ETHER_TYPE_IPv4)){
            ipv4hdr=(struct ipv4_hdr*)iphdr;
            if(al_geoip_find(ipv4hdr->src_addr, &margs.vid)){
                //failed
                goto END;
            }else{
                AL_SET_DEFAULT_TTL_IPV4(ipv4hdr);
                ipv4hdr->hdr_checksum = 0;
                udphdr->dgram_cksum = 0;
            }
        }else if(etype == ETHER_TYPE_IPv6){
            ipv6hdr=(struct ipv6_hdr*)iphdr;
            if(al_geoip6_find(*(uint128_t*)ipv6hdr->src_addr, &margs.vid)){
                //failed
                goto END;
            }else{
                AL_SET_DEFAULT_TTL_IPV6(ipv6hdr);
            }
        }else{
            //failed
            goto END;
        }

        *margs.data=qstr2;
        if( al_member_lookup(0, dr->gid, &margs) ){
            //failed
            goto END;
        }
        
        dnshdr->ancount=htons(1);
        memcpy(qstr2, qstr1, dlen);
        anptr=(struct R_DATA*)(qstr2+dlen);
        anptr->type=margs.query_type;
        anptr->class=DNS_Q_CLASS_IN;
        anptr->ttl=margs.ttl;
        anptr->data_len=margs.data_len;
        qstr2+=margs.data_len;

        dnshdr->nscount = 0;
        dnshdr->arcount = 0;

        udphdr->dgram_len=(qstr2-(uint8_t*)udphdr)+sizeof(struct udp_hdr);
        dnshdr->flag = 0;
        fptr->rcode=0;
        fptr->qr = 1;
        fptr->aa = 1;

        SWAP_INT(udphdr->src_port, udphdr->dst_port);
        if(likely(etype == ETHER_TYPE_IPv4)){
            AL_SET_DEFAULT_TTL_IPV4(ipv4hdr);
            SWAP_INT(ipv4hdr->src_addr, ipv4hdr->dst_addr);
            ip_hlen=sizeof(struct ipv4_hdr);
            ipv4hdr->hdr_checksum = 0;
            udphdr->dgram_cksum = 0;
            ipv4hdr->total_length =  htons(sizeof(struct ipv4_hdr)+udphdr->dgram_len);
            if(unlikely((ipv4hdr->version_ihl&0xF)*4 > sizeof(struct ipv4_hdr))){
                ipv4hdr->version_ihl &= 0xF0;
                ipv4hdr->version_ihl |= (sizeof(struct ipv4_hdr)>>4 & 0xF);
                memcpy((uint8_t*)ipv4hdr+sizeof(struct ipv4_hdr), (uint8_t*)dnshdr, udphdr->dgram_len);
            }
            ipv4hdr->hdr_checksum = inet_chksum((uint8_t*)ipv4hdr, ip_hlen);
            pkt_len=sizeof(struct ether_hdr)+sizeof(struct ipv4_hdr)+udphdr->dgram_len;
        }else if(etype == ETHER_TYPE_IPv6){
            SWAP_INT(*(uint128_t*)ipv6hdr->src_addr, *(uint128_t*)ipv6hdr->dst_addr);
            ipv6hdr->payload_len=htons(udphdr->dgram_len);
            AL_SET_DEFAULT_TTL_IPV6(ipv6hdr);
            pkt_len=sizeof(struct ether_hdr)+sizeof(struct ipv6_hdr)+udphdr->dgram_len;
        }

        ehaddr=*(struct ether_addr*)&eth->s_addr;
        eth->s_addr=eth->d_addr;
        eth->d_addr=ehaddr;

        if(pkt_len < rte_pktmbuf_pkt_len(buf)){
            rte_pktmbuf_trim(buf, rte_pktmbuf_pkt_len(buf)-pkt_len);
        }else if(pkt_len > rte_pktmbuf_pkt_len(buf)){
            rte_pktmbuf_append(buf, pkt_len - rte_pktmbuf_pkt_len(buf));
        }
    }

    is_tokni=0;

END:
    send_to_switch(g_lcoreid, buf->port, &buf, 1, is_tokni);
}

#if 0
static uint8_t* dns_parse_name(uint8_t *query, al_buf_t * b, int steps){
    
    unsigned char n;
    uint8_t*  rc = NULL;

    if( unlikely(steps > 10)){
        return NULL;
    }
    
    n = *query++;

    if ((n & 0xc0) == 0xc0) {
        return NULL;
    } else {
        if( unlikely(n == 0)){
            return query;
        }
        
        rc=dns_parse_name(query+n, b, steps+1);
        if( unlikely(rc == NULL)){
            return rc;
        }else{
            if(likely(n+1 < al_buf_rest_len(b))){
                if( likely(al_buf_len(b) > 0) ){
                    buf->last[0]='.';
                    memcpy(buf->last+1, query, n);
                    buf->last+=(n+1);
                }else{
                    memcpy(buf->last, query, n);
                    buf->last+=n;
                }
            }else{
                return NULL;
            }
        }
    }
    
    return rc;
}

#endif

static uint8_t* dns_parse_name(uint8_t *query, al_buf_t * b)
{
    unsigned char n;
    uint8_t* ptr;
    uint8_t c=0;
    
    do {
        n = *query;
        *query=c;
        query++;
        
        if ((n & 0xc0) == 0xc0) {
            return NULL;
        } else {
            c=n;
            if(unlikely(al_buf_len(b) <n)){
                /*no space*/
                return NULL;
            }else if(al_buf_len(b)){
                *b->last='.';
                b->last++;
            }
                
            while (n > 0) {
                ++query;
                --n;
                *b->last='.';
                b->last++;
            };
        }
    } while (*query != 0);

    ptr=query+1;
    
    return ptr;
}


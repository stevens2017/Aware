#include <rte_mempool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "al_common.h"
#include "al_ip.h"
#include "al_hash.h"
#include "al_mempool.h"
#include "al_log.h"

const ipaddr_t al_ip_any={'\0'};

int ip_pool_init(void){
    return 0;
}

ip_t* ip_get(ipcs_t* ip, const char* mask, uint8_t iptype){

    ip_t* s;
    int   rc;
    struct in6_addr addr_mask;

    rc = inet_pton( iptype , mask, (void*)&addr_mask);
    if( rc != 1 ){
        errno = EINVAL;
        LOG(LOG_ERROR, LAYOUT_CONF,  "netmask inet_pton %s\n", mask);
        return NULL;
    }

    s=al_calloc(sizeof(ip_t));
    if(s==NULL){
        errno = ENOMEM;
        return NULL;
    }else{
        if( iptype == AF_INET ){
            s->ip=*ip;
            s->mask.ipv4=*(uint32_t*)&addr_mask;
        }else if( iptype == AF_INET6 ){
            s->ip=*ip;
            s->mask.ipv6=*(uint128_t*)&addr_mask;
        }else{
            errno = EPROTOTYPE;
            al_free(s);
            return NULL;
        }
		
        s->iptype = iptype;
        s->ref.cnt=1;
        return s;
    }
}

ip_t* ip_get_with_mask(ipcs_t* ip, ipcs_t* mask, uint8_t iptype, uint8_t asstype){

    ip_t* s;

    s=al_calloc(sizeof(ip_t));
    if(s==NULL){
        errno = ENOMEM;
        return NULL;
    }else{
        if( iptype == AF_INET ){
            s->ip=*ip;
            s->mask=*mask;
        }else if( iptype == AF_INET6 ){
            s->ip=*ip;
            s->mask=*mask;
        }else{
            errno = EPROTOTYPE;
            al_free(s);
            return NULL;
        }
		
        s->iptype = iptype;
        s->asstype = asstype;
        s->ref.cnt=1;
        return s;
    }
}

void ip_put(ip_t* ip){
    al_free(ip);
}

int  ip_genmask(ipcs_t* ip, uint8_t iptype, uint8_t prefix){

    uint32_t mask;
    uint16_t rest_mask;
    char ipv6_mask[64]={'\0'}, *ptr=ipv6_mask;
    int i,k,rc;
    uint8_t rest;
    struct in6_addr addr_mask;

    if((prefix > 127) && (prefix == 0)){
        return -1;
    }
	
    if( iptype == AF_INET ){
        mask=IPV4_NETMASK(prefix);
        ip->ipv4=mask;
    }else{
        k=0;
        rest = prefix/16;
        for(i=0; i< rest; i++){
            memcpy(ptr, "FFFF:", 5);
            ptr += 5;
            k++;
        }

        if( prefix%16 != 0 ){
            rest_mask=~(0xFFFF>>(prefix%16));
            sprintf(ptr,"%x", rest_mask);
            ptr+=4;
            *ptr++=':';
        }

        k++;

        for(; k<8; k++ ){
            memcpy(ptr, "0000:", 5);
            ptr += 5;
        }

        *(ptr - 1 ) ='\0';

        rc = inet_pton(AF_INET6, ipv6_mask, (void*)&addr_mask);
        if( rc != 1 ){
            return -1;
        }
        IPV6_COPY(ip, addr_mask.s6_addr);
    }

    return 0;
}

void ip_cmd_output(FILE* fp, char* pfx_tab, ip_t* ip){

#define MAX_IP_BUF 128

    char addr[MAX_IP_BUF], mask[MAX_IP_BUF];

    if(ip->iptype == 0){
        inet_ntop(AF_INET, &ip->ip, addr, MAX_IP_BUF);
    }else{
        inet_ntop(AF_INET6, &ip->ip, mask, MAX_IP_BUF);
    }

    fprintf(fp, "%s [%s]  %s %s\n", pfx_tab, ip->iptype == 0? "IPv4" : "IPv6", addr, mask);
}

void al_dump_ip(FILE* fp, char* pfx_tab, ip_t* ip){

#define MAX_IP_BUF 128

    char addr[MAX_IP_BUF], mask[MAX_IP_BUF];
    inet_ntop(ip->iptype, &ip->ip, addr, MAX_IP_BUF);
    inet_ntop(ip->iptype, &ip->mask, mask, MAX_IP_BUF);

    fprintf(fp, "%s {\"iptype\":\"%s\",  \"addrss\":\"%s\",\"netmask\":\"%s\"}", pfx_tab, ip->iptype == AF_INET ? "IPv4" : "IPv6", addr, mask);
    
}


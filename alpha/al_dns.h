#ifndef _AL_DNS_H_
#define _AL_DNS_H_

typedef struct{
    uint32_t gid;
    uint32_t dnsid;
}al_dns_record;

#define DOMAIN_MAX_LEN 255

int al_dns_init(void);
int al_dns_lookup(al_buf_t* dstr, al_dns_record** g);

#endif /*_AL_DNS_H_*/


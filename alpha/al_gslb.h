#ifndef __AL_DNS_H__
#define __AL_DNS_H__

#include <inttypes.h>

#define AL_DOMAIN_NAME_LEN 63

/*query type*/
/*query type*/
#define DNS_Q_A              0x01
#define DNS_Q_NS            0x02
#define DNS_Q_CNAME   0x05
#define DNS_Q_AAAA      0x1c

/*query class*/
#define DNS_Q_CLASS_IN  0x01

char* al_domain_name(const char* start, const char* end, char* dst);
int al_domain_name_len(uint8_t* ptr, int steps);

#endif /*__AL_DNS_H__ */

#ifndef _PKG_CAPTURE_H_
#define _PKG_CAPTURE_H_

#include <inttypes.h>
#include <sys/time.h>

struct pcap_file_header {
        uint32_t magic;
        uint16_t version_major;
        uint16_t version_minor;
        int32_t  thiszone;     /* gmt to local correction */
        uint32_t sigfigs;    /* accuracy of timestamps */
        uint32_t snaplen;    /* max length saved portion of each pkt */
        uint32_t linktype;   /* data link type (LINKTYPE_*) */
}__attribute__((__packed__));

struct pcap_pkthdr {
        uint32_t ts_sec;         /* timestamp seconds */
	uint32_t ts_usec;        /* timestamp microseconds */
        uint32_t caplen;    
        uint32_t len;       
}__attribute__((__packed__));

int al_init_dump(void);
void al_output(uint8_t portid, int type,  struct rte_mbuf** buf, int cnt);
void al_set_pcap_duration(uint8_t id, uint8_t type, int duration);
#endif /* pkg */



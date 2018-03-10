#ifndef _VPORT_H_
#define _VPORT_H_

#include <rte_rwlock.h>
#include <rte_ether.h>
#include "hlist.h"
#include "al_ip.h"

#define VPORT_ENABLE 0x01

#if 0
#define IPV4_LOCK    0x02
#define IPV6_LOCK    0x04
#endif

struct vport;

void  vport_init(uint8_t portid);
void  vport_release_res(uint8_t portid);

int    vport_add_ip(uint8_t portid, ip_t* ip);
struct vport* vport_get(uint8_t portid);
int vport_has_ip(struct vport* vport, ipcs_t* ip, uint8_t iptype);
int    vport_primer_ip(struct vport* vport, ipcs_t* ip, uint8_t iptype);
int    vport_select_local_ip(struct vport* vport, ipcs_t* ip, uint8_t iptype, ipcs_t* rip);

void vport_out_ip(FILE* fp, char* pfx_tab, struct vport* vp);
void al_dump_vport_ip_list(FILE* fp, struct vport* vp);

static inline int al_maddr2eaddr(const char* mac, struct ether_addr* ether){

    int i;
    unsigned int h[6];

    if(sscanf(mac, "%x:%x:%x:%x:%x:%x", &h[0], &h[1], &h[2], &h[3], &h[4], &h[5] ) != 6){
        return -1;
    }else{
        for(i=0; i<6; i++){
            ether->addr_bytes[i]=h[i]&0xFF;
        }
    }
	
    return 0;
}


#endif /*_VPORT_H_*/


#ifndef _AL_DEV_H_
#define _AL_DEV_H_

#include <rte_mbuf.h>
#include <rte_kni.h>
#include <rte_rwlock.h>
#include "common/bit.h"
#include "hlist.h"

#define EHTER_ADDR_LEN  6
#define ETHER_NAME_LEN 32

typedef struct al_dev_st al_dev;
typedef struct vport    vport_t;

struct vport{
    uint8_t           flags;
    uint8_t           dhcp_state;
    uint16_t          portid;
    uint16_t	 mtu;
    rte_rwlock_t      ipv4_lock;
    struct hlist_head ipv4;
    rte_rwlock_t      ipv6_lock;
    struct hlist_head ipv6;
    struct ether_addr  haddr;
    uint8_t ifname[ETHER_NAME_LEN];
}__attribute__((__packed__));

struct al_dev_st{

    uint16_t (*read)(uint8_t, uint8_t, struct rte_mbuf**, uint16_t, uint8_t);
    uint16_t (*write)(uint8_t, uint8_t, struct rte_mbuf**, uint16_t, uint8_t);

    uint8_t          dev_type;
    uint8_t          type;
    uint8_t          port;
    uint8_t          qcnt;
    uint16_t         pvid;
    int16_t          id;
    uint16_t        is_pcap:1;
    uint16_t                    :15;

    union{
        union{
            struct rte_kni* kni;
        }kni;
        union{
            vport_t port;
        }vport;
    }dev;

    struct rte_ring* out[RTE_MAX_LCORE];
}__attribute__((__packed__));

#define VLAN2VPORT(aldev) &(aldev)->dev.vport.port
#define VPORT2VLAN(vp) container_of(vp, al_dev, dev.vport.port)

#define MAX_QUEUE_PEER_NIC 128
#define MAX_QUEUE_CNT 0x1FFF

enum{
   FROM_PHYSIC=0,
   FROM_VPORT,
   FROM_KNI
};

typedef struct dev_queue{
    uint32_t  valid:1;
    uint32_t  portid:12;
    uint32_t  type:8;
    uint32_t         :3;
    uint32_t  queueid:8;
    al_dev    *port;
}__attribute__((__packed__))al_dev_queue;

#define DEV_VALID    1
#define DEV_INVALID  0

extern al_dev  g_ports_ary[MAX_ETHER_COUTER];
extern al_dev  g_kni_ary[MAX_ETHER_COUTER];
extern al_dev  g_vport_ary[MAX_ETHER_COUTER];


int al_dev_init(void);
int init_port_dev(al_dev* ports, uint8_t type);

vport_t* al_get_vport_by_vlanid(uint16_t vid);
al_dev* al_get_dev_by_vlanid(uint16_t vid);
uint16_t send_to_switch(uint8_t lcore, uint8_t portid, struct rte_mbuf** buf, uint16_t cnt, int to_kni);
int16_t al_find_port_with_portid( int16_t id);

void al_finished_pcap(uint8_t id, int type);

static inline int al_get_port_index_byid(uint32_t id, uint8_t* index){
    int i=0;
    al_dev *dev;
    for(i=0; i<g_sys_port_counter; i++){
        dev=&(g_ports_ary[i]);
        if( dev->id == id){
            *index=dev->port;
            return 0;
        }
    }
    return -1;
}
#endif /*_AL_DEV_H_*/


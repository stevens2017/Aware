#ifndef _PORT_H_
#define _PORT_H_

#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_atomic.h>
#include "al_common.h"
#include "hlist.h"
#include "al_ip.h"

struct tcp_seg;

typedef struct port_st{
  struct hlist_node list;
  ipcs_t rip;
  ipcs_t lip;
  uint16_t rport;
  uint16_t lport;
  uint16_t  proto_type:8;
  uint16_t  thread_id:8;
}__attribute__((__packed__))port_t;


static inline uint32_t connect_session_key_v6(uint128_t rip, uint16_t rport, uint128_t lip, uint16_t lport, uint8_t proto, uint32_t mask){
    uint128_t key=((rip^lip)|(rport^lport)) | proto ;
    return ((key & 0xFFFFFFFF)^((key>>32))) & mask;
}

static inline uint32_t connect_session_key_v4(uint32_t rip, uint16_t rport, uint32_t lip, uint16_t lport, uint8_t proto, uint32_t mask){
    return (((rip^lip)|(rport^lport)) | proto) & mask ;
}

int port_init(void);

int al_port_insert(session_t* u);
int al_port_del(session_t* u);
int al_port_insert_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);
int al_port_insert_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);
int al_port_del_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);
int al_port_del_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);


int al_new_port_with_session(session_t* s);

int which_thread_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);
int which_thread_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);
int is_connect_port_already_used_v4(uint32_t rip, uint32_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);
int is_connect_port_already_used_v6(uint128_t rip, uint128_t lip, uint16_t rport, uint16_t lport, uint8_t proto_type);

int al_alocate_new_port_with_session(session_t* s);
#endif

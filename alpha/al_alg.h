#ifndef _AL_ALG_H_
#define _AL_ALG_H_

#include "al_member.h"
#include "al_group.h"

al_member_t* al_alg_rr(al_pool_t* pool, ipaddr_t ip, uint8_t* iptype, uint16_t* port);
al_member_t* al_alg_weight(al_pool_t* pool, ipaddr_t ip, uint8_t* iptype, uint16_t* port);
al_member_t* al_alg_rr_weight(al_pool_t* pool, ipaddr_t ip, uint8_t* iptype, uint16_t* port);

#endif /*_AL_ALG_H_*/


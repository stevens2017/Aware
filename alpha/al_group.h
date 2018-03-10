#ifndef _AL_GROUP_H_
#define _AL_GROUP_H_

#include "al_ip.h"
#include "al_member.h"

/*schedule algorithm*/
enum{
  AL_ALG_RR = 0,
  AL_ALG_WEIGHT,
  AL_ALG_RR_WEIGHT,
  AL_ALG_END
};

typedef struct{
    uint8_t color;
    uint8_t schd;
    uint32_t gid;
}al_group_t;


int al_group_delete(uint32_t fid, uint32_t gid);
int al_group_insert(uint32_t fid, uint32_t gid, uint8_t sched);
int al_group_load(uint32_t fid, uint32_t gid);

#endif /*_AL_GROUP_H_*/


#ifndef _SHARED_MEMORY_H_
#define _SHARED_MEMORY_H_

#include <inttypes.h>

enum{
    SHM_RONLY=0,
    SHM_WRONLY
};

uint8_t* al_openshm(int key, size_t size, int flag);
int al_closeshm(uint8_t* p);

#endif /*_SHARED_MEMORY_H_*/


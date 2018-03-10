#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "common/al_shmem.h"

uint8_t* al_openshm(int key, size_t size, int flag){

    uint8_t *ptr;
    int rc=shmget(key, size,  flag ? IPC_CREAT|0700 : 0);
    if( rc == -1 ){
        ptr=NULL;
    }else{
        ptr=shmat(rc, NULL, 0);
        if( ptr == (void *) -1 ){
            ptr=NULL;
        }
    }
    
    return ptr;
}

int al_closeshm(uint8_t* p){
    return shmdt(p);
}


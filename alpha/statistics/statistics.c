#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "statistics/statistics.h"

struct sys_statistics* g_stat;

#define SYS_STATISTICS_KEY  0x123321

/* ind 0 create and open, 1 open */
int statistics_open(int ind){

    //int flag = (ind == 0) ? IPC_CREAT | SHM_HUGETLB : 0 ;
    int flag = (ind == 0) ? IPC_CREAT : 0 ;
    int key = shmget( SYS_STATISTICS_KEY, sizeof(struct sys_statistics), flag);
    if( key == -1 ){
        fprintf(stderr, "Shmget failed, %s\n", strerror(errno));
        return -1;
    }
    
    g_stat = (struct sys_statistics*)shmat(key, NULL, ind == 1 ? SHM_RDONLY : 0);
    if( g_stat == NULL ){
        fprintf(stderr, "Get statistics memory failed, %s\n", strerror(errno));
        return -1;
    }

    if( ind == 0 ){
        memset((void*)g_stat, '\0', sizeof(struct sys_statistics));
    }

    return 0;
}

int statistics_close(void){
    if(shmdt(g_stat) == -1 ){
	fprintf(stderr, "Memory close failed, %s\n", strerror(errno));
        return -1; 
    }else{
	return 0;
    }
}


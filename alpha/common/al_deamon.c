#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "al_deamon.h"

int al_deamon(const char* wdir){

    int sid;

    pid_t pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork process failed, msg:%s\n", strerror(errno));
        return -1;
    }else if(pid > 0){
        exit(0);
    }

    if((sid = setsid()) < 0){
        fprintf(stderr, "Create new session id failed, msg:%s\n", strerror(errno));
        exit(-1);
    }
 
    if((chdir(wdir==NULL?"/var":wdir)) < 0){
        fprintf(stderr, "Change workspace to %s failed, msg:%s\n", wdir, strerror(errno));
        exit(-1);
    }
 
    umask(0);

#ifndef DEBUG
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif
    
    return pid;
}


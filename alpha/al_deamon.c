#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "al_deamon.h"
#include "al_common.h"

static int sys_status=0;

static void sigchld_handler(int UNUSED(signal_num)) {
    sys_status = 1;
}

static void sigusr_handler(int UNUSED(signal_num)) {
    sys_status = 2;
}

static void shutdown_handler(int UNUSED(signal_num)) {
    sys_status = 3;
}

static void deamon(const char* wdir){
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork process failed, msg:%s\n", strerror(errno));
        exit(-1);
    } else if (pid > 0) {
        exit(0);
    }

    if (setsid() == ((pid_t)-1)) {
        fprintf(stderr, "Create new session id failed, msg:%s\n", strerror(errno));
        exit(-1);
    }

    if ((chdir(wdir == NULL ? "/var" : wdir)) < 0) {
        fprintf(stderr, "Change workspace to %s failed, msg:%s\n", wdir, strerror(errno));
        exit(-1);
    }

    umask(0);

}

static int syslock(const char* lf){

    int fd=open(lf, O_WRONLY|O_CREAT, 0644);
    if( fd == -1 ){
        return -1;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_CUR;
    lock.l_len = 0;
    lock.l_pid=getpid();

    int rc=fcntl(fd, F_SETLK, &lock);
    if( rc == -1 ){
        close(fd);
        return -1;
    }

    return 0;        
}

static int syslock_info(const char* lf){

    int fd=open(lf, O_WRONLY, 0644);
    if( fd == -1 ){
        return 0;
    }

    struct flock lock;
    memset(&lock, '\0', sizeof(struct flock));
    int rc=fcntl(fd, F_GETLK, &lock);
    if( rc == -1 ){
        fprintf(stderr, "Fetch lock info failed, message:%s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if( lock.l_type == F_UNLCK){
        close(fd);
        return 0;
    }

    close(fd);
    return lock.l_pid;        
}

/*
 * front: 1 frontend, 0 backend
 * act  : 0 shutdown, 1 reload, 2 start
 */
int main_loop(int front, int act, const char* wdir, const char* locker){


    sigset_t newmask, oldmask, blockmask;
    pid_t    pid;
    time_t   last;
    int      status, shutdown;
    time_t   now;
    double   seconds;
    int      rpid;

    rpid = syslock_info(locker);

    if( act == ACT_SHUTDOWN ){
        if( rpid <= 0 ){
            exit(0);
        }else{
            kill(rpid, SIGUSR2);
            exit(0);
        }
    }else if( act == ACT_RELOAD ){
        if( rpid <= 0 ){
            exit(-1);
        }else{
            kill(rpid, SIGUSR1);
            exit(0);
        }
    }else if( act == ACT_START ){ 
        if( rpid > 0 ){
            exit(-1);
        }
    }

    deamon(wdir);

    signal(SIGCHLD, sigchld_handler);
    signal(SIGUSR1, sigusr_handler);
    signal(SIGUSR2, shutdown_handler);

    if(sigfillset(&newmask) == -1){
        fprintf(stderr, "sigfillset error %s", strerror(errno));
        return -1;
    }else{
        if(sigprocmask(SIG_BLOCK, &newmask, &oldmask)==-1){
            fprintf(stderr, "sigprocmask error %s", strerror(errno));
            return -1;
        }
    }

    while(1) {
        sys_status = 0;
        shutdown = 0;
        last = time(NULL);

        pid=fork();               
        if (pid < 0) {
            fprintf(stderr, "fork error %s", strerror(errno));
            return -1;
        }else if(pid > 0) {

            sigemptyset(&blockmask);
            sigaddset(&blockmask,SIGUSR1);
            sigaddset(&blockmask,SIGUSR2);
            sigaddset(&blockmask,SIGCHLD);
            sigaddset(&blockmask,SIGPROF);
            if(sigprocmask(SIG_UNBLOCK, &blockmask, NULL) == -1) {
                fprintf(stderr, "sigprocmask error %s", strerror(errno));
                return -1;
            }

            if(syslock(locker) != 0){
                fprintf(stderr, "Set lock file failed: %s", strerror(errno));
                return -1;
            }

            while(1) {
                sleep(3);
                if(sys_status == 1){

                    if( shutdown ){
                        exit(0);
                    }

                    waitpid(pid, &status, WNOHANG);
                    now = time(NULL);
                    seconds = difftime(now, last);
                    if(seconds < 3) {
                        sleep(3);
                    }
                    break;
                }else if(sys_status == 2){
                    kill(pid,SIGTERM);
                }else if( sys_status == 3){
                    kill(pid,SIGTERM);
                    shutdown = 1;
                }
            }
        }else{

            sigemptyset(&blockmask);
            sigaddset(&blockmask, SIGTERM);
            sigaddset(&blockmask, SIGPROF);
            if(sigprocmask(SIG_UNBLOCK, &blockmask, NULL) == -1) {
                fprintf(stderr, "sigprocmask error %s", strerror(errno));
                exit(-1);
            }

            if( front == START_BACKEND ){
                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
            }

            break;
        }
    }

    return 0;
}


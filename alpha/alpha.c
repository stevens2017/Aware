#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <getopt.h>
#include <netdb.h>
#include <sys/sysinfo.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "al_ip.h"
#include "al_pktdump.h"
#include "alpha.h"
#include "al_log.h"
#include "al_macport.h"
#include "al_time.h"
#include "statistics/statistics.h"
#include "al_vport.h"
#include "al_dev.h"
#include "al_deamon.h"
#include "al_to_api.h"
#include "al_to_main.h"

#define AL_DEFAULT_WDIR  "/var"
#define AL_DEFAULT_LOCK  "/var/alpha.lock"

struct thread_info   g_thread_info[RTE_MAX_LCORE];
struct rte_ring*     g_thread_ring[RTE_MAX_LCORE][RTE_MAX_LCORE];
int     g_max_queue_counter=0;
uint8_t g_has_llb=0;

uint8_t g_sys_stop=1;
__thread uint8_t     g_lcoreid;

static int front=START_BACKEND;
static int act=ACT_START;
char* g_wkdir=AL_DEFAULT_WDIR;
static char* locker=AL_DEFAULT_LOCK;

int mbuf_size=sizeof(struct rte_mbuf);
static void core_parament(char*  ptr);
static int parse_args(int argc, char** args);

int main(int argc, char *argv[]){

    int i, ret;
    char* pgv[10];

    char prg_name[1024];
    char pgptr[1024];
    char pgcptr[16];
    char pgcnt[36];
    char pgnptr[16];
    char pgncnt[16];

    if(parse_args(argc, argv) < -1 ){
        fprintf(stderr, "Argument parse failed");
        return -1;
    }

    main_loop(front, act, g_wkdir, locker);

    sprintf(prg_name, "%s", argv[0]);
    sprintf(pgptr, "%s", basename(prg_name));
    pgv[0]=pgptr;
    
    sprintf(pgcptr, "-c" );
    pgv[1]=pgcptr;
    core_parament(pgcnt);
    pgv[2]=pgcnt;
    sprintf(pgnptr, "-n");
    pgv[3]=pgnptr;
    sprintf(pgncnt, "1");
    pgv[4]=pgncnt;

    g_sys_time=time(NULL);

    if(time_init() != 0 ){
        return -1;
    }

    if(al_db_initial("/usr/local/aware", 0)){
        fprintf(stderr, "Init conf db failed");
        return  -1;
    }
        
    ret=al_init_dpdk(5, pgv);
    if( ret != 0 ){
        return ret;
    }
    
//    lwip_init();

    //udp_init();
//    tcp_init();

    if( statistics_open(0) == -1 ){
        fprintf(stderr, "Init statistics\n");
        return -1;
    } 
	
    if(al_init_dump() != 0){
        fprintf(stderr, "Init message dump failed\n");
        return -1;
    }
	
#if 0
    //init log
    int logfile = open("/var/log/lwip.log", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if( logfile == -1 ){
        fprintf(stderr, "Open log file failed, %s\n", strerror(errno));
        return -1;
    }

    if( dup2(logfile,STDOUT_FILENO) == -1 ){
        fprintf(stderr, "Dup2 log file failed, %s\n", strerror(errno));
        return -1;
    }else{
        close(logfile);
    }
#endif

    rte_eal_mp_remote_launch(al_mainloop, NULL, CALL_MASTER);

    RTE_LCORE_FOREACH(i) {
        if (rte_eal_wait_lcore(i) < 0){
            return -1;
        }
    }
        
    return 0;
}

static void core_parament(char*  ptr){
    int rc=get_nprocs();
    int c = rc%4,  w = rc/4, i;

    *ptr++='0';
    *ptr++='x';

    for( i=0; i<w; i++){
        *ptr++='F';
    }
    
    if( c == 3 ){
        *ptr++ = 'E';
    }else if( c == 2 ){
        *ptr++ = '6';
    }else if( c == 1 ){
        *ptr++ = '2';
    }

    *ptr='\0';
}

static int parse_args(int argc, char** args){

    int c;
    char* const short_options = "fs:w:l:";
    struct option long_options[] =
    {
            {"deamon",     0, NULL, 'f'},
            {"action",        1, NULL, 's'},      
            {"workspace", 1, NULL, 'w'},
            {"locker",       1, NULL, 'l'},
            {0, 0, 0, 0},      
    };

    while((c = getopt_long (argc, args, short_options, long_options, NULL)) != -1){
         switch (c)
         {
            case 'f':
                front=START_FRONT;
                break;
            case 's':
                if(strcmp(optarg, "start") == 0 ){
                    act = ACT_START;
                }else if(strcmp(optarg, "stop") == 0 ){
                    act = ACT_SHUTDOWN;
                }else if(strcmp(optarg, "reload") == 0 ){
                    act = ACT_RELOAD;
                }else{
                    return -1;
                }
                break;
            case 'w':
               g_wkdir = optarg;
               break;
            case 'l':
               locker = optarg;
               break;
         }
    }

    return 0;
}


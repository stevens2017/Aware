#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/prctl.h>
#include <rte_mbuf.h>
#include "al_common.h"
#include "alpha.h"
#include "al_api.h"
#include "al_macport.h"
#include "al_log.h"
#include "al_dev.h"
#include "common/al_nic.h"
#include "al_to_api.h"
#include "al_to_main.h"
#include "al_errmsg.h"

static void* timer_proc(void* args);
static void* poll_outlog_proc(void* args);
static void* load_nic_mac(void* args);
static void* load_ip_proc(void* args);

int al_master_process(void* UNUSED(args)){
    
    pthread_t p_thread1, p_thread2, p_thread3, p_thread4;
    int rc=0;

    prctl(PR_SET_NAME, "aware");
        
    rc=pthread_create(&p_thread1, NULL, (void*)poll_outlog_proc, NULL);
    if( rc == -1 ){
        return rc;
    }

    rc=pthread_create(&p_thread2, NULL, (void*)timer_proc, NULL);
    if( rc == -1 ){
        return rc;
    }

    rc=pthread_create(&p_thread3, NULL, (void*)load_ip_proc, NULL);
    if( rc == -1 ){
        return rc;
    }
    
    rc=pthread_create(&p_thread4, NULL, (void*)load_nic_mac, NULL);
    if( rc == -1 ){
        return rc;
    }
    
    /*load lb configure*/
    al_load_configure();
	
    while(g_sys_stop){
        al_start_api("0.0.0.0", 80);
    }
    
    return rc;
}

static void* timer_proc(void* UNUSED(args)){

    prctl(PR_SET_NAME, "timer");
    while(g_sys_stop){
        g_sys_time=time(NULL);
        clock_gettime(CLOCK_REALTIME, &g_timeval);
        g_now_micrsec=g_timeval.tv_sec*1000 + g_timeval.tv_nsec/1000000;
        portid_clear();
    }

    return NULL;
}

#define SYSLOGLEN 128

static void* poll_outlog_proc(void* UNUSED(args)){

    int fd = -1;
    int st = 0;
    off_t off;
    char buf[SYSLOGLEN];
    char logfilet[32];
    time_t t;

    prctl(PR_SET_NAME, "log");
    while(g_sys_stop){
        if( fd == -1 ){
            t=time(NULL);
	    strftime(logfilet, sizeof(logfilet), "%S:%M:%H-%Y-%m-%d", localtime(&t));
            sprintf(buf, "%s/alpha%s.log", g_wkdir, logfilet);
            fd=open(buf, O_WRONLY | O_CREAT, 0755);
            if( fd == -1 ){
                fprintf(stderr, "Log file open failed, %s\n", strerror(errno));
                continue;
            }else{
                st=0;
            }
        }
		
        readlog(fd);
        if((off=lseek(fd, 0, SEEK_CUR)) == ((off_t) -1)){
            fprintf(stderr, "Log file open failed, %s\n", strerror(errno));
			st=1;
        }else if( off > 100*1024*1024 ){
            st=1;
        }

        if(st == 1){
	    close(fd);
            fd=-1;
	    st=0;
	}
    }

    close(fd);
    return NULL;
}

static void* load_nic_mac(void* UNUSED(args)){
    int i;
    prctl(PR_SET_NAME, "sync_route");
start:
    sleep(1);
    for ( i=0; i < RTE_MAX_LCORE; i++){
        if(g_thread_info[i].used){
            if( !g_thread_info[i].started ){
                goto start;   
            }
        }
    }
#if 0    
    for(i=0; i<g_sys_port_counter; i++){
        if( al_nic_up_and_running((char*)g_vport_ary[i].dev.vport.port.ifname)){
            LOG(LOG_EMERG, LAYOUT_CONF, "Set nic %s up failed", g_vport_ary[i].dev.vport.port.ifname);
        }else{
            LOG(LOG_INFO, LAYOUT_CONF, "Set nic %s up success", g_vport_ary[i].dev.vport.port.ifname);
        }
    }
#endif

    if(al_mac_load()){
        LOG(LOG_INFO, LAYOUT_CONF, "Set nic up failed");
    }
    
    if(al_watch_route() == -1){
        LOG(LOG_EMERG, LAYOUT_CONF, "Sync route failed");
        return NULL;
    }

    return NULL;
}

static void* load_ip_proc(void* UNUSED(args)){
    int i;
    prctl(PR_SET_NAME, "load_ip");
    
start:
    sleep(1);
    for ( i=0; i < RTE_MAX_LCORE; i++){
        if(g_thread_info[i].used){
            if( !g_thread_info[i].start_lp ){
                goto start;   
            }
        }
    }

    if( al_ip_load() ){
        LOG(LOG_EMERG, LAYOUT_CONF, "Load ip failed, msg:%s", al_get_errmsg(errno));
    }
    
    return NULL;
}


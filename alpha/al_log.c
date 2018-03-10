#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <stdarg.h>
#include <unistd.h>
#include "al_common.h"
#include "al_log.h"

static struct log_memory* g_log_mem;

static const char *const log_level_desc[]={
    "ERMARG",
    "FETAL",
    "ERROR",
    "WORN",
    "TRACE",
    "INFO",
    "DEBUG1",
    "DEBUG2",
    NULL
};

static const char* const layout_desc[]={
    "dpdk",
    "app",
    "datalink",
    "http",
    "switch",
    "conf",
    "arp",
    "ip",
    "tcp_proxy",
    "tcp"
};


int init_log(void){

    /*allocate port struct*/
    uint8_t* logptr=(uint8_t*)rte_zmalloc("log_mem", LOG_MEM_SIZE, CACHE_ALIGN);
    if(logptr == NULL ){
        fprintf(stderr, "Allocate core dev memory space failed, %s", rte_strerror(rte_errno));
        return -1;
    }else{
        g_log_mem=(struct log_memory*)logptr;
        g_log_mem->log=logptr+sizeof(struct log_memory);
        g_log_mem->head=g_log_mem->tail=0;
        g_log_mem->lock=0;
        g_log_mem->count=(LOG_MEM_SIZE/LOG_LINE_MAXLEN);
        return 0;
    }
    
};

void log_core(int level, int module, const char* frmt, ...){
    
    char  *ptr;
    struct core_log* log;
    va_list  args;

    while(!CAS(&g_log_mem->lock, 0, 1));
    if((g_log_mem->tail+1)%g_log_mem->count == g_log_mem->head){
		
        g_log_mem->head=(g_log_mem->head+1)%g_log_mem->count;
        log=(struct core_log*)(g_log_mem->log+LOG_LINE_MAXLEN*g_log_mem->tail);
	
        log->time=g_sys_time;
        log->level=level;
        log->module=module;
	
        ptr=(char*)log+sizeof(struct core_log);
		
        va_start(args, frmt);
        log->msglen=vsnprintf((char*)ptr, MSG_ENTRYLEN, frmt, args);
        va_end(args);
		
    }else{
    
        log=(struct core_log*)(g_log_mem->log+LOG_LINE_MAXLEN*g_log_mem->tail);
        log->time=g_sys_time;
        log->level=level;
        log->module=module;
	
        ptr=(char*)log+sizeof(struct core_log);
		
        va_start(args, frmt);
        log->msglen=vsnprintf((char*)ptr, MSG_ENTRYLEN, frmt, args);
        va_end(args);
        
    }
	g_log_mem->tail=(g_log_mem->tail+1)%g_log_mem->count;
    g_log_mem->lock=0;
}


int readlog(int fd){

    struct core_log* log;
    char buf[LOG_LINE_MAXLEN];
    char buf1[LOG_LINE_MAXLEN];

    struct tm tm1;
    
    while(!CAS(&g_log_mem->lock, 0, 1));
    while(g_log_mem->tail != g_log_mem->head){
        log=(struct core_log*)(g_log_mem->log+g_log_mem->head*LOG_LINE_MAXLEN);

        localtime_r(&log->time, &tm1);
        sprintf( buf1, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
                 tm1.tm_year+1900, tm1.tm_mon+1, tm1.tm_mday,tm1.tm_hour, tm1.tm_min,tm1.tm_sec);
        
        sprintf(buf, "%s (%s)[%s]", buf1, layout_desc[log->module], log_level_desc[log->level]);
        write(fd, buf, strlen(buf));
        write(fd, (char*)log+MSG_ENTRY_OFF, log->msglen);
        write(fd, "\n", 1);
        g_log_mem->head=(g_log_mem->head+1)%g_log_mem->count;      
    }
    g_log_mem->lock=0;

    fdatasync(fd);
    return 0;
}



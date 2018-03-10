#ifndef _AL_LOG_H_
#define _AL_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <inttypes.h>
#include "al_common.h"

#define LOG_LINE_MAXLEN 1024

#define LOG_EMERG    0
#define LOG_FETAL    1
#define LOG_ERROR    2
#define LOG_WORN     3
#define LOG_TRACE    4
#define LOG_INFO     5
#define LOG_DEBUG1   6
#define LOG_DEBUG2   7

/*module type*/
#define LAYOUT_DPDK 0
#define LAYOUT_APP  1
#define LAYOUT_DATALINK 2
#define LAYOUT_HTTP   3
#define LAYOUT_SWITCH 4
#define LAYOUT_CONF   5
#define LAYOUT_ARP    6
#define LAYOUT_IP     7
#define LAYOUT_TCP_PROXY 8
#define LAYOUT_TCP    9


#define LOG_MEM_SIZE 1024*1024*2

struct log_memory{
   uint32_t     head;
   uint32_t     tail;
   uint32_t     count;
   uint8_t      lock;
   uint8_t      *log;
}STRUCT_ALIGNED(CACHE_ALIGN);

struct core_log{
    time_t     time;
    uint8_t    level:3;
    uint8_t    module:5;
    uint16_t   msglen;
}__attribute__((__packed__));

#define MSG_ENTRY_OFF     sizeof(struct core_log)
#define MSG_ENTRYLEN      (LOG_LINE_MAXLEN - MSG_ENTRY_OFF)


#define LOG(level, module, format, ...)    \
    log_core(level, module, format, ##__VA_ARGS__)

extern void log_core(int level, int module, const char* format, ...);
int init_log(void);
int readlog(int fd);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_AL_LOG_H_*/


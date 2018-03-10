#include "alpha.h"

long g_now_micrsec=0;
static long g_start_micrsec=0;
struct timespec g_timeval;
time_t g_sys_time;

int time_init(void){

    g_sys_time=time(NULL);
    clock_gettime(CLOCK_REALTIME, &g_timeval);
    g_now_micrsec=g_timeval.tv_sec*1000 + g_timeval.tv_nsec/1000000;
    g_start_micrsec=g_now_micrsec=g_timeval.tv_sec*1000 + g_timeval.tv_nsec/1000000;
    
    return 0;
}

uint32_t sys_now(void)
{
  return g_now_micrsec - g_start_micrsec;
}


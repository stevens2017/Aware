#ifndef _ALPHA_H_
#define _ALPHA_H_

#include <rte_mbuf.h>

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

/* Max size of a single packet */
#define MAX_PACKET_SZ           2048

/* Number of bytes needed for each mbuf */
#define MBUF_SZ \
        (MAX_PACKET_SZ + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

/* Number of mbufs in mempool that is created */
#define AL_NB_MBUF                 4096

/* How many packets to attempt to read from NIC in one go */
#define PKT_BURST_SZ            64

/* How many objects (mbufs) to keep in per-lcore mempool cache */
#define MEMPOOL_CACHE_SZ        PKT_BURST_SZ

#define PORT_MAX_QUEUE          64

#define AL_MAX_EHERPORTS 128

extern long g_now_micrsec;
extern uint8_t g_sys_stop;
extern struct timespec g_timeval;
extern int g_sys_port_counter;

#define FLAG_OUTPUT_SESSION 0x01

struct thread_info{
    uint8_t   lcoreid;
    uint8_t   used:1;
    uint8_t   started:1;
    /*should dump session tables*/
    uint8_t   oss:1;
    /*should load ip*/
    uint8_t   start_lp:1;
    uint8_t   :4;
    int (*proc)(void* args);
};

extern struct thread_info  g_thread_info[RTE_MAX_LCORE];
extern struct rte_ring*    g_thread_ring[RTE_MAX_LCORE][RTE_MAX_LCORE];
extern               int   g_max_queue_counter;

extern int g_work_thread_count;
extern int g_max_port;

extern int g_max_queue_counter;
extern uint8_t g_sys_stop;
extern uint8_t g_has_llb;
extern char* g_wkdir;
extern int g_obj_max_counter;

extern const struct rte_memzone *g_queue_pool;

int al_master_process(void* args);
int al_work_proc(void* args);
int al_main_process(uint8_t lcoreid, uint8_t portid, struct rte_mbuf **mbuf, int cnt, uint8_t from, uint8_t type);
int al_init_dpdk(int argc, char *argv[]);
void al_load_configure(void);
int al_mainloop(void* args);
int al_watch_route(void);
int al_sync_route(void);
#endif

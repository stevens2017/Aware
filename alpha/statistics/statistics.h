#ifndef __STATUS_H__
#define __STATUS_H__

#include <inttypes.h>

struct sys_statistics{
    //new connection
    uint64_t new_conn;
    //close session
    uint64_t close_conn;
    //listen reset
    uint64_t acc_rst;
    //no service drop pkg
    uint64_t nos_drop;

    //recieved buffer counter
    uint64_t in_mbuf;
    //output buffer counter
    uint64_t out_mbuf;

    //too many pkg for proxy,default three
    uint64_t mbuf_refuse_by_proxy;

    //create new segment failed
    uint64_t segment_create_failed;
};

extern struct sys_statistics* g_stat;

int statistics_close(void);
int statistics_open(int ind);

//tcp
#define SYS_NEW_CONNECTION                      \
    __sync_add_and_fetch(&g_stat->new_conn, 1)

#define SYS_CLOSE_CONNECTION                            \
    __sync_add_and_fetch(&g_stat->close_conn, 1);

#define SYS_ACC_RST                             \
    __sync_add_and_fetch(&g_stat->acc_rst, 1)

#define SYS_NOS_DROP                            \
    __sync_add_and_fetch(&g_stat->nos_drop, 1)

#define SYS_INPUT_BUF                           \
    __sync_add_and_fetch(&g_stat->in_mbuf, 1)

#define SYS_OUTPUT_BUF                          \
    __sync_add_and_fetch(&g_stat->out_mbuf, 1)

#define SYS_PROXY_REFUSE_DATA                                   \
    __sync_add_and_fetch(&g_stat->mbuf_refuse_by_proxy, 1)

#define SYS_PROXY_CREATE_NEW_SEGMENT_FAILED                     \
    __sync_add_and_fetch(&g_stat->segment_create_failed, 1)



#endif /*__STATUS_H__*/


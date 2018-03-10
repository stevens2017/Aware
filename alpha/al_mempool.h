#ifndef _AL_MEMPOOL_H_
#define _AL_MEMPOOL_H_

#include <rte_mbuf.h>
#include <rte_mempool.h>

extern struct rte_mempool *al_pktmbuf_pool;

int al_mempool_init();

#define GET_RTE_MBUF()  rte_pktmbuf_alloc(al_pktmbuf_pool)


void* al_malloc(size_t size);
void* al_calloc(size_t size);
void al_free(void* p);

#endif

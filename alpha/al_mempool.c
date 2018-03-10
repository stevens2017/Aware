#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "al_common.h"
#include "al_mempool.h"
#include "alpha.h"
#include "al_slab.h"


struct rte_mempool* al_pktmbuf_pool;
al_slab_pool_t* al_slab_pool;

static const struct rte_memzone* al_slab_mempool;

static int al_dynamic_mempool_init(void);

int al_mempool_init()
{
	al_pktmbuf_pool= rte_pktmbuf_pool_create("al_mbuf_pool", AL_NB_MBUF, 64,
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (!al_pktmbuf_pool){
		return -1;
	}

	return al_dynamic_mempool_init();
}

static int al_dynamic_mempool_init(void){

#define SLAB_POOL_SIZE (100*1024*1024)

    al_slab_mempool = rte_memzone_reserve_aligned("al_slab_pool", SLAB_POOL_SIZE, SOCKET_ID_ANY, RTE_MEMZONE_SIZE_HINT_ONLY, CACHE_ALIGN);
    if( al_slab_mempool == NULL ){
        fprintf(stderr, "Allocate member_pool failed\n");
        return -1;
    }else{
        AL_SLAB_HEAD_INIT(al_slab_pool, (uint8_t*)al_slab_mempool->addr, SLAB_POOL_SIZE);
    }

    return 0;
}

void* al_malloc(size_t size){
    return al_slab_alloc(al_slab_pool, size);
}

void* al_calloc(size_t size){
    return al_slab_calloc(al_slab_pool, size);    
}

void al_free(void* p){
    al_slab_free(al_slab_pool, p);
}


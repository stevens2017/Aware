#ifndef _SLAB_H_INCLUDED_
#define _SLAB_H_INCLUDED_

typedef struct al_slab_page_s  al_slab_page_t;

struct al_slab_page_s {
    uintptr_t         slab;
    al_slab_page_t  *next;
    uintptr_t         prev;
};


typedef struct {
    uintptr_t        total;
    uintptr_t        used;

    uintptr_t        reqs;
    uintptr_t        fails;
} al_slab_stat_t;


typedef struct {
    rte_spinlock_t  lock;

    size_t            min_size;
    size_t            min_shift;

    al_slab_page_t  *pages;
    al_slab_page_t  *last;
    al_slab_page_t   free;

    al_slab_stat_t  *stats;
    uintptr_t        pfree;

    u_char           *start;
    u_char           *end;

    u_char           *log_ctx;
    u_char            zero;

    void             *data;
    void             *addr;
} al_slab_pool_t;

#define AL_SLAB_HEAD_INIT(pool, pstart, len)   \
    pool = (al_slab_pool_t *)pstart; \
    pool->min_shift = 6;  \
    pool->addr = pstart; \
    pool->end = (uint8_t*)pstart + len; \
    al_slab_init(pool)

void al_slab_init(al_slab_pool_t *pool);
void *al_slab_alloc(al_slab_pool_t *pool, size_t size);
void *al_slab_alloc_locked(al_slab_pool_t *pool, size_t size);
void *al_slab_calloc(al_slab_pool_t *pool, size_t size);
void *al_slab_calloc_locked(al_slab_pool_t *pool, size_t size);
void al_slab_free(al_slab_pool_t *pool, void *p);
void al_slab_free_locked(al_slab_pool_t *pool, void *p);

#endif


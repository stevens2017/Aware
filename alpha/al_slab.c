#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <rte_spinlock.h>
#include "al_common.h"
#include "al_slab.h"

#define AL_SLAB_PAGE_MASK   3
#define AL_SLAB_PAGE        0
#define AL_SLAB_BIG         1
#define AL_SLAB_EXACT       2
#define AL_SLAB_SMALL       3


#define AL_SLAB_PAGE_FREE   0
#define AL_SLAB_PAGE_BUSY   0xffffffffffffffff
#define AL_SLAB_PAGE_START  0x8000000000000000

#define AL_SLAB_SHIFT_MASK  0x000000000000000f
#define AL_SLAB_MAP_MASK    0xffffffff00000000
#define AL_SLAB_MAP_SHIFT   32

#define AL_SLAB_BUSY        0xffffffffffffffff


#define al_slab_slots(pool)                                                  \
    (al_slab_page_t *) ((u_char *) (pool) + sizeof(al_slab_pool_t))

#define al_slab_page_type(page)   ((page)->prev & AL_SLAB_PAGE_MASK)

#define al_slab_page_prev(page)                                              \
    (al_slab_page_t *) ((page)->prev & ~AL_SLAB_PAGE_MASK)

#define al_slab_page_addr(pool, page)                                        \
    ((((page) - (pool)->pages) << al_pagesize_shift)                         \
     + (uintptr_t) (pool)->start)




#define al_slab_junk(p, size)


static al_slab_page_t *al_slab_alloc_pages(al_slab_pool_t *pool,
    uintptr_t pages);
static void al_slab_free_pages(al_slab_pool_t *pool, al_slab_page_t *page,
    uintptr_t pages);


static uintptr_t  al_slab_max_size;
static uintptr_t  al_slab_exact_size;
static uintptr_t  al_slab_exact_shift;

//static uintptr_t al_cacheline_size = 64;
static uintptr_t al_pagesize_shift=0;


void
al_slab_init(al_slab_pool_t *pool)
{
    u_char           *p;
    size_t            size;
    intptr_t          m;
    uintptr_t        i, n, pages, k;
    al_slab_page_t  *slots, *page;

    /* STUB */
    if (al_slab_max_size == 0) {
        al_slab_max_size = PAGESIZE / 2;
        al_slab_exact_size = PAGESIZE / (8 * sizeof(uintptr_t));
        for (n = al_slab_exact_size; n >>= 1; al_slab_exact_shift++) {
            /* void */
        }
        
        for (k = PAGESIZE; k >>= 1; al_pagesize_shift++) { /* void */ }
    }
    /**/

    pool->min_size = (size_t) 1 << pool->min_shift;

    slots = al_slab_slots(pool);

    p = (u_char *) slots;
    size = pool->end - p;

    al_slab_junk(p, size);

    n = al_pagesize_shift - pool->min_shift;

    for (i = 0; i < n; i++) {
        /* only "next" is used in list head */
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(al_slab_page_t);

    pool->stats = (al_slab_stat_t *) p;
    memset(pool->stats, '\0', n * sizeof(al_slab_stat_t));

    p += n * sizeof(al_slab_stat_t);

    size -= n * (sizeof(al_slab_page_t) + sizeof(al_slab_stat_t));

    pages = (uintptr_t) (size / (PAGESIZE + sizeof(al_slab_page_t)));

    pool->pages = (al_slab_page_t *) p;
    memset(pool->pages, '\0', pages * sizeof(al_slab_page_t));

    page = pool->pages;

    /* only "next" is used in list head */
    pool->free.slab = 0;
    pool->free.next = page;
    pool->free.prev = 0;

    page->slab = pages;
    page->next = &pool->free;
    page->prev = (uintptr_t) &pool->free;

    pool->start = al_align_ptr(p + pages * sizeof(al_slab_page_t), PAGESIZE);

    m = pages - (pool->end - pool->start) / PAGESIZE;
    if (m > 0) {
        pages -= m;
        page->slab = pages;
    }

    pool->last = pool->pages + pages;
    pool->pfree = pages;

   rte_spinlock_init(&pool->lock);

}


void *
al_slab_alloc(al_slab_pool_t *pool, size_t size)
{
    void  *p;
    rte_spinlock_lock(&pool->lock);
    p = al_slab_alloc_locked(pool, size);
    rte_spinlock_unlock(&pool->lock);    

    return p;
}


void *
al_slab_alloc_locked(al_slab_pool_t *pool, size_t size)
{
    size_t            s;
    uintptr_t         p, n, m, mask, *bitmap;
    uintptr_t        i, slot, shift, map;
    al_slab_page_t  *page, *prev, *slots;

    if (size > al_slab_max_size) {

        page = al_slab_alloc_pages(pool, (size >> al_pagesize_shift)
                                          + ((size % PAGESIZE) ? 1 : 0));
        if (page) {
            p = al_slab_page_addr(pool, page);

        } else {
            p = 0;
        }

        goto done;
    }

    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        shift = pool->min_shift;
        slot = 0;
    }

    pool->stats[slot].reqs++;

    slots = al_slab_slots(pool);
    page = slots[slot].next;

    if (page->next != page) {

        if (shift < al_slab_exact_shift) {

            bitmap = (uintptr_t *) al_slab_page_addr(pool, page);

            map = (PAGESIZE >> shift) / (sizeof(uintptr_t) * 8);

            for (n = 0; n < map; n++) {

                if (bitmap[n] != AL_SLAB_BUSY) {

                    for (m = 1, i = 0; m; m <<= 1, i++) {
                        if (bitmap[n] & m) {
                            continue;
                        }

                        bitmap[n] |= m;

                        i = (n * sizeof(uintptr_t) * 8 + i) << shift;

                        p = (uintptr_t) bitmap + i;

                        pool->stats[slot].used++;

                        if (bitmap[n] == AL_SLAB_BUSY) {
                            for (n = n + 1; n < map; n++) {
                                if (bitmap[n] != AL_SLAB_BUSY) {
                                    goto done;
                                }
                            }

                            prev = al_slab_page_prev(page);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = AL_SLAB_SMALL;
                        }

                        goto done;
                    }
                }
            }

        } else if (shift == al_slab_exact_shift) {

            for (m = 1, i = 0; m; m <<= 1, i++) {
                if (page->slab & m) {
                    continue;
                }

                page->slab |= m;

                if (page->slab == AL_SLAB_BUSY) {
                    prev = al_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = AL_SLAB_EXACT;
                }

                p = al_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }

        } else { /* shift > ngx_slab_exact_shift */

            mask = ((uintptr_t) 1 << (PAGESIZE >> shift)) - 1;
            mask <<= AL_SLAB_MAP_SHIFT;

            for (m = (uintptr_t) 1 << AL_SLAB_MAP_SHIFT, i = 0;
                 m & mask;
                 m <<= 1, i++)
            {
                if (page->slab & m) {
                    continue;
                }

                page->slab |= m;

                if ((page->slab & AL_SLAB_MAP_MASK) == mask) {
                    prev = al_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = AL_SLAB_BIG;
                }

                p = al_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }
        }

    }

    page = al_slab_alloc_pages(pool, 1);

    if (page) {
        if (shift < al_slab_exact_shift) {
            bitmap = (uintptr_t *) al_slab_page_addr(pool, page);

            n = (PAGESIZE >> shift) / ((1 << shift) * 8);

            if (n == 0) {
                n = 1;
            }

            /* "n" elements for bitmap, plus one requested */
            bitmap[0] = ((uintptr_t) 2 << n) - 1;

            map = (PAGESIZE >> shift) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
                bitmap[i] = 0;
            }

            page->slab = shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | AL_SLAB_SMALL;

            slots[slot].next = page;

            pool->stats[slot].total += (PAGESIZE >> shift) - n;

            p = al_slab_page_addr(pool, page) + (n << shift);

            pool->stats[slot].used++;

            goto done;

        } else if (shift == al_slab_exact_shift) {

            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | AL_SLAB_EXACT;

            slots[slot].next = page;

            pool->stats[slot].total += sizeof(uintptr_t) * 8;

            p = al_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;

        } else { /* shift > ngx_slab_exact_shift */

            page->slab = ((uintptr_t) 1 << AL_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | AL_SLAB_BIG;

            slots[slot].next = page;

            pool->stats[slot].total += PAGESIZE >> shift;

            p = al_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;
        }
    }

    p = 0;

    pool->stats[slot].fails++;

done:

    return (void *) p;
}


void *
al_slab_calloc(al_slab_pool_t *pool, size_t size)
{
    void  *p;
    rte_spinlock_lock(&pool->lock);
    p = al_slab_calloc_locked(pool, size);
    rte_spinlock_unlock(&pool->lock);    
    return p;
}


void *
al_slab_calloc_locked(al_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = al_slab_alloc_locked(pool, size);
    if (p) {
        memset(p, '\0',  size);
    }

    return p;
}


void
al_slab_free(al_slab_pool_t *pool, void *p)
{
    rte_spinlock_lock(&pool->lock);
    al_slab_free_locked(pool, p);
    rte_spinlock_unlock(&pool->lock);    
}


void
al_slab_free_locked(al_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    uintptr_t        i, n, type, slot, shift, map;
    al_slab_page_t  *slots, *page;

    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        goto fail;
    }

    n = ((u_char *) p - pool->start) >> al_pagesize_shift;
    page = &pool->pages[n];
    slab = page->slab;
    type = al_slab_page_type(page);

    switch (type) {

    case AL_SLAB_SMALL:

        shift = slab & AL_SLAB_SHIFT_MASK;
        size = (size_t) 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        n = ((uintptr_t) p & (PAGESIZE - 1)) >> shift;
        m = (uintptr_t) 1 << (n % (sizeof(uintptr_t) * 8));
        n /= sizeof(uintptr_t) * 8;
        bitmap = (uintptr_t *)
                             ((uintptr_t) p & ~((uintptr_t) PAGESIZE - 1));

        if (bitmap[n] & m) {
            slot = shift - pool->min_shift;

            if (page->next == NULL) {
                slots = al_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | AL_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | AL_SLAB_SMALL;
            }

            bitmap[n] &= ~m;

            n = (PAGESIZE >> shift) / ((1 << shift) * 8);

            if (n == 0) {
                n = 1;
            }

            if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                goto done;
            }

            map = (PAGESIZE >> shift) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
                if (bitmap[i]) {
                    goto done;
                }
            }

            al_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= (PAGESIZE >> shift) - n;

            goto done;
        }

        goto chunk_already_free;

    case AL_SLAB_EXACT:

        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (PAGESIZE - 1)) >> al_slab_exact_shift);
        size = al_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        if (slab & m) {
            slot = al_slab_exact_shift - pool->min_shift;

            if (slab == AL_SLAB_BUSY) {
                slots = al_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | AL_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | AL_SLAB_EXACT;
            }

            page->slab &= ~m;

            if (page->slab) {
                goto done;
            }

            al_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= sizeof(uintptr_t) * 8;

            goto done;
        }

        goto chunk_already_free;

    case AL_SLAB_BIG:

        shift = slab & AL_SLAB_SHIFT_MASK;
        size = (size_t) 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        m = (uintptr_t) 1 << ((((uintptr_t) p & (PAGESIZE - 1)) >> shift)
                              + AL_SLAB_MAP_SHIFT);

        if (slab & m) {
            slot = shift - pool->min_shift;

            if (page->next == NULL) {
                slots = al_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | AL_SLAB_BIG;
                page->next->prev = (uintptr_t) page | AL_SLAB_BIG;
            }

            page->slab &= ~m;

            if (page->slab & AL_SLAB_MAP_MASK) {
                goto done;
            }

            al_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= PAGESIZE >> shift;

            goto done;
        }

        goto chunk_already_free;

    case AL_SLAB_PAGE:

        if ((uintptr_t) p & (PAGESIZE - 1)) {
            goto wrong_chunk;
        }

        if (!(slab & AL_SLAB_PAGE_START)) {
            goto fail;
        }

        if (slab == AL_SLAB_PAGE_BUSY) {
            goto fail;
        }

        n = ((u_char *) p - pool->start) >> al_pagesize_shift;
        size = slab & ~AL_SLAB_PAGE_START;

        al_slab_free_pages(pool, &pool->pages[n], size);

        al_slab_junk(p, size << al_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    pool->stats[slot].used--;

    al_slab_junk(p, size);

    return;

wrong_chunk:


    goto fail;

chunk_already_free:


fail:

    return;
}


static al_slab_page_t *
al_slab_alloc_pages(al_slab_pool_t *pool, uintptr_t pages)
{
    al_slab_page_t  *page, *p;

    for (page = pool->free.next; page != &pool->free; page = page->next) {

        if (page->slab >= pages) {

            if (page->slab > pages) {
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (al_slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            } else {
                p = (al_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | AL_SLAB_PAGE_START;
            page->next = NULL;
            page->prev = AL_SLAB_PAGE;

            pool->pfree -= pages;

            if (--pages == 0) {
                return page;
            }

            for (p = page + 1; pages; pages--) {
                p->slab = AL_SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = AL_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    return NULL;
}


static void
al_slab_free_pages(al_slab_pool_t *pool, al_slab_page_t *page,
    uintptr_t pages)
{
    al_slab_page_t  *prev, *join;

    pool->pfree += pages;

    page->slab = pages--;

    if (pages) {
        memset(&page[1], '\0', pages * sizeof(al_slab_page_t));
    }

    if (page->next) {
        prev = al_slab_page_prev(page);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    join = page + page->slab;

    if (join < pool->last) {

        if (al_slab_page_type(join) == AL_SLAB_PAGE) {

            if (join->next != NULL) {
                pages += join->slab;
                page->slab += join->slab;

                prev = al_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = AL_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = AL_SLAB_PAGE;
            }
        }
    }

    if (page > pool->pages) {
        join = page - 1;

        if (al_slab_page_type(join) == AL_SLAB_PAGE) {

            if (join->slab == AL_SLAB_PAGE_FREE) {
                join = al_slab_page_prev(join);
            }

            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;

                prev = al_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                page->slab = AL_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = AL_SLAB_PAGE;

                page = join;
            }
        }
    }

    if (pages) {
        page[pages].prev = (uintptr_t) page;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


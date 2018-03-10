#ifndef _HASH_H_
#define _HASH_H_

#include <rte_atomic.h>
#include <rte_rwlock.h>

#include "hlist.h"

struct hash_table_nolock{
	struct hlist_head list;
};

struct hash_table_lock{ 
        struct hlist_head list;
        rte_rwlock_t lock;
};

#endif /*_HASH_H_*/


#ifndef _BIT_H_
#define _BIT_H_

#include<limits.h>

#define BITMASK(b) 	    (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) 	    ((b) / CHAR_BIT)
#define BITSET(a, b) 	((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b)	((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b)	((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb)	((nb + CHAR_BIT - 1) / CHAR_BIT)

#endif

#ifndef AL_RADIX_H
#define AL_RADIX_H

#include <rte_rwlock.h>
/*
 * Radix search tree node layout.
 */

struct radix_node {
	struct	radix_mask *rn_mklist;	/* list of masks contained in subtree */
	struct	radix_node *rn_parent;	/* parent */
	short	rn_bit;			        /* bit offset; -1-index(netmask) */
	char	rn_bmask;		                        /* node: mask for bit test*/
	u_char	rn_flags;		                /* enumerated next */
#define RNF_NORMAL	1		/* leaf contains normal route */
#define RNF_ROOT	2		/* leaf is root leaf for tree */
#define RNF_ACTIVE	4		/* This node is alive (for rtfree) */
	union {
		struct {			                /* leaf only data: */
			caddr_t	rn_Key;		/* object of search */
			caddr_t	rn_Mask;	       /* netmask, if present */
			struct	radix_node *rn_Dupedkey;
		} rn_leaf;
		struct {			                  /* node only data: */
			int	rn_Off;		          /* where to start compare */
			struct	radix_node *rn_L;/* progeny */
			struct	radix_node *rn_R;/* progeny */
		} rn_node;
	}		rn_u;
#ifdef RN_DEBUG
	int rn_info;
	struct radix_node *rn_twin;
	struct radix_node *rn_ybro;
#endif
};

#define	rn_dupedkey	rn_u.rn_leaf.rn_Dupedkey
#define	rn_key		rn_u.rn_leaf.rn_Key
#define	rn_mask		rn_u.rn_leaf.rn_Mask
#define	rn_offset	rn_u.rn_node.rn_Off
#define	rn_left		rn_u.rn_node.rn_L
#define	rn_right	rn_u.rn_node.rn_R

/*
 * Annotations to tree concerning potential routes applying to subtrees.
 */

struct radix_mask {
	short	rm_bit;			         /* bit offset; -1-index(netmask) */
	char	rm_unused;		                /* cf. rn_bmask */
	u_char	rm_flags;		                /* cf. rn_flags */
	struct	radix_mask *rm_mklist;	/* more masks to try */
	union	{
		caddr_t	rmu_mask;		/* the mask */
		struct	radix_node *rmu_leaf;	/* for normal routes */
	}	rm_rmu;
	int	rm_refs;		                       /* # of references to this struct */
};

#define	rm_mask rm_rmu.rmu_mask
#define	rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */

struct radix_head;

typedef int walktree_f_t(struct radix_node *, void *);
typedef struct radix_node *rn_matchaddr_f_t(void *v,
    struct radix_head *head);
typedef struct radix_node *rn_addaddr_f_t(void *v, void *mask,
    struct radix_head *head, struct radix_node nodes[]);
typedef struct radix_node *rn_deladdr_f_t(void *v, void *mask,
    struct radix_head *head);
typedef struct radix_node *rn_lookup_f_t(void *v, void *mask,
    struct radix_head *head);
typedef int rn_walktree_t(struct radix_head *head, walktree_f_t *f,
    void *w);
typedef int rn_walktree_from_t(struct radix_head *head,
    void *a, void *m, walktree_f_t *f, void *w);
typedef void rn_close_t(struct radix_node *rn, struct radix_head *head);

struct radix_mask_head;

struct radix_head {
	struct	radix_node *rnh_treetop;
	struct	radix_mask_head *rnh_masks;	/* Storage for our masks */
};

struct radix_node_head {
        rte_rwlock_t    lock;
	struct radix_head rh;
	rn_matchaddr_f_t	*rnh_matchaddr;	/* longest match for sockaddr */
	rn_addaddr_f_t	*rnh_addaddr;	/* add based on sockaddr*/
	rn_deladdr_f_t	*rnh_deladdr;	/* remove based on sockaddr */
	rn_lookup_f_t	*rnh_lookup;	/* exact match for sockaddr */
	rn_walktree_t	*rnh_walktree;	/* traverse tree */
	rn_walktree_from_t	*rnh_walktree_from; /* traverse tree below a */
	rn_close_t	*rnh_close;	/*do something when the last ref drops*/
	struct	radix_node rnh_nodes[3];	/* empty tree for common case */
};

struct radix_mask_head {
	struct radix_head head;
	struct radix_node mask_nodes[3];
};


#define R_Malloc(p, t, n) (p = (t) al_malloc((unsigned int)(n)))
#define R_Zalloc(p, t, n) (p = (t) al_calloc((unsigned int)(n)))
#define R_Free(p) al_free((char *)p);

int   al_route_head_init(void **, int);


#endif

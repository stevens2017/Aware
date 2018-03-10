#ifndef AL_TREE_H
#define AL_TREE_H

typedef uintptr_t  al_rbtree_key_t;
typedef intptr_t   al_rbtree_key_int_t;

typedef struct al_rbtree_node_s  al_rbtree_node_t;

struct al_rbtree_node_s {
    al_rbtree_key_t       key;
    al_rbtree_node_t     *left;
    al_rbtree_node_t     *right;
    al_rbtree_node_t     *parent;
    uint8_t                 color;
    uint8_t                 data;
};

typedef struct al_rbtree_s  al_rbtree_t;

typedef void (*al_rbtree_insert_pt) (al_rbtree_node_t *root,
    al_rbtree_node_t *node, al_rbtree_node_t *sentinel);

typedef struct al_rbtree_s {
    al_rbtree_node_t     *root;
    al_rbtree_node_t     *sentinel;
    al_rbtree_insert_pt   insert;
}al_rbtree_t;


#define al_rbtree_init(tree, s, i)                                           \
    al_rbtree_sentinel_init(s);                                              \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i


void al_rbtree_insert(al_rbtree_t *tree, al_rbtree_node_t *node);
void al_rbtree_delete(al_rbtree_t *tree, al_rbtree_node_t *node);
void al_rbtree_insert_value(al_rbtree_node_t *root, al_rbtree_node_t *node,
    al_rbtree_node_t *sentinel);
al_rbtree_node_t *al_rbtree_next(al_rbtree_t *tree, al_rbtree_node_t *node);
al_rbtree_node_t* al_rbtree_lookup(al_rbtree_t *t, uintptr_t key);

#define al_rbt_red(node)               ((node)->color = 1)
#define al_rbt_black(node)             ((node)->color = 0)
#define al_rbt_is_red(node)            ((node)->color)
#define al_rbt_is_black(node)          (!al_rbt_is_red(node))
#define al_rbt_copy_color(n1, n2)      (n1->color = n2->color)

/* a sentinel must be black */
#define al_rbtree_sentinel_init(node)  al_rbt_black(node)

static inline al_rbtree_node_t *
al_rbtree_min(al_rbtree_node_t *node, al_rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}

static inline uintptr_t al_make_key(uint32_t fid, uint32_t id){
    uintptr_t k=fid;
    return (k<<32) | id;
}
#endif


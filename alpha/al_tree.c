#include <stdio.h>
#include "al_common.h"
#include "al_tree.h"

static inline void al_rbtree_left_rotate(al_rbtree_node_t **root,
    al_rbtree_node_t *sentinel, al_rbtree_node_t *node);
static inline void al_rbtree_right_rotate(al_rbtree_node_t **root,
    al_rbtree_node_t *sentinel, al_rbtree_node_t *node);


void
al_rbtree_insert(al_rbtree_t *tree, al_rbtree_node_t *node)
{
    al_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        al_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && al_rbt_is_red(node->parent)) {

        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (al_rbt_is_red(temp)) {
                al_rbt_black(node->parent);
                al_rbt_black(temp);
                al_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    al_rbtree_left_rotate(root, sentinel, node);
                }

                al_rbt_black(node->parent);
                al_rbt_red(node->parent->parent);
                al_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
            temp = node->parent->parent->left;

            if (al_rbt_is_red(temp)) {
                al_rbt_black(node->parent);
                al_rbt_black(temp);
                al_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    al_rbtree_right_rotate(root, sentinel, node);
                }

                al_rbt_black(node->parent);
                al_rbt_red(node->parent->parent);
                al_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    al_rbt_black(*root);
}


void
al_rbtree_insert_value(al_rbtree_node_t *temp, al_rbtree_node_t *node,
    al_rbtree_node_t *sentinel)
{
    al_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    al_rbt_red(node);
}

void
al_rbtree_delete(al_rbtree_t *tree, al_rbtree_node_t *node)
{
    al_uint_t           red;
    al_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = al_rbtree_min(node->right, sentinel);

        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    if (subst == *root) {
        *root = temp;
        al_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = al_rbt_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        if (subst->parent == node) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        al_rbt_copy_color(subst, node);

        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && al_rbt_is_black(temp)) {

        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (al_rbt_is_red(w)) {
                al_rbt_black(w);
                al_rbt_red(temp->parent);
                al_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (al_rbt_is_black(w->left) && al_rbt_is_black(w->right)) {
                al_rbt_red(w);
                temp = temp->parent;

            } else {
                if (al_rbt_is_black(w->right)) {
                    al_rbt_black(w->left);
                    al_rbt_red(w);
                    al_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                al_rbt_copy_color(w, temp->parent);
                al_rbt_black(temp->parent);
                al_rbt_black(w->right);
                al_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        } else {
            w = temp->parent->left;

            if (al_rbt_is_red(w)) {
                al_rbt_black(w);
                al_rbt_red(temp->parent);
                al_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (al_rbt_is_black(w->left) && al_rbt_is_black(w->right)) {
                al_rbt_red(w);
                temp = temp->parent;

            } else {
                if (al_rbt_is_black(w->left)) {
                    al_rbt_black(w->right);
                    al_rbt_red(w);
                    al_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                al_rbt_copy_color(w, temp->parent);
                al_rbt_black(temp->parent);
                al_rbt_black(w->left);
                al_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    al_rbt_black(temp);
}


static inline void
al_rbtree_left_rotate(al_rbtree_node_t **root, al_rbtree_node_t *sentinel,
    al_rbtree_node_t *node)
{
    al_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}


static inline void
al_rbtree_right_rotate(al_rbtree_node_t **root, al_rbtree_node_t *sentinel,
    al_rbtree_node_t *node)
{
    al_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


al_rbtree_node_t *
al_rbtree_next(al_rbtree_t *tree, al_rbtree_node_t *node)
{
    al_rbtree_node_t  *root, *sentinel, *parent;

    sentinel = tree->sentinel;

    if (node->right != sentinel) {
        return al_rbtree_min(node->right, sentinel);
    }

    root = tree->root;

    for ( ;; ) {
        parent = node->parent;

        if (node == root) {
            return NULL;
        }

        if (node == parent->left) {
            return parent;
        }

        node = parent;
    }
}

al_rbtree_node_t* al_rbtree_lookup(al_rbtree_t *t, uintptr_t key)
{
    al_rbtree_node_t  *p;
    al_rbtree_node_t *temp=t->root;
    al_rbtree_node_t  *s=t->sentinel;

    for ( p=temp; p != s; ) {

        if( key < p->key ){
           p=p->left; 
	   continue;
	}

        if( key > p->key ){
           p=p->right; 
	   continue;
	}

        return p;    
    }

    return NULL;
}


/* kdtree: a k-d tree of double vectors with caller-chosen ids for nearest-neighbour
 * search (squared Euclidean distance by default); points can be built in bulk or
 * inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance,
 * reset) needs it to itself, but searches only read it and may run at the same time. */
#ifndef TLD_KDTREE_H
#define TLD_KDTREE_H

#include "../core/tld-core.h"

/* A tree node: p is the point (n doubles, owned by the tree), id the caller's id. */
typedef struct kd_node {
        double* p;
        struct kd_node* left;
        struct kd_node* right;
        /* int split; */
        uint32_t id;
        int n;
} kd_node;

struct kdyard;

/* A k-d tree; create with kdtree_alloc, free with kdtree_free. Fields a caller may set:
 * fptr_cmp (distance, default kddist_euclidian) with the matching fptr_plane (default
 * kdplane_check_euclidian), and rebalance_at (insertions between balance checks, default
 * 10000). The rest is internal. */
typedef struct kdtree {
        /* kd_node* root; */
        kd_node* root;
        struct kdyard* yard;
        int(*fptr_cmp)(double*, double*, int, double*);
        int(*fptr_plane)(double*, double*, int, double);
        int rebalance_at;
        int n_step;
        int depth;
        int max_depth;
        int min_depth;
        int vec_len;
} kdtree;

/* Allocate in *tree an empty tree for points of size (> 0) dimensions, using squared
 * Euclidean distance. Storage grows as points are added. Free with kdtree_free. */
tld_external  int kdtree_alloc(kdtree **tree, int size);

/* Free the tree and all its points; NULL is ignored. */
tld_external void kdtree_free(kdtree *t);

/* Remove every point and keep the memory for reuse. */
tld_external  int kdtree_reset(kdtree *t);

/* Add n points x[i] of size doubles (size <= the length given to kdtree_alloc) with
 * ids[i] and rebuild the tree balanced. The points are copied; x and ids are not
 * changed. On failure (no memory) the tree is unchanged. */
tld_external  int kdtree_build(kdtree *t, double **x,uint32_t* ids, int n, int size);

/* Insert one point x of size doubles with id (copied). Every rebalance_at insertions
 * the tree is rebuilt if its depth is more than 10 levels off the balanced depth. On
 * failure (no memory) the tree is unchanged. */
tld_external  int kdtree_insert(kdtree* t, double *x,uint32_t id, int size);

/* Rebuild the tree balanced from the points it holds (allocates nothing). */
tld_external  int kdtree_rebalance(kdtree *t);

/* The id of the stored point nearest to x (a vector of the tree's length) into *id.
 * FAIL (with an error) when the tree is empty. */
tld_external  int kdtree_nn_search(kdtree *t, double *x, uint32_t *id);

/* Brute force: the id of the stored point nearest to x, comparing every point; FAIL
 * (with an error) when the tree is empty. For checking kdtree_nn_search. */
tld_external  int kdtree_bf_search(kdtree *t, double *x, uint32_t *id);

/* Squared Euclidean distance between a and b (vec_len doubles) into *dist. The default
 * fptr_cmp. */
tld_external  int kddist_euclidian(double *a, double *b, int vec_len, double *dist);

/* OK when the squared distance between a and b along dimension cd is below min_dist
 * (the far side of the splitting plane may hold a nearer point), FAIL otherwise. The
 * default fptr_plane. */
tld_external  int kdplane_check_euclidian(double *a, double *b, int cd, double min_dist);

/* Debugging: print the subtree under n (pass t->root, depth 0) to stderr, as split
 * values (kdtree_debug_chk) or as an indented list of ids (kdtree_debug_print). A NULL
 * n prints nothing. */
tld_external void kdtree_debug_chk(kd_node *n, int depth);
tld_external void kdtree_debug_print(kd_node *n, int depth);

#endif

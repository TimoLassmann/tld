/* kdtree: a k-d tree of double vectors with caller-chosen ids for nearest-neighbour
 * search (squared Euclidean distance by default); points can be built in bulk or
 * inserted one at a time. Not locked: changing a tree (alloc, build, insert, rebalance,
 * reset) needs it to itself, but searches only read it and may run at the same time. */
#ifndef TLD_KDTREE_H
#define TLD_KDTREE_H

#include "../core/tld-core.h"

/* A tree node: p is the point (n doubles, owned by the tree), id the caller's id. */
typedef struct tld_kdtree_node {
        double* p;
        struct tld_kdtree_node* left;
        struct tld_kdtree_node* right;
        uint32_t id;
        int n;
} tld_kdtree_node;

struct tld__kdyard;

/* A k-d tree; create with tld_kdtree_alloc, free with tld_kdtree_free. Fields a caller may set:
 * fptr_cmp (distance, default tld_kdtree_dist_euclidean) with the matching fptr_plane (default
 * tld_kdtree_plane_euclidean), and rebalance_at (insertions between balance checks, default
 * 10000). The rest is internal. */
typedef struct tld_kdtree {
        tld_kdtree_node* root;
        struct tld__kdyard* yard;
        int(*fptr_cmp)(double*, double*, int, double*);
        int(*fptr_plane)(double*, double*, int, double);
        int rebalance_at;
        int n_step;
        int depth;
        int max_depth;
        int min_depth;
        int vec_len;
} tld_kdtree;

/* Allocate in *tree an empty tree for points of size (> 0) dimensions, using squared
 * Euclidean distance. Storage grows as points are added. Free with tld_kdtree_free. */
TLD_API  int tld_kdtree_alloc(tld_kdtree **tree, int size);

/* Free the tree and all its points; NULL is ignored. */
TLD_API void tld_kdtree_free(tld_kdtree *t);

/* Remove every point and keep the memory for reuse. */
TLD_API  int tld_kdtree_reset(tld_kdtree *t);

/* Add n points x[i] of size doubles (size <= the length given to tld_kdtree_alloc) with
 * ids[i] and rebuild the tree balanced. The points are copied; x and ids are not
 * changed. On failure (no memory) the tree is unchanged. */
TLD_API  int tld_kdtree_build(tld_kdtree *t, double **x,uint32_t* ids, int n, int size);

/* Insert one point x of size doubles with id (copied). Every rebalance_at insertions
 * the tree is rebuilt if its depth is more than 10 levels off the balanced depth. On
 * failure (no memory) the tree is unchanged. */
TLD_API  int tld_kdtree_insert(tld_kdtree* t, double *x,uint32_t id, int size);

/* Rebuild the tree balanced from the points it holds (allocates nothing). */
TLD_API  int tld_kdtree_rebalance(tld_kdtree *t);

/* The id of the stored point nearest to x (a vector of the tree's length) into *id.
 * FAIL (with an error) when the tree is empty. */
TLD_API  int tld_kdtree_nn_search(tld_kdtree *t, double *x, uint32_t *id);

/* Brute force: the id of the stored point nearest to x, comparing every point; FAIL
 * (with an error) when the tree is empty. For checking tld_kdtree_nn_search. */
TLD_API  int tld_kdtree_bf_search(tld_kdtree *t, double *x, uint32_t *id);

/* Squared Euclidean distance between a and b (vec_len doubles) into *dist. The default
 * fptr_cmp. */
TLD_API  int tld_kdtree_dist_euclidean(double *a, double *b, int vec_len, double *dist);

/* OK when the squared distance between a and b along dimension cd is below min_dist
 * (the far side of the splitting plane may hold a nearer point), FAIL otherwise. The
 * default fptr_plane. */
TLD_API  int tld_kdtree_plane_euclidean(double *a, double *b, int cd, double min_dist);

#endif

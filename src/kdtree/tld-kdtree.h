/* kdtree: a k-d tree of double vectors with caller-chosen ids for nearest-neighbour
 * search by Euclidean distance; points can be built in bulk or inserted one at a time.
 * Not locked: changing a tree (alloc, build, insert, rebalance, reset) needs it to
 * itself, but searches only read it and may run at the same time. */
#ifndef TLD_KDTREE_H
#define TLD_KDTREE_H

#include "../core/tld-core.h"

/* Opaque tree; create with tld_kdtree_alloc, free with tld_kdtree_free. */
typedef struct tld_kdtree tld_kdtree;

/* Allocate in *tree an empty tree for points of dim (> 0) doubles. Storage grows as
 * points are added. Free with tld_kdtree_free. */
TLD_API int tld_kdtree_alloc(tld_kdtree **tree, int dim);

/* Free the tree and all its points; NULL is ignored. */
TLD_API void tld_kdtree_free(tld_kdtree *t);

/* Remove every point and keep the memory for reuse. */
TLD_API int tld_kdtree_reset(tld_kdtree *t);

/* Add n points x[i] (each of the tree's dim doubles) with ids[i] and rebuild the tree
 * balanced. The points are copied; x and ids are not changed. On failure (no memory)
 * the tree is unchanged. */
TLD_API int tld_kdtree_build(tld_kdtree *t, double **x, uint32_t *ids, int n);

/* Insert one point x (dim doubles) with id (copied). Every 10000 insertions the tree is
 * rebuilt if its depth is more than 10 levels off the balanced depth. On failure (no
 * memory) the tree is unchanged. */
TLD_API int tld_kdtree_insert(tld_kdtree *t, double *x, uint32_t id);

/* Rebuild the tree balanced from the points it holds (allocates nothing). */
TLD_API int tld_kdtree_rebalance(tld_kdtree *t);

/* The id of the stored point nearest to x (dim doubles) into *id. FAIL (with an error)
 * when the tree is empty. */
TLD_API int tld_kdtree_nn_search(tld_kdtree *t, double *x, uint32_t *id);

/* Brute force: the id of the stored point nearest to x, comparing every point; FAIL
 * (with an error) when the tree is empty. For checking tld_kdtree_nn_search. */
TLD_API int tld_kdtree_bf_search(tld_kdtree *t, double *x, uint32_t *id);

#endif

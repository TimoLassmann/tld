#ifndef TLD_KDTREE_H
#define TLD_KDTREE_H

#include "../core/tld-core.h"

typedef struct kd_node {
        double* p;
        struct kd_node* left;
        struct kd_node* right;
        /* int split; */
        uint32_t id;
        int n;
} kd_node;

struct kdyard;

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

tld_external  int kdtree_bf_search(kdtree *t, double *x, uint32_t *id);
tld_external  int kdtree_nn_search(kdtree *t, double *x, uint32_t *id);

tld_external  int kdtree_build(kdtree *t, double **x,uint32_t* ids, int n, int size);
/* tld_external int kdtree_insert(kdtree *t, double *x, int size); */
tld_external  int kdtree_insert(kdtree* t, double *x,uint32_t id, int size);
tld_external  int kdtree_rebalance(kdtree *t);

tld_external  int kdtree_alloc(kdtree **tree, int size);
tld_external  int kdtree_reset(kdtree *t);
tld_external void kdtree_free(kdtree *t);

tld_external void kdtree_debug_chk(kd_node *n, int depth);
tld_external void kdtree_debug_print(kd_node *n, int depth);


tld_external  int kddist_euclidian(double *a, double *b, int vec_len, double *dist);
tld_external  int kdplane_check_euclidian(double *a, double *b, int cd, double min_dist);
#endif

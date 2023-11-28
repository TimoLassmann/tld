#ifndef TLD_KDTREE_H
#define TLD_KDTREE_H

#include "../core/tld-core.h"

typedef struct kd_node {
        float* p;
        struct kd_node* left;
        struct kd_node* right;
        int split;
        int n;
} kd_node;

struct kdyard;

typedef struct kdtree {
        /* kd_node* root; */
        kd_node* root;
        struct kdyard* yard;
} kdtree;

tld_external  int kdtree_alloc(kdtree **tree, int size);
tld_external void kdtree_free(kdtree *t);

tld_external int kdtree_build(kdtree* t,kd_node** root,float **x, int n, int size, int depth);

tld_external int kdtree_insert(kdtree* t, float *x, int size);

tld_external void quickselect(float **x, int left, int right, int k, int dim);

tld_external void kdtree_debug_print(kd_node *n, int depth);
tld_external void kdtree_node_free(kd_node *n);
#endif

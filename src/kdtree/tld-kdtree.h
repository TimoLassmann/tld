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

typedef struct kdtree {
        /* kd_node* root; */
        kd_node* root;



} kdtree;

tld_external int kdtree_build(float **x, int n, int size, int depth, kd_node **root);
tld_external int kdtree_insert(kd_node *root, float *x, int size,kd_node **ret);

tld_external void quickselect(float **x, int left, int right, int k, int dim);

tld_external void kdtree_debug_print(kd_node *n, int depth);
tld_external void kdtree_node_free(kd_node *n);
#endif

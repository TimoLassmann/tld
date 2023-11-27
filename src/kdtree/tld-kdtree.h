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

tld_external int kdtree_insert(kd_node *root, float *x, int size,kd_node **ret);

tld_external void quickselect(float **x, int left, int right, int k, int dim);
#endif

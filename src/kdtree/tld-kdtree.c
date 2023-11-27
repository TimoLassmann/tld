#include "tld-kdtree.h"

#include "../alloc/tld-alloc.h"
#include <stdlib.h>

int compare_x_cd(const void *a, const void *b, void *arg);

kd_node *insert(kd_node *root, float *x, int size, int depth);
int kdtree_node_create(float *x, int size, kd_node **ret);
int kdtree_node_alloc(kd_node **node, int size);
void kdtree_node_free(kd_node *n);


void quickselect(float **x, int left, int right, int k, int dim);
 int partition(float **x, int left, int right, int pivotIndex, int dim);
void swap_vec(float **x, int a, int b);

/* recursive  */

int kdtree_build(float **x, int n, int size, int depth, kd_node **root)
{
        ASSERT(n < 0,"No points!!!");

        int cd = depth % size;

        /* qsort_r(x,n,sizeof(float*), compare_x_cd, &cd); */

        return OK;
ERROR:
        return FAIL;
}

int compare_x_cd(const void *a, const void *b, void *arg)
{
        int d = *(int*) arg;
        float* one = *(float**) a;
        float* two = *(float**) b;
        return (one[d] < two[d]) ? -1 : one[d] > two[d];
}


int kdtree_insert(kd_node *root, float *x, int size,kd_node **ret)
{

        *ret = insert(root, x, size, 0);

        return OK;
ERROR:
        return FAIL;
}

kd_node *insert(kd_node *root, float *x, int size, int depth)
{
        if(root == NULL){
                RUN(kdtree_node_create(x, size, &root));
                return root;
        }

        int cd = depth % size;
        if(x[cd] < root->p[cd]){
                root->left = insert(root->left, x, size, depth+1);
        }else{
                root->right = insert(root->right, x, size, depth+1);
        }

        return root;
ERROR:
        return NULL;
}


int kdtree_node_create(float *x, int size, kd_node **ret)
{
        kd_node* n = NULL;

        ASSERT(size > 0,"Size less than one!");

        kdtree_node_alloc(&n, size);
        for(int i = 0; i < size;i++){
                n->p[i] = x[i];
        }

        *ret = n;

        return OK;
ERROR:
        kdtree_node_free(n);
        return FAIL;
}

int kdtree_node_alloc(kd_node **node, int size)
{
        kd_node* n = NULL;
        MMALLOC(n, sizeof(kd_node));
        n->p = NULL;
        n->left = NULL;
        n->right = NULL;
        n->split = -1;
        n->n = size;

        galloc(&n->p,n->n);
        *node = n;

        return OK;
ERROR:
        kdtree_node_free(n);
        return FAIL;
}

void kdtree_node_free(kd_node *n)
{
        if(n){
                if(n->p){
                        gfree(n->p);
                }
                MFREE(n);
        }

}


/* Sorting code  */

void quickselect(float **x, int left, int right, int k, int dim)

{
        if(left == right){
                return;
        }

        int pivotIndex = (left + right) / 2;
        pivotIndex = partition(x, left, right, pivotIndex, dim);
        if(k == pivotIndex){
                return;
        }else if( k < pivotIndex){
                quickselect(x, left, pivotIndex-1, k, dim);
        }else{
                quickselect(x, pivotIndex+1,right , k, dim);
        }
}

int partition(float **x, int left, int right, int pivotIndex, int dim)
{
        float* pivot_val = x[pivotIndex];

        swap_vec(x, pivotIndex, right);

        int storeIdx = left;

        for(int i = left; i < right;i++){
                if(x[i][dim] < pivot_val[dim]){
                        swap_vec(x, storeIdx,i);
                        storeIdx++;
                }
        }

        swap_vec(x, right, storeIdx);
        return storeIdx;
}


void swap_vec(float **x, int a, int b)
{
        float *tmp = x[a];
        x[a] = x[b];
        x[b] = tmp;

}

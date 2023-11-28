#include "tld-kdtree.h"

#include "../alloc/tld-alloc.h"
#include <stdlib.h>
#include <stdio.h>

struct sort_node {
        float* vec;
        float val;
};

struct kdyard {
        kd_node** nodes;
        struct sort_node** s_nodes;
        int n_alloc;
        int n_node;
        int n_snode;
        int vec_len;
};

int kdyard_node_get(struct kdyard *y, kd_node **ret);
int kdyard_snode_get(struct kdyard *y, kd_node **ret);

int kdyard_alloc(struct kdyard ** yard, int size, int len);
int kdyard_realloc(struct kdyard *n);
void kdyard_free(struct kdyard *n);

int mat_sort(float **x, int dim1,int cd);
int sort_nodes(const void *a, const void *b);

int insert(kdtree* t,kd_node *root, float *x, int size, int depth);
/* kd_node *insert(kd_node *root, float *x, int size, int depth); */
/* int kdtree_node_create(float *x, int size, kd_node **ret); */
int kdtree_node_create(kdtree* t, float *x, int size, kd_node **ret);
int kdtree_node_alloc(kd_node **node, int size);
/* void kdtree_node_free(kd_node *n); */



/* recursive  */

int kdtree_build(kdtree* t,kd_node** root, float **x, int n, int size, int depth)
{
        kd_node* node = NULL;
        kd_node* nl = NULL;
        kd_node* nr = NULL;
        if( n <= 0){
                return OK;
        }
        /* ASSERT(n > 0,"No points!!!"); */

        int cd = depth % size;
        RUN(mat_sort(x, n, cd));

        /* kdyard_node_get(struct kdyard *y, kd_node **ret) */
        RUN(kdtree_node_create(t,x[n/2], size, &node));


        RUN(kdtree_build(t,&nl,x, n/2, size,  depth+1));

        RUN(kdtree_build(t,&nr,x+n/2+1, n - n / 2 -1, size,  depth+1));


        node->left = nl;
        node->right = nr;

        *root = node;

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

int kdtree_insert(kdtree* t, float *x, int size)
{

        RUN(insert(t,t->root, x, size, 0));

        return OK;
ERROR:
        return FAIL;
}

int insert(kdtree* t,kd_node *root, float *x, int size, int depth)
{
        if(root == NULL){
                RUN(kdtree_node_create(t,x, size, &root));
                return OK;
        }

        int cd = depth % size;
        if(x[cd] < root->p[cd]){
                RUN(insert(t,root->left, x, size, depth+1));
        }else{
                RUN(insert(t,root->right, x, size, depth+1));
        }
        return OK;
ERROR:
        return FAIL;
}


int kdtree_node_create(kdtree* t, float *x, int size, kd_node **ret)
{
        kd_node* n = NULL;

        ASSERT(size > 0,"Size less than one!");

        kdyard_node_get(t->yard, &n);
        /* kdtree_node_alloc(&n, size); */
        for(int i = 0; i < size;i++){
                n->p[i] = x[i];
        }

        *ret = n;

        return OK;
ERROR:
        kdtree_node_free(n);
        return FAIL;
}

void kdtree_debug_print(kd_node *n, int depth)
{
        if(n->left){
                kdtree_debug_print(n->left, depth+1);
        }

        for(int i = 0; i < depth*3;i++){
                fprintf(stdout," ");
        }
        int cd = depth % n->n;
        fprintf(stdout,"%d",cd);


        fprintf(stdout,"\n");

        if(n->right){
                kdtree_debug_print(n->right, depth+1);
        }

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
                /* if(n->left){ */
                /*         kdtree_node_free(n->left); */
                /* } */
                /* if(n->right){ */
                /*         kdtree_node_free(n->right); */
                /* } */
                if(n->p){
                        gfree(n->p);
                }
                MFREE(n);
        }
}



int mat_sort(float **x, int dim1,int cd)
{
        struct sort_node** node_list = NULL;

        MMALLOC(node_list, sizeof(struct sort_node*) * dim1);

        for(int i = 0; i < dim1;i++){
                node_list[i] = NULL;
                MMALLOC(node_list[i], sizeof(struct sort_node));
                node_list[i]->val = x[i][cd];
                node_list[i]->vec = x[i];
        }

        qsort(node_list, dim1, sizeof(struct sort_node*), sort_nodes);

        for(int i = 0; i < dim1;i++){
                x[i] = node_list[i]->vec;
                MFREE(node_list[i]);
        }


        MFREE(node_list);

        return OK;
ERROR:
        return FAIL;
}

int sort_nodes(const void *a, const void *b)
{
        struct sort_node * const *one = a;
        struct sort_node * const *two = b;
        if((*one)->val >  (*two)->val){
                return 1;
        }else{
                return -1;
        }
}

int kdtree_alloc(kdtree **tree, int size)
{
        kdtree* t = NULL;
        MMALLOC(t, sizeof(kdtree));
        t->yard = NULL;
        t->root = NULL;

        RUN(kdyard_alloc(&t->yard, 1024,size));

        *tree = t;
        return OK;
ERROR:
        return FAIL;
}

void kdtree_free(kdtree *t)
{
        if(t){
                if(t->yard){
                        kdyard_free(t->yard);
                }
                MFREE(t);
        }
}

int kdyard_node_get(struct kdyard *y, kd_node **ret)
{
        kd_node*n = NULL;
        n = y->nodes[y->n_node];
        y->n_node++;
        if(y->n_node == y->n_alloc){
                kdyard_realloc(y);
        }
        *ret = n;
        return OK;
}

int kdyard_snode_get(struct kdyard *y, kd_node **ret)
{
        kd_node*n = NULL;
        n = y->nodes[y->n_snode];
        y->n_snode++;
        if(y->n_snode == y->n_alloc){
                kdyard_realloc(y);
        }
        *ret = n;
        return OK;
}

int kdyard_alloc(struct kdyard ** yard, int size, int len)
{
        struct kdyard* n = NULL;
        MMALLOC(n, sizeof(struct kdyard));
        n->n_node = 0;
        n->n_snode = 0;
        n->n_alloc = size;
        n->vec_len = len;
        n->nodes = NULL;
        n->s_nodes = NULL;

        MMALLOC(n->nodes, sizeof( kd_node*) * n->n_alloc);
        MMALLOC(n->s_nodes, sizeof(struct sort_node*) * n->n_alloc);

        for(int i = 0;i < n->n_alloc;i++){
                n->nodes[i] = NULL;
                n->s_nodes[i] = NULL;

                MMALLOC(n->s_nodes[i], sizeof(struct sort_node));
                RUN(kdtree_node_alloc(&n->nodes[i],n->vec_len));
        }
        *yard = n;
        return OK;
ERROR:
        kdyard_free(n);
        return FAIL;
}

int kdyard_realloc(struct kdyard *n)
{
        int old_size = n->n_alloc;

        n->n_alloc = n->n_alloc + n->n_alloc / 2;

        MREALLOC(n->nodes, sizeof( kd_node*) * n->n_alloc);
        MREALLOC(n->s_nodes, sizeof(struct sort_node*) * n->n_alloc);

        for(int i = old_size;i < n->n_alloc;i++){
                n->nodes[i] = NULL;
                n->s_nodes[i] = NULL;

                MMALLOC(n->s_nodes[i], sizeof(struct sort_node));
                RUN(kdtree_node_alloc(&n->nodes[i],n->vec_len));
        }
        return OK;
ERROR:
        kdyard_free(n);
        return FAIL;
}

void kdyard_free(struct kdyard *n)
{
        if(n){
                for(int i = 0; i < n->n_alloc;i++){
                        MFREE(n->s_nodes[i]);
                        kdtree_node_free(n->nodes[i]);
                }
                MFREE(n->s_nodes);
                MFREE(n->nodes);
                MFREE(n);
        }
}

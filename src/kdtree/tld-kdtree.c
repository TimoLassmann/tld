#include "tld-kdtree.h"

#include "../alloc/tld-alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

struct sort_node {
        double* vec;
        double val;
        uint32_t id;
};

struct kdyard {
        kd_node** nodes;
        struct sort_node** s_nodes;
        double** tmp_mat;
        uint32_t* ids;
        int n_mat;
        int n_alloc;
        int n_node;
        int n_snode;
        int vec_len;
};

int nn_search(kdtree *t,kd_node* n, double *x,double* min_d, uint32_t *id);

int kdyard_node_get(struct kdyard *y, kd_node **ret);
int kdyard_snode_get(struct kdyard *y, kd_node **ret);

int kdyard_alloc(struct kdyard ** yard, int size, int len);
int kdyard_realloc(struct kdyard *n);
void kdyard_free(struct kdyard *n);


int mat_sort(double **x,uint32_t* ids, int dim1,int cd);
int sort_nodes(const void *a, const void *b);

/* kd_node * insert(kdtree* t,kd_node *root, double *x, int size); */
kd_node * insert(kdtree* t,kd_node *n, double *x,uint32_t id, int size);
int  kdtree_node_create(kdtree* t, double *x, uint32_t id,int size, kd_node **ret);
/* int  kdtree_node_create(kdtree* t, double *x, int size, kd_node **ret); */
int  kdtree_node_alloc(kd_node **node, int size);
int  kdtree_node_reset(kd_node *n);
void kdtree_node_free(kd_node *n);

int rebuild(kdtree* t,kd_node** root, double **x,uint32_t* ids, int n, int size);
/* int rebuild(kdtree* t,kd_node** root, double **x, int n, int size); */

int kdtree_bf_search(kdtree *t, double *x, uint32_t *id)
{
        double min_d = DBL_MAX;
        double d = 0.0;
        uint32_t l_id = 0;
        int n = t->yard->n_node;

        for(int i = 0; i < n;i++){
                t->fptr_cmp(x, t->yard->nodes[i]->p, t->vec_len,&d);
                if(d < min_d){
                        min_d = d;
                        l_id = t->yard->nodes[i]->id;
                }
        }

        *id = l_id;
        return OK;
}

int kdtree_nn_search(kdtree *t, double *x, uint32_t *id)
{
        double min_d = DBL_MAX;
        t->depth = 0;
        RUN(nn_search(t,t->root, x, &min_d,id));
        return OK;
ERROR:
        return FAIL;
}

int nn_search(kdtree *t,kd_node* n, double *x,double* min_d, uint32_t *id)
{
        int depth = 0;
        double d = 0.0;

        if(n == NULL){
                return OK;
        }

        depth = t->depth;
        /* fprintf(stdout,"Search: depth %d at node: %d (min: %f)\n", depth,n->id, *min_d); */
        d = 0.0;
        t->fptr_cmp(n->p,x,t->vec_len, &d);

        if(d < *min_d){
                *min_d = d;
                *id = n->id;
        }
        int cd = depth % t->vec_len;

        if(x[cd] < n->p[cd]){
                t->depth = depth + 1;
                nn_search(t,n->left,x,min_d,id);
        }else{
                t->depth = depth + 1;
                nn_search(t,n->right,x,min_d,id);
        }

        if(t->fptr_plane(n->p,x,cd, *min_d) == OK){
                /* Note this has to be the REVERSE from above
                   Because I need to search the other branch!
                */
                /* fprintf(stdout,"Search: at node: %d (plane search)\n", n->id); */
                if(x[cd] < n->p[cd]){
                        t->depth = depth + 1;
                        nn_search(t,n->right,x,min_d,id);
                }else{
                        t->depth = depth + 1;
                        nn_search(t,n->left,x,min_d,id);
                }
        }
        return OK;
}


int kdtree_rebalance(kdtree *t)
{
        int n_nodes = 0;

        n_nodes = t->yard->n_node;
        for(int i = 0; i < n_nodes;i++){
                t->yard->tmp_mat[i] = t->yard->nodes[i]->p;
                t->yard->ids[i] = t->yard->nodes[i]->id;
                t->yard->nodes[i]->p = NULL;

        }

        kdtree_reset(t);

        RUN(kdtree_build(t, t->yard->tmp_mat, t->yard->ids,  n_nodes, t->vec_len));

        return OK;
ERROR:
        return FAIL;
}


int kdtree_build(kdtree *t, double **x,uint32_t* ids, int n, int size)
{
        t->depth = 0;
        RUN(rebuild(t, &t->root, x,ids, n, size));
        if(size > t->vec_len ){
                t->vec_len = size;
        }
        return OK;
ERROR:
        return FAIL;
}


/* recursive  */
int rebuild(kdtree* t,kd_node** root, double **x,uint32_t* ids, int n, int size)
{
        kd_node* node = NULL;
        kd_node* nl = NULL;
        kd_node* nr = NULL;
        int d = t->depth;


        if( n <= 0){
                return OK;
        }
        /* ASSERT(n > 0,"No points!!!"); */

        int cd = d % size;
        RUN(mat_sort(x,ids,n, cd));

        /* kdyard_node_get(struct kdyard *y, kd_node **ret) */
        RUN(kdtree_node_create(t,x[n/2], ids[n/2], size, &node));
        /* LOG_MSG("%d ", d); */
        t->depth = d+1;
        RUN(rebuild(t,&nl,x,ids, n/2, size));
        t->depth = d+1;
        RUN(rebuild(t,&nr,x+n/2+1,ids+n/2+1, n - n / 2 -1, size));


        node->left = nl;
        node->right = nr;

        if(d > t->max_depth){
                t->max_depth = d;
        }

        if(d < t->min_depth){
                t->min_depth = d;
        }

        *root = node;

        return OK;
ERROR:
        return FAIL;
}

int kdtree_reset(kdtree *t)
{
        RUN(kdtree_node_reset(t->root));
        t->root = NULL;
        t->yard->n_node = 0;
        t->yard->n_snode = 0;
        t->depth = 0;
        t->max_depth = 0;
        t->min_depth = 0;
        return OK;
ERROR:
        return FAIL;
}

int kdtree_node_reset(kd_node *n)
{
        if(n){
                if(n->left){
                        RUN(kdtree_node_reset(n->left));
                }
                if(n->right){
                        RUN(kdtree_node_reset(n->right));
                }
                n->left = NULL;
                n->right = NULL;
        }
        return OK;
ERROR:
        return FAIL;
}


int compare_x_cd(const void *a, const void *b, void *arg)
{
        int d = *(int*) arg;
        double* one = *(double**) a;
        double* two = *(double**) b;
        return (one[d] < two[d]) ? -1 : one[d] > two[d];
}

int kdtree_insert(kdtree* t, double *x,uint32_t id, int size)
{

        t->depth = 0;
        t->root = insert(t,t->root, x,id, size);

        if(size > t->vec_len){
                t->vec_len = size;
        }

        t->n_step = t->n_step + 1;
        /* fprintf(stdout,"Step: %d\n", t->n_step); */

        if(t->n_step == t->rebalance_at){
                int test =  t->yard->n_node;
                int opt_d = 0;
                while(test){
                        test = test / 2;
                        opt_d++;
                }

                fprintf(stdout,"opt: %d real %d\n", opt_d, t->max_depth);
                if(abs(t->max_depth  - opt_d) > 10){
                        fprintf(stdout,"re-balancing tree!! (size: %d depth : %d)\n", t->yard->n_node,t->max_depth);
                        kdtree_rebalance(t);
                        fprintf(stdout,"opt: %d real %d (afterwards)\n", opt_d, t->max_depth);
                }
                t->n_step = 0;
        }

        return OK;
ERROR:
        return FAIL;
}

kd_node * insert(kdtree* t,kd_node *n, double *x,uint32_t id, int size)
{

        if(n == NULL){
                if(t->depth > t->max_depth){
                        t->max_depth = t->depth;
                }
                if(t->depth < t->min_depth){
                        t->min_depth = t->depth;
                }

                /* kdyard_node_get(t->yard, &n); */

                kdtree_node_create(t,x,id, size, &n);

                return n;
        }
        int depth = t->depth;
        int cd = t->depth % size;
        /* fprintf(stdout,"%d -> %f , %f\n", t->depth, x[cd], n->p[cd]); */

        if(x[cd] < n->p[cd]){
                t->depth = depth+1;
                n->left = insert(t,n->left, x, id,size);

        }else{
                t->depth = depth+1;
                n->right = insert(t,n->right, x,id, size);
        }

        return n;
}


int kdtree_node_create(kdtree* t, double *x, uint32_t id,int size, kd_node **ret)
{
        kd_node* n = NULL;

        ASSERT(size > 0,"Size less than one!");

        kdyard_node_get(t->yard, &n);
        n->left = NULL;
        n->right = NULL;
        n->id = id;
        if(n->p == NULL){
                n->p = x;
        }else{
                for(int i = 0; i < size;i++){
                        n->p[i] = x[i];
                }
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
        fprintf(stdout,"%d -> id: %d",cd,n->id);


        fprintf(stdout,"\n");

        if(n->right){
                kdtree_debug_print(n->right, depth+1);
        }

}

void kdtree_debug_chk(kd_node *n, int depth)
{
        int cd = depth % n->n;
        if(n->left && n->right){
                fprintf(stdout,"L:%f\tM:%f\tR:%f\n",n->left->p[cd], n->p[cd],n->right->p[cd]  );
        }
        if(n->left){
                kdtree_debug_chk(n->left, depth+1);
        }
        if(n->right){
                kdtree_debug_chk(n->right, depth+1);
        }
}


int kdtree_node_alloc(kd_node **node, int size)
{
        kd_node* n = NULL;
        MMALLOC(n, sizeof(kd_node));
        n->p = NULL;
        n->left = NULL;
        n->right = NULL;
        /* n->split = -1; */
        n->n = size;
        n->id = 0;
        galloc(&n->p,n->n);
        for(int i = 0; i < n->n;i++){
                n->p[i] = 0.0f;
        }
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



int mat_sort(double **x,uint32_t* ids, int dim1,int cd)
{
        struct sort_node** node_list = NULL;

        MMALLOC(node_list, sizeof(struct sort_node*) * dim1);

        for(int i = 0; i < dim1;i++){
                node_list[i] = NULL;
                MMALLOC(node_list[i], sizeof(struct sort_node));
                node_list[i]->val = x[i][cd];
                node_list[i]->vec = x[i];
                node_list[i]->id = ids[i];

        }

        qsort(node_list, dim1, sizeof(struct sort_node*), sort_nodes);

        for(int i = 0; i < dim1;i++){
                x[i] = node_list[i]->vec;
                ids[i] = node_list[i]->id;
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
        /* default...  */
        t->fptr_cmp = kddist_euclidian;
        t->fptr_plane = kdplane_check_euclidian;
        t->depth = 0;
        t->max_depth = 0;
        t->min_depth = 0;
        t->vec_len = 0;
        t->rebalance_at = 10000;
        t->n_step = 0;
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
        n->n_mat = 0;
        n->nodes = NULL;
        n->s_nodes = NULL;
        n->tmp_mat = NULL;
        n->ids = NULL;
        MMALLOC(n->nodes, sizeof( kd_node*) * n->n_alloc);
        MMALLOC(n->s_nodes, sizeof(struct sort_node*) * n->n_alloc);
        MMALLOC(n->tmp_mat, sizeof(double*) * n->n_alloc);
        MMALLOC(n->ids, sizeof(uint32_t) * n->n_alloc);

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
        MREALLOC(n->tmp_mat, sizeof(double*) * n->n_alloc);
        MREALLOC(n->ids, sizeof(uint32_t) * n->n_alloc);
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
                MFREE(n->tmp_mat);
                MFREE(n->ids);
                MFREE(n->s_nodes);
                MFREE(n->nodes);
                MFREE(n);
        }
}


int kddist_euclidian(double *a, double *b, int vec_len, double *dist)
{
        double d = 0.0;
        double x;
        ASSERT(a != NULL,"No a");
        ASSERT(b != NULL,"No b");
        for(int i = 0; i < vec_len;i++){
                x = a[i] - b[i];
                d += x*x;
        }
        /* d = sqrt(d); */
        *dist = d;
        return OK;
ERROR:
        return FAIL;
}

int kdplane_check_euclidian(double *a, double *b, int cd, double min_dist)
{
        double x;

        x = a[cd]-b[cd];
        x = x * x;
        /* x = sqrt(x); */
        /* fprintf(stdout,"PLANE chk: %f %f\n", x, min_dist); */
        if(x < min_dist){
                return OK;
        }else{
                return FAIL;
        }
}

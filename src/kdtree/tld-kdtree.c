#include "tld-kdtree.h"

#include "../alloc/tld-alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* The yard owns every node and its point. nodes[0 .. n_node) are in the tree; the
   slots up to n_alloc are allocated and free for later points. Building and
   rebalancing only re-link these nodes, so they never allocate and never move a
   point between nodes. */
struct kdyard {
        kd_node** nodes;
        int n_alloc;
        int n_node;
        int vec_len;
};

static int nn_search(const kdtree *t, const kd_node* n, const double *x, int depth, double* min_d, uint32_t *id);

static int kdyard_alloc(struct kdyard ** yard, int size, int len);
static int kdyard_reserve(struct kdyard *y, int n);
static void kdyard_free(struct kdyard *n);

static void kdtree_node_set(kdtree *t, kd_node *n, const double *x, uint32_t id, int size);
static void kdtree_relink(kdtree *t);
static kd_node* link_balanced(kd_node **v, int n, int depth, int dim, int *max_depth);
static void select_nth(kd_node **v, int n, int k, int cd);
static int kdtree_node_alloc(kd_node **node, int size);
static void kdtree_node_free(kd_node *n);

int kdtree_node_alloc(kd_node **node, int size)
{
        kd_node* n = NULL;
        MMALLOC(n, sizeof(kd_node));
        n->n = size;
        galloc(&n->p, n->n);
        *node = n;
        return OK;
ERROR:
        kdtree_node_free(n);
        return FAIL;
}

void kdtree_node_free(kd_node *n)
{
        if(n){
                gfree(n->p);
                MFREE(n);
        }
}

int kdyard_alloc(struct kdyard ** yard, int size, int len)
{
        struct kdyard* n = NULL;
        MMALLOC(n, sizeof(struct kdyard));
        n->vec_len = len;
        RUN(kdyard_reserve(n, size));
        *yard = n;
        return OK;
ERROR:
        kdyard_free(n);
        return FAIL;
}

/* Room for n more nodes after the ones in the tree, growing by at least half. On
   failure the yard is unchanged apart from extra free slots, so the tree is intact. */
int kdyard_reserve(struct kdyard *y, int n)
{
        int need = 0;
        int new_size = 0;
        ASSERT(n >= 0 && y->n_node <= INT32_MAX - n, "Too many points (%d more after %d)", n, y->n_node);
        need = y->n_node + n;
        if(need <= y->n_alloc){
                return OK;
        }
        new_size = y->n_alloc + y->n_alloc / 2 + 1;
        if(new_size < need || new_size < y->n_alloc){
                new_size = need;
        }
        MREALLOC(y->nodes, sizeof(kd_node*) * (size_t) new_size);
        while(y->n_alloc < new_size){
                y->nodes[y->n_alloc] = NULL;
                RUN(kdtree_node_alloc(&y->nodes[y->n_alloc], y->vec_len));
                y->n_alloc++;
        }
        return OK;
ERROR:
        return FAIL;
}

void kdyard_free(struct kdyard *n)
{
        if(n){
                for(int i = 0; i < n->n_alloc; i++){
                        kdtree_node_free(n->nodes[i]);
                }
                MFREE(n->nodes);
                MFREE(n);
        }
}

int kdtree_alloc(kdtree **tree, int size)
{
        kdtree* t = NULL;
        ASSERT(size > 0, "Vector length must be positive (got %d)", size);
        MMALLOC(t, sizeof(kdtree));
        t->fptr_cmp = kddist_euclidian;
        t->fptr_plane = kdplane_check_euclidian;
        t->rebalance_at = 10000;
        RUN(kdyard_alloc(&t->yard, 1024, size));
        *tree = t;
        return OK;
ERROR:
        kdtree_free(t);
        return FAIL;
}

void kdtree_free(kdtree *t)
{
        if(t){
                kdyard_free(t->yard);
                MFREE(t);
        }
}

int kdtree_reset(kdtree *t)
{
        ASSERT(t != NULL, "No tree");
        t->root = NULL;
        t->yard->n_node = 0;
        t->depth = 0;
        t->max_depth = 0;
        t->min_depth = 0;
        return OK;
ERROR:
        return FAIL;
}

/* Put point x (size doubles, zero-padded to the yard's length) and id into the free
   node n. */
void kdtree_node_set(kdtree *t, kd_node *n, const double *x, uint32_t id, int size)
{
        n->left = NULL;
        n->right = NULL;
        n->id = id;
        for(int i = 0; i < size; i++){
                n->p[i] = x[i];
        }
        for(int i = size; i < t->yard->vec_len; i++){
                n->p[i] = 0.0;
        }
}

/* Reorder v[0..n) so that v[k] holds the node whose coordinate cd would be k-th in
   sorted order, with no larger value before it and no smaller one after it. */
void select_nth(kd_node **v, int n, int k, int cd)
{
        int lo = 0;
        int hi = n - 1;
        while(lo < hi){
                const double pivot = v[lo + (hi - lo) / 2]->p[cd];
                int i = lo;
                int j = hi;
                while(i <= j){
                        while(v[i]->p[cd] < pivot){
                                i++;
                        }
                        while(v[j]->p[cd] > pivot){
                                j--;
                        }
                        if(i <= j){
                                kd_node* tmp = v[i];
                                v[i] = v[j];
                                v[j] = tmp;
                                i++;
                                j--;
                        }
                }
                if(k <= j){
                        hi = j;
                }else if(k >= i){
                        lo = i;
                }else{
                        break;
                }
        }
}

/* Link v[0..n) into a balanced subtree splitting on coordinate depth % dim; returns
   its root. */
kd_node* link_balanced(kd_node **v, int n, int depth, int dim, int *max_depth)
{
        kd_node* r = NULL;
        int mid = n / 2;
        if(n <= 0){
                return NULL;
        }
        select_nth(v, n, mid, depth % dim);
        r = v[mid];
        if(depth > *max_depth){
                *max_depth = depth;
        }
        r->left = link_balanced(v, mid, depth + 1, dim, max_depth);
        r->right = link_balanced(v + mid + 1, n - mid - 1, depth + 1, dim, max_depth);
        return r;
}

/* Rebuild the tree balanced from the nodes it holds. */
void kdtree_relink(kdtree *t)
{
        t->max_depth = 0;
        t->min_depth = 0;
        t->root = link_balanced(t->yard->nodes, t->yard->n_node, 0, t->vec_len, &t->max_depth);
}

int kdtree_build(kdtree *t, double **x,uint32_t* ids, int n, int size)
{
        ASSERT(t != NULL, "No tree");
        ASSERT(n >= 0, "Negative number of points (%d)", n);
        ASSERT(n == 0 || (x != NULL && ids != NULL), "No points or ids");
        ASSERT(size > 0 && size <= t->yard->vec_len, "Point length %d is not in 1..%d", size, t->yard->vec_len);
        RUN(kdyard_reserve(t->yard, n));
        for(int i = 0; i < n; i++){
                kdtree_node_set(t, t->yard->nodes[t->yard->n_node + i], x[i], ids[i], size);
        }
        t->yard->n_node += n;
        if(size > t->vec_len){
                t->vec_len = size;
        }
        kdtree_relink(t);
        return OK;
ERROR:
        return FAIL;
}

int kdtree_insert(kdtree* t, double *x,uint32_t id, int size)
{
        kd_node* n = NULL;
        kd_node** slot = NULL;
        int depth = 0;
        ASSERT(t != NULL, "No tree");
        ASSERT(x != NULL, "No point");
        ASSERT(size > 0 && size <= t->yard->vec_len, "Point length %d is not in 1..%d", size, t->yard->vec_len);

        RUN(kdyard_reserve(t->yard, 1));
        n = t->yard->nodes[t->yard->n_node];
        kdtree_node_set(t, n, x, id, size);
        t->yard->n_node++;

        if(size > t->vec_len){
                /* the split coordinates depend on the length: re-link all */
                t->vec_len = size;
                kdtree_relink(t);
                t->n_step = 0;
                return OK;
        }

        slot = &t->root;
        while(*slot != NULL){
                const int cd = depth % t->vec_len;
                /* n->p, not x: past size, x holds nothing the tree stores */
                slot = n->p[cd] < (*slot)->p[cd] ? &(*slot)->left : &(*slot)->right;
                depth++;
        }
        *slot = n;
        if(depth > t->max_depth){
                t->max_depth = depth;
        }

        t->n_step = t->n_step + 1;
        if(t->n_step >= t->rebalance_at){
                int test = t->yard->n_node;
                int opt_d = 0;
                while(test){
                        test = test / 2;
                        opt_d++;
                }
                t->n_step = 0;
                if(abs(t->max_depth - opt_d) > 10){
                        kdtree_relink(t);
                }
        }
        return OK;
ERROR:
        return FAIL;
}

int kdtree_rebalance(kdtree *t)
{
        ASSERT(t != NULL, "No tree");
        kdtree_relink(t);
        return OK;
ERROR:
        return FAIL;
}

/* Depth-first nearest-neighbour search below n (at depth). The tree is only read, so
   searches of one tree may run at the same time. */
int nn_search(const kdtree *t, const kd_node* n, const double *x, int depth, double* min_d, uint32_t *id)
{
        double d = 0.0;
        int cd = 0;
        int near_left = 0;

        if(n == NULL){
                return OK;
        }
        t->fptr_cmp(n->p, (double*) x, t->vec_len, &d);
        if(d < *min_d){
                *min_d = d;
                *id = n->id;
        }
        cd = depth % t->vec_len;
        near_left = x[cd] < n->p[cd];
        nn_search(t, near_left ? n->left : n->right, x, depth + 1, min_d, id);
        /* the far side can hold a nearer point only if the splitting plane is closer
           than the best distance so far */
        if(t->fptr_plane(n->p, (double*) x, cd, *min_d) == OK){
                nn_search(t, near_left ? n->right : n->left, x, depth + 1, min_d, id);
        }
        return OK;
}

int kdtree_nn_search(kdtree *t, double *x, uint32_t *id)
{
        double min_d = DBL_MAX;
        uint32_t best = 0;
        ASSERT(t != NULL, "No tree");
        ASSERT(x != NULL, "No point");
        ASSERT(id != NULL, "No id");
        ASSERT(t->root != NULL, "The tree is empty");
        RUN(nn_search(t, t->root, x, 0, &min_d, &best));
        *id = best;
        return OK;
ERROR:
        return FAIL;
}

int kdtree_bf_search(kdtree *t, double *x, uint32_t *id)
{
        double min_d = DBL_MAX;
        double d = 0.0;
        uint32_t best = 0;
        ASSERT(t != NULL, "No tree");
        ASSERT(x != NULL, "No point");
        ASSERT(id != NULL, "No id");
        ASSERT(t->yard->n_node > 0, "The tree is empty");
        for(int i = 0; i < t->yard->n_node; i++){
                t->fptr_cmp(x, t->yard->nodes[i]->p, t->vec_len, &d);
                if(d < min_d){
                        min_d = d;
                        best = t->yard->nodes[i]->id;
                }
        }
        *id = best;
        return OK;
ERROR:
        return FAIL;
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
        if(x < min_dist){
                return OK;
        }else{
                return FAIL;
        }
}

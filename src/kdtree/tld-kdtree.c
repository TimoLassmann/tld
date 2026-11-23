#include "tld-kdtree.h"

#include "../alloc/tld-alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* A tree node: p is the point (the tree's dim doubles, owned by the yard), id the
   caller's id. */
typedef struct tld__kdtree_node {
        double* p;
        struct tld__kdtree_node* left;
        struct tld__kdtree_node* right;
        uint32_t id;
} tld__kdtree_node;

/* The yard owns every node and its point. nodes[0 .. n_node) are in the tree; the
   slots up to n_alloc are allocated and free for later points. Building and
   rebalancing only re-link these nodes, so they never allocate and never move a
   point between nodes. */
struct tld__kdyard {
        tld__kdtree_node** nodes;
        int n_alloc;
        int n_node;
        int vec_len;
};

/* rebalance_at: insertions between balance checks (10000; the oom test lowers it). */
struct tld_kdtree {
        tld__kdtree_node* root;
        struct tld__kdyard* yard;
        int rebalance_at;
        int n_step;
        int depth;
        int max_depth;
        int min_depth;
        int vec_len;
};

static int tld__kdtree_nn(const tld_kdtree *t, const tld__kdtree_node* n, const double *x, int depth, double* min_d, uint32_t *id);

static int tld__kdyard_alloc(struct tld__kdyard ** yard, int size, int len);
static int tld__kdyard_reserve(struct tld__kdyard *y, int n);
static void tld__kdyard_free(struct tld__kdyard *n);

static void tld__kdtree_node_set(tld_kdtree *t, tld__kdtree_node *n, const double *x, uint32_t id);
static double tld__kdtree_dist(const double *a, const double *b, int vec_len);
static void tld__kdtree_relink(tld_kdtree *t);
static tld__kdtree_node* tld__kdtree_link_balanced(tld__kdtree_node **v, int n, int depth, int dim, int *max_depth);
static void tld__kdtree_select_nth(tld__kdtree_node **v, int n, int k, int cd);
static int tld__kdtree_node_alloc(tld__kdtree_node **node, int size);
static void tld__kdtree_node_free(tld__kdtree_node *n);

int tld__kdtree_node_alloc(tld__kdtree_node **node, int size)
{
        tld__kdtree_node* n = NULL;
        MMALLOC(n, sizeof(tld__kdtree_node));
        galloc(&n->p, size);
        *node = n;
        return OK;
ERROR:
        tld__kdtree_node_free(n);
        return FAIL;
}

void tld__kdtree_node_free(tld__kdtree_node *n)
{
        if(n){
                gfree(n->p);
                MFREE(n);
        }
}

int tld__kdyard_alloc(struct tld__kdyard ** yard, int size, int len)
{
        struct tld__kdyard* n = NULL;
        MMALLOC(n, sizeof(struct tld__kdyard));
        n->vec_len = len;
        RUN(tld__kdyard_reserve(n, size));
        *yard = n;
        return OK;
ERROR:
        tld__kdyard_free(n);
        return FAIL;
}

/* Room for n more nodes after the ones in the tree, growing by at least half. On
   failure the yard is unchanged apart from extra free slots, so the tree is intact. */
int tld__kdyard_reserve(struct tld__kdyard *y, int n)
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
        MREALLOC(y->nodes, sizeof(tld__kdtree_node*) * (size_t) new_size);
        while(y->n_alloc < new_size){
                y->nodes[y->n_alloc] = NULL;
                RUN(tld__kdtree_node_alloc(&y->nodes[y->n_alloc], y->vec_len));
                y->n_alloc++;
        }
        return OK;
ERROR:
        return FAIL;
}

void tld__kdyard_free(struct tld__kdyard *n)
{
        if(n){
                for(int i = 0; i < n->n_alloc; i++){
                        tld__kdtree_node_free(n->nodes[i]);
                }
                MFREE(n->nodes);
                MFREE(n);
        }
}

int tld_kdtree_alloc(tld_kdtree **tree, int dim)
{
        tld_kdtree* t = NULL;
        ASSERT(tree != NULL, "No output pointer");
        ASSERT(dim > 0, "Number of dimensions must be positive (got %d)", dim);
        MMALLOC(t, sizeof(tld_kdtree));
        t->rebalance_at = 10000;
        t->vec_len = dim;
        RUN(tld__kdyard_alloc(&t->yard, 1024, dim));
        *tree = t;
        return OK;
ERROR:
        tld_kdtree_free(t);
        return FAIL;
}

void tld_kdtree_free(tld_kdtree *t)
{
        if(t){
                tld__kdyard_free(t->yard);
                MFREE(t);
        }
}

int tld_kdtree_reset(tld_kdtree *t)
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

/* Put point x (the tree's vec_len doubles) and id into the free node n. */
void tld__kdtree_node_set(tld_kdtree *t, tld__kdtree_node *n, const double *x, uint32_t id)
{
        n->left = NULL;
        n->right = NULL;
        n->id = id;
        for(int i = 0; i < t->vec_len; i++){
                n->p[i] = x[i];
        }
}

/* Reorder v[0..n) so that v[k] holds the node whose coordinate cd would be k-th in
   sorted order, with no larger value before it and no smaller one after it. */
void tld__kdtree_select_nth(tld__kdtree_node **v, int n, int k, int cd)
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
                                tld__kdtree_node* tmp = v[i];
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
tld__kdtree_node* tld__kdtree_link_balanced(tld__kdtree_node **v, int n, int depth, int dim, int *max_depth)
{
        tld__kdtree_node* r = NULL;
        int mid = n / 2;
        if(n <= 0){
                return NULL;
        }
        tld__kdtree_select_nth(v, n, mid, depth % dim);
        r = v[mid];
        if(depth > *max_depth){
                *max_depth = depth;
        }
        r->left = tld__kdtree_link_balanced(v, mid, depth + 1, dim, max_depth);
        r->right = tld__kdtree_link_balanced(v + mid + 1, n - mid - 1, depth + 1, dim, max_depth);
        return r;
}

/* Rebuild the tree balanced from the nodes it holds. */
void tld__kdtree_relink(tld_kdtree *t)
{
        t->max_depth = 0;
        t->min_depth = 0;
        t->root = tld__kdtree_link_balanced(t->yard->nodes, t->yard->n_node, 0, t->vec_len, &t->max_depth);
}

int tld_kdtree_build(tld_kdtree *t, double **x, uint32_t *ids, int n)
{
        ASSERT(t != NULL, "No tree");
        ASSERT(n >= 0, "Negative number of points (%d)", n);
        ASSERT(n == 0 || (x != NULL && ids != NULL), "No points or ids");
        for(int i = 0; i < n; i++){
                ASSERT(x[i] != NULL, "Point %d is NULL", i);
        }
        RUN(tld__kdyard_reserve(t->yard, n));
        for(int i = 0; i < n; i++){
                tld__kdtree_node_set(t, t->yard->nodes[t->yard->n_node + i], x[i], ids[i]);
        }
        t->yard->n_node += n;
        tld__kdtree_relink(t);
        return OK;
ERROR:
        return FAIL;
}

int tld_kdtree_insert(tld_kdtree *t, double *x, uint32_t id)
{
        tld__kdtree_node* n = NULL;
        tld__kdtree_node** slot = NULL;
        int depth = 0;
        ASSERT(t != NULL, "No tree");
        ASSERT(x != NULL, "No point");

        RUN(tld__kdyard_reserve(t->yard, 1));
        n = t->yard->nodes[t->yard->n_node];
        tld__kdtree_node_set(t, n, x, id);
        t->yard->n_node++;

        slot = &t->root;
        while(*slot != NULL){
                const int cd = depth % t->vec_len;
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
                        tld__kdtree_relink(t);
                }
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_kdtree_rebalance(tld_kdtree *t)
{
        ASSERT(t != NULL, "No tree");
        tld__kdtree_relink(t);
        return OK;
ERROR:
        return FAIL;
}

/* Depth-first nearest-neighbour search below n (at depth). The tree is only read, so
   searches of one tree may run at the same time. */
int tld__kdtree_nn(const tld_kdtree *t, const tld__kdtree_node* n, const double *x, int depth, double* min_d, uint32_t *id)
{
        double d = 0.0;
        int cd = 0;
        int near_left = 0;

        if(n == NULL){
                return OK;
        }
        d = tld__kdtree_dist(n->p, x, t->vec_len);
        if(d < *min_d){
                *min_d = d;
                *id = n->id;
        }
        cd = depth % t->vec_len;
        near_left = x[cd] < n->p[cd];
        tld__kdtree_nn(t, near_left ? n->left : n->right, x, depth + 1, min_d, id);
        /* the far side can hold a nearer point only if the splitting plane is closer
           than the best distance so far */
        if((n->p[cd] - x[cd]) * (n->p[cd] - x[cd]) < *min_d){
                tld__kdtree_nn(t, near_left ? n->right : n->left, x, depth + 1, min_d, id);
        }
        return OK;
}

int tld_kdtree_nn_search(tld_kdtree *t, double *x, uint32_t *id)
{
        double min_d = DBL_MAX;
        uint32_t best = 0;
        ASSERT(t != NULL, "No tree");
        ASSERT(x != NULL, "No point");
        ASSERT(id != NULL, "No id");
        ASSERT(t->root != NULL, "The tree is empty");
        RUN(tld__kdtree_nn(t, t->root, x, 0, &min_d, &best));
        *id = best;
        return OK;
ERROR:
        return FAIL;
}

int tld_kdtree_bf_search(tld_kdtree *t, double *x, uint32_t *id)
{
        double min_d = DBL_MAX;
        double d = 0.0;
        uint32_t best = 0;
        ASSERT(t != NULL, "No tree");
        ASSERT(x != NULL, "No point");
        ASSERT(id != NULL, "No id");
        ASSERT(t->yard->n_node > 0, "The tree is empty");
        for(int i = 0; i < t->yard->n_node; i++){
                d = tld__kdtree_dist(x, t->yard->nodes[i]->p, t->vec_len);
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

/* Squared Euclidean distance between a and b (vec_len doubles). */
double tld__kdtree_dist(const double *a, const double *b, int vec_len)
{
        double d = 0.0;
        for(int i = 0; i < vec_len; i++){
                const double x = a[i] - b[i];
                d += x * x;
        }
        return d;
}

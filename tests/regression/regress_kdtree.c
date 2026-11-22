#include "tld.h"
#include "test.h"
#include <string.h>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

/* regress-kdtree: regression tests for the kdtree fixes: the sort comparator is a proper
   three-way compare (many tied coordinates), inserts that trigger the
   periodic rebalance check print nothing to stdout, and the yard grows
   past its initial size. Nearest-neighbour search must agree with brute
   force throughout. */

/* Queries are grid points + 0.25 on each axis, so the nearest neighbour is
   unique and both searches must return the same id. */
static int check_nn(kdtree *kd, struct rng_state *rng, int vec_len, int grid)
{
        double p[2];
        for(int i = 0; i < 2000; i++){
                uint32_t id_nn = 0;
                uint32_t id_bf = 0;
                for(int j = 0; j < vec_len; j++){
                        p[j] = (double) tl_random_int(rng, grid) + 0.25;
                }
                RUN(kdtree_nn_search(kd, p, &id_nn));
                RUN(kdtree_bf_search(kd, p, &id_bf));
                ASSERT(id_nn == id_bf, "nn search id %u differs from brute force id %u", id_nn, id_bf);
        }
        return OK;
ERROR:
        return FAIL;
}

static int test_build_with_ties(void)
{
        kdtree *kd = NULL;
        struct rng_state *rng = NULL;
        double **x = NULL;
        uint32_t *ids = NULL;
        const int n = 3000;
        const int vec_len = 2;

        RUN(init_rng(&rng, 5));
        RUN(kdtree_alloc(&kd, vec_len));
        galloc(&x, n, vec_len);
        galloc(&ids, n);
        /* integer coordinates on a coarse grid: many exact ties per axis,
           but distinct points, so the nearest neighbour of an off-grid
           query (+0.25) is unique */
        for(int i = 0; i < n; i++){
                x[i][0] = (double) (i % 60);
                x[i][1] = (double) (i / 60);
                ids[i] = (uint32_t) i;
        }
        RUN(kdtree_build(kd, x, ids, n, vec_len));
        RUN(check_nn(kd, rng, vec_len, 50));

        kdtree_free(kd);
        gfree(x);
        gfree(ids);
        free_rng(rng);
        return OK;
ERROR:
        kdtree_free(kd);
        if(x){
                gfree(x);
        }
        if(ids){
                gfree(ids);
        }
        free_rng(rng);
        return FAIL;
}

static int test_insert_quiet(void)
{
        kdtree *kd = NULL;
        struct rng_state *rng = NULL;
        double p[2];
        const int vec_len = 2;
        char path[] = "/tmp/tld_kdtree_XXXXXX";
        int fd = -1;
        int saved = -1;
        long n_bytes = 0;
        int status = OK;

        RUN(init_rng(&rng, 17));
        RUN(kdtree_alloc(&kd, vec_len));
        kd->rebalance_at = 500;

        fflush(stdout);
        fd = mkstemp(path);
        ASSERT(fd >= 0, "mkstemp failed");
        unlink(path);
        saved = dup(STDOUT_FILENO);
        ASSERT(saved >= 0, "dup failed");
        dup2(fd, STDOUT_FILENO);
        /* 5000 inserts: grows the yard past its initial 1024 nodes and runs
           the rebalance check ten times. Sorted input makes a deep tree so
           the rebalance itself is exercised too. */
        for(int i = 0; i < 5000 && status == OK; i++){
                p[0] = (double) i;
                p[1] = (double) (i % 7);
                status = kdtree_insert(kd, p, (uint32_t) i, vec_len);
        }
        fflush(stdout);
        dup2(saved, STDOUT_FILENO);
        close(saved);
        n_bytes = (long) lseek(fd, 0, SEEK_END);
        close(fd);
        fd = -1;
        ASSERT(status == OK, "kdtree_insert failed");
        ASSERT(n_bytes == 0, "kdtree_insert wrote %ld bytes to stdout", n_bytes);

        for(int i = 0; i < 2000; i++){
                uint32_t id_nn = 0;
                uint32_t id_bf = 0;
                p[0] = tl_random_double(rng) * 5000.0;
                p[1] = tl_random_double(rng) * 7.0;
                RUN(kdtree_nn_search(kd, p, &id_nn));
                RUN(kdtree_bf_search(kd, p, &id_bf));
                ASSERT(id_nn == id_bf, "nn search id %u differs from brute force id %u", id_nn, id_bf);
        }

        kdtree_free(kd);
        free_rng(rng);
        return OK;
ERROR:
        if(fd >= 0){
                close(fd);
        }
        kdtree_free(kd);
        free_rng(rng);
        return FAIL;
}

/* Both searches of an empty tree fail; nn_search used to leave *id as it was and
   bf_search to return id 0, both reporting OK. */
static int test_empty(void)
{
        kdtree *kd = NULL;
        double p[2] = {1.0, 2.0};
        uint32_t id = 77;
        RUN(kdtree_alloc(&kd, 2));
        ASSERT(kdtree_nn_search(kd, p, &id) == FAIL, "nn search of an empty tree returned OK");
        ASSERT(kdtree_bf_search(kd, p, &id) == FAIL, "bf search of an empty tree returned OK");
        ASSERT(id == 77, "a failed search changed *id to %u", id);
        RUN(kdtree_insert(kd, p, 5, 2));
        RUN(kdtree_reset(kd));
        ASSERT(kdtree_nn_search(kd, p, &id) == FAIL, "nn search after reset returned OK");
        kdtree_free(kd);
        return OK;
ERROR:
        kdtree_free(kd);
        return FAIL;
}

/* Searches only read the tree; they kept their recursion depth in the tree, so two
   threads searching one tree corrupted each other's descent. */
struct search_job {
        kdtree *kd;
        const double *q;
        const uint32_t *want;
        int n;
        int n_wrong;
};

static void *search_worker(void *arg)
{
        struct search_job *j = arg;
        for(int round = 0; round < 20; round++){
                for(int i = 0; i < j->n; i++){
                        uint32_t id = 0;
                        if(kdtree_nn_search(j->kd, (double*) (j->q + 3 * i), &id) != OK || id != j->want[i]){
                                j->n_wrong++;
                        }
                }
        }
        return NULL;
}

static int test_concurrent_search(void)
{
        kdtree *kd = NULL;
        struct rng_state *rng = NULL;
        double p[3];
        double *q = NULL;
        uint32_t *want = NULL;
        const int n_q = 500;
        pthread_t th[4];
        struct search_job job[4];

        RUN(init_rng(&rng, 23));
        RUN(kdtree_alloc(&kd, 3));
        for(int i = 0; i < 4000; i++){
                for(int j = 0; j < 3; j++){
                        p[j] = tl_random_double(rng);
                }
                RUN(kdtree_insert(kd, p, (uint32_t) i, 3));
        }
        galloc(&q, n_q * 3);
        galloc(&want, n_q);
        for(int i = 0; i < n_q; i++){
                for(int j = 0; j < 3; j++){
                        q[3 * i + j] = tl_random_double(rng);
                }
                RUN(kdtree_bf_search(kd, q + 3 * i, &want[i]));
        }
        for(int t = 0; t < 4; t++){
                job[t] = (struct search_job){kd, q, want, n_q, 0};
                ASSERT(pthread_create(&th[t], NULL, search_worker, &job[t]) == 0, "pthread_create failed");
        }
        for(int t = 0; t < 4; t++){
                pthread_join(th[t], NULL);
        }
        for(int t = 0; t < 4; t++){
                ASSERT(job[t].n_wrong == 0, "thread %d: %d of %d searches wrong", t, job[t].n_wrong, 20 * n_q);
        }
        gfree(q);
        gfree(want);
        kdtree_free(kd);
        free_rng(rng);
        return OK;
ERROR:
        gfree(q);
        gfree(want);
        kdtree_free(kd);
        free_rng(rng);
        return FAIL;
}

/* Points shorter than the tree's length: insert split on the point's own length while
   search split on the tree's, so the two disagreed. A reused node also kept stale
   coordinates past the new point's length. */
static int test_mixed_length(void)
{
        kdtree *kd = NULL;
        struct rng_state *rng = NULL;
        double p[2];
        RUN(init_rng(&rng, 29));
        RUN(kdtree_alloc(&kd, 2));
        for(int round = 0; round < 2; round++){
                for(int i = 0; i < 300; i++){
                        p[0] = tl_random_double(rng);
                        p[1] = tl_random_double(rng);
                        RUN(kdtree_insert(kd, p, (uint32_t) i, (i % 3 == 0) ? 1 : 2));
                }
                for(int i = 0; i < 2000; i++){
                        uint32_t id_nn = 0;
                        uint32_t id_bf = 0;
                        p[0] = tl_random_double(rng);
                        p[1] = tl_random_double(rng);
                        RUN(kdtree_nn_search(kd, p, &id_nn));
                        RUN(kdtree_bf_search(kd, p, &id_bf));
                        ASSERT(id_nn == id_bf, "round %d: nn search id %u differs from brute force id %u",
                               round, id_nn, id_bf);
                }
                RUN(kdtree_reset(kd));
        }
        kdtree_free(kd);
        free_rng(rng);
        return OK;
ERROR:
        kdtree_free(kd);
        free_rng(rng);
        return FAIL;
}

/* kdtree_build adds to the points already in the tree and leaves x and ids alone;
   it used to replace the root, leaving the earlier points unreachable by nn_search
   while bf_search still saw them. */
static int test_build_adds(void)
{
        kdtree *kd = NULL;
        double a[3][2] = {{0, 0}, {10, 0}, {0, 10}};
        double b[2][2] = {{10, 10}, {5, 5}};
        double *xa[3] = {a[0], a[1], a[2]};
        double *xb[2] = {b[0], b[1]};
        uint32_t ia[3] = {1, 2, 3};
        uint32_t ib[2] = {4, 5};
        double q[2] = {0.5, 0.5};
        uint32_t id = 0;
        RUN(kdtree_alloc(&kd, 2));
        RUN(kdtree_build(kd, xa, ia, 3, 2));
        RUN(kdtree_build(kd, xb, ib, 2, 2));
        ASSERT(xa[0] == a[0] && xa[1] == a[1] && xa[2] == a[2] && ia[0] == 1 && ia[1] == 2 && ia[2] == 3,
               "kdtree_build reordered its input");
        RUN(kdtree_nn_search(kd, q, &id));
        ASSERT(id == 1, "nearest to (0.5,0.5) is id %u, want 1", id);
        q[0] = 9.0;
        q[1] = 9.5;
        RUN(kdtree_nn_search(kd, q, &id));
        ASSERT(id == 4, "nearest to (9,9.5) is id %u, want 4", id);
        kdtree_free(kd);
        return OK;
ERROR:
        kdtree_free(kd);
        return FAIL;
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"ties", test_build_with_ties, "build with many tied coordinates (three-way compare)"},
        {"insert-quiet", test_insert_quiet, "5000 inserts: no stdout, yard grows, rebalance"},
        {"empty", test_empty, "searching an empty tree fails"},
        {"concurrent", test_concurrent_search, "4 threads search one tree at the same time"},
        {"mixed-length", test_mixed_length, "points shorter than the tree's length"},
        {"build-adds", test_build_adds, "kdtree_build adds to a tree and keeps its input"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the kdtree fixes; nearest-neighbour search must agree\n"
                  "with brute force throughout.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  ties          build with many tied coordinates (three-way compare)\n"
                  "  insert-quiet  5000 inserts: no stdout, yard grows, rebalance\n"
                  "  empty         searching an empty tree fails\n"
                  "  concurrent    4 threads search one tree at the same time\n"
                  "  mixed-length  points shorter than the tree's length\n"
                  "  build-adds    kdtree_build adds to a tree and keeps its input\n"
                  "\n"
                  "Example: regress-kdtree ties");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-kdtree: no check named '%s' (see -h)\n", argv[i]);
                        return 2;
                }
        }
        for(int k = 0; k < n_checks; k++){
                int want = argc == 1;
                for(int i = 1; i < argc; i++){
                        want |= strcmp(argv[i], checks[k].name) == 0;
                }
                if(!want){
                        continue;
                }
                SECTION_MSG("%s: %s", checks[k].name, checks[k].about);
                RUN(checks[k].fn());
                OK_MSG("%s passed", checks[k].name);
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

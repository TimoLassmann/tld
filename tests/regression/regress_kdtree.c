#include "tld.h"
#include "test.h"
#include <string.h>

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

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"ties", test_build_with_ties, "build with many tied coordinates (three-way compare)"},
        {"insert-quiet", test_insert_quiet, "5000 inserts: no stdout, yard grows, rebalance"},
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

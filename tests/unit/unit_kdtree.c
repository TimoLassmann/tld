#include "tld.h"
#include "test.h"
#include <stdio.h>
#include <time.h>

/* CPU time on stderr (KV_MSG) instead of TLD_START_TIMER/TLD_END_TIMER, which print to stdout */
#define T_START clock_t t0_ = clock();
#define T_END(WHAT) KV_MSG(WHAT, "%.3f s cpu", (double)(clock() - t0_) / CLOCKS_PER_SEC);

int test_search_speed(int n);
int test_random_inserts(void);
int test_build_opt_tree(void);

int rnd_mat_alloc(double ***mat, int dim1, int dim2, int seed);
int rnd_mat_free(double **x, int dim1);

/* unit-kdtree X,Y ... QX,QY: nearest neighbour of the last point among the others. */
static int demo(int n, char **args)
{
        tld_kdtree *kd = NULL;
        double **pts = NULL;
        double q[2];
        uint32_t id_nn = 0;
        uint32_t id_bf = 0;
        int np = n - 1;
        if(np < 1){
                ERROR_MSG("give at least one point and a query point");
        }
        galloc(&pts, n, 2);
        for(int i = 0; i < n; i++){
                if(sscanf(args[i], "%lf,%lf", &pts[i][0], &pts[i][1]) != 2){
                        ERROR_MSG("expected X,Y, got %s", args[i]);
                }
        }
        q[0] = pts[np][0];
        q[1] = pts[np][1];
        SECTION_MSG("%d points, query (%g, %g)", np, q[0], q[1]);
        RUN(tld_kdtree_alloc(&kd, 2));
        for(int i = 0; i < np; i++){
                RUN(tld_kdtree_insert(kd, pts[i], (uint32_t)i, 2));
                DEBUG_MSG("insert #%d (%g, %g)", i, pts[i][0], pts[i][1]);
        }
        RUN(tld_kdtree_rebalance(kd));
        KV_MSG("depth", "%d..%d", kd->min_depth, kd->max_depth);
        RUN(tld_kdtree_nn_search(kd, q, &id_nn));
        RUN(tld_kdtree_bf_search(kd, q, &id_bf));
        fprintf(stdout, "nearest (k-d tree):    #%u (%g, %g)\n", id_nn, pts[id_nn][0], pts[id_nn][1]);
        fprintf(stdout, "nearest (brute force): #%u (%g, %g)\n", id_bf, pts[id_bf][0], pts[id_bf][1]);
        if(id_nn == id_bf){
                OK_MSG("both searches agree");
        } else {
                WARNING_MSG("searches differ (equal distances?)");
        }
        tld_kdtree_free(kd);
        gfree(pts);
        return OK;
ERROR:
        if(kd){
                tld_kdtree_free(kd);
        }
        gfree(pts);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[X,Y ... QX,QY]",
                  "With no arguments: builds a 1032-point tree in 1000 dimensions (build,\n"
                  "reset, insert, rebalance), checks k-d against brute-force search after random\n"
                  "inserts, and times both searches on 1000..100000 points.\n"
                  "With points: inserts every X,Y but the last into a 2-D tree and prints the\n"
                  "nearest neighbour of the last one (QX,QY) by k-d and by brute-force search.\n"
                  "Example: unit-kdtree 0,0 1,1 5,5 0.9,1.2");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }
        SECTION_MSG("build an optimal tree");
        RUN(test_build_opt_tree());
        OK_MSG("built");

        SECTION_MSG("random inserts");
        RUN(test_random_inserts());
        OK_MSG("k-d and brute-force search agree");

        SECTION_MSG("search speed");
        for(int i = 1000; i < 1000000;i = i *10){
                test_search_speed(i);
        }

        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int test_search_speed(int n)
{
        tld_kdtree* kd = NULL;
        struct tld_rng* rng = NULL;
        double* p = NULL;
        int vec_len = 2;

        RUN(tld_kdtree_alloc(&kd, vec_len));

        galloc(&p, vec_len);
        RUN(tld_rng_alloc(&rng,0));

        for(int i = 0; i < n;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tld_rng_double(rng);
                }
                tld_kdtree_insert(kd, p, i,vec_len);
        }

        tld_rng_set_seed(rng, 42);
        KV_MSG("points", "%d", n);
        {
        T_START
        for(int i = 0; i < 1000;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tld_rng_double(rng);
                }
                uint32_t id_bf;
                tld_kdtree_bf_search(kd,p,&id_bf);
        }
        T_END("brute force")
        }
        tld_rng_set_seed(rng, 42);
        {
        T_START
        for(int i = 0; i < 1000;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tld_rng_double(rng);
                }
                uint32_t id_nn;
                tld_kdtree_nn_search(kd,p,&id_nn);
        }
        T_END("k-d search")
        }

        gfree(p);
        tld_kdtree_free(kd);
        tld_rng_free(rng);
        return OK;
ERROR:
        if(p){
                gfree(p);
        }

        tld_kdtree_free(kd);
        tld_rng_free(rng);
        return FAIL;
}

int test_random_inserts(void)
{
        tld_kdtree* kd = NULL;
        struct tld_rng* rng = NULL;
        double* p = NULL;
        int vec_len = 2;

        RUN(tld_kdtree_alloc(&kd, vec_len));

        galloc(&p, vec_len);
        RUN(tld_rng_alloc(&rng,0));

        for(int i = 0; i < 15;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tld_rng_double(rng);
                }
                tld_kdtree_insert(kd, p, i,vec_len);
        }
        tld_kdtree_rebalance(kd);

        DEBUG_MSG("Insertion done - let's search !");
        for(int i = 0; i < 1;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tld_rng_double(rng);
                }
                uint32_t id_nn;
                uint32_t id_bf;
                tld_kdtree_nn_search(kd,p,&id_nn);
                tld_kdtree_bf_search(kd,p,&id_bf);
                ASSERT(id_bf == id_nn, "Different ID!!!!");

        }

        gfree(p);
        tld_kdtree_free(kd);
        tld_rng_free(rng);
        return OK;
ERROR:
        if(p){
                gfree(p);
        }

        tld_kdtree_free(kd);
        tld_rng_free(rng);
        return FAIL;
}

int test_build_opt_tree(void)
{
        tld_kdtree* kd = NULL;

        double** x = NULL;
        uint32_t* ids = NULL;
        int dim1 = 1032;
        int dim2 = 1000;

        RUN(tld_kdtree_alloc(&kd, dim2));

        RUN(rnd_mat_alloc(&x,dim1 , dim2, 0));

        galloc(&ids, dim1);
        for(int i = 0;i < dim1;i++){
                ids[i] = i;
        }

        {
        T_START
                tld_kdtree_build(kd, x, ids, dim1,  dim2);

        T_END("build")
        }

        KV_MSG("depth", "min %d max %d", kd->min_depth, kd->max_depth);
        tld_kdtree_reset(kd);
        tld_kdtree_build(kd, x, ids, dim1,  dim2);
        KV_MSG("depth", "min %d max %d", kd->min_depth, kd->max_depth);
        tld_kdtree_reset(kd);
        for(int i = 0; i < dim1;i++){
                tld_kdtree_insert(kd, x[i],i, dim2);
        }
        KV_MSG("depth", "min %d max %d", kd->min_depth, kd->max_depth);

        tld_kdtree_rebalance(kd);
        KV_MSG("depth", "min %d max %d", kd->min_depth, kd->max_depth);

        rnd_mat_free(x, dim1);
        gfree(ids);
        tld_kdtree_free(kd);
        return OK;
ERROR:
        return FAIL;
}

int rnd_mat_alloc(double ***mat, int dim1, int dim2, int seed)
{
        struct tld_rng* rng = NULL;
        double** x = NULL;

        RUN(tld_rng_alloc(&rng,seed));

        MMALLOC(x, sizeof(double* ) * dim1);
        for(int i = 0; i < dim1;i++){
                x[i] = NULL;
                MMALLOC(x[i], sizeof(double) * dim2);
                for(int j = 0; j < dim2;j++){
                        x[i][j] = tld_rng_double(rng);
                }
        }

        *mat = x;
        tld_rng_free(rng);
        return OK;
ERROR:
        return FAIL;
}

int rnd_mat_free(double **x, int dim1)
{
        for(int i = 0; i < dim1;i++){
                MFREE(x[i]);
        }
        MFREE(x);
        return OK;
}

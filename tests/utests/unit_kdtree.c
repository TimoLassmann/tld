#include "tld.h"
#include <stdio.h>

int test_search_speed(int n);
int test_random_inserts(void);
int test_build_opt_tree(void);
/* int test_median_sort(void); */

int rnd_mat_alloc(double ***mat, int dim1, int dim2, int seed);
int rnd_mat_free(double **x, int dim1);

int main(void)
{
        /* RUN(test_median_sort()); */
        LOG_MSG("All ok");
        RUN(test_build_opt_tree());

        LOG_MSG("Random Insert test ");
        RUN(test_random_inserts());


        /* exit(0); */
        LOG_MSG("Speed testing:");
        for(int i = 1000; i < 1000000;i = i *10){
                test_search_speed(i);
        }

        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int test_search_speed(int n)
{
        kdtree* kd = NULL;
        struct rng_state* rng = NULL;
        double* p = NULL;
        int vec_len = 2;

        RUN(kdtree_alloc(&kd, vec_len));

        galloc(&p, vec_len);
        RUN(init_rng(&rng,0));

        for(int i = 0; i < n;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tl_random_double(rng);
                }
                /* LOG_MSG("Insert %d",i); */
                kdtree_insert(kd, p, i,vec_len);
        }

        tld_rng_set_seed(rng, 42);
        /* kdtree_debug_print(kd->root, 0); */
        LOG_MSG("Brute force search (%d):",n);
        TLD_START_TIMER
        for(int i = 0; i < 1000;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tl_random_double(rng);
                }
                /* uint32_t id_nn; */
                uint32_t id_bf;
                /* kdtree_nn_search(kd,p,&id_nn); */
                kdtree_bf_search(kd,p,&id_bf);
                /* ASSERT(id_bf == id_nn, "Different ID!!!!"); */
        }
        TLD_END_TIMER
        tld_rng_set_seed(rng, 42);
        /* kdtree_debug_print(kd->root, 0); */
        LOG_MSG("K-d search (%d):",n);
        TLD_START_TIMER
        for(int i = 0; i < 1000;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tl_random_double(rng);
                }
                uint32_t id_nn;
                /* uint32_t id_bf; */
                kdtree_nn_search(kd,p,&id_nn);
                /* kdtree_bf_search(kd,p,&id_bf); */
                /* ASSERT(id_bf == id_nn, "Different ID!!!!"); */
        }
        TLD_END_TIMER


        gfree(p);
        kdtree_free(kd);
        free_rng(rng);
        return OK;
ERROR:
        if(p){
                gfree(p);
        }

        kdtree_free(kd);
        free_rng(rng);
        return FAIL;
}


int test_random_inserts(void)
{
        kdtree* kd = NULL;
        struct rng_state* rng = NULL;
        double* p = NULL;
        int vec_len = 2;

        RUN(kdtree_alloc(&kd, vec_len));

        galloc(&p, vec_len);
        RUN(init_rng(&rng,0));

        for(int i = 0; i < 15;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tl_random_double(rng);
                }
                /* LOG_MSG("Insert %d",i); */
                kdtree_insert(kd, p, i,vec_len);
        }
        kdtree_rebalance(kd);

        kdtree_debug_print(kd->root, 0);

        kdtree_debug_chk(kd->root, 0);
        LOG_MSG("Insertion done - let's search !");
        for(int i = 0; i < 1;i++){
                for(int j = 0; j < vec_len;j++){
                        p[j] = tl_random_double(rng);
                }
                uint32_t id_nn;
                uint32_t id_bf;
                kdtree_nn_search(kd,p,&id_nn);
                kdtree_bf_search(kd,p,&id_bf);
                ASSERT(id_bf == id_nn, "Different ID!!!!");

                /* LOG_MSG("%d %d", id_bf,id_nn); */

                /* LOG_MSG("Insert %d",i); */
                /* kdtree_insert(kd, p, i,vec_len); */
        }



        gfree(p);
        kdtree_free(kd);
        free_rng(rng);
        return OK;
ERROR:
        if(p){
                gfree(p);
        }

        kdtree_free(kd);
        free_rng(rng);
        return FAIL;
}

int test_build_opt_tree(void)
{
        kdtree* kd = NULL;

        double** x = NULL;
        uint32_t* ids = NULL;
        /* kd_node* root = NULL; */
        int dim1 = 1032;
        int dim2 = 1000;

        RUN(kdtree_alloc(&kd, dim2));

        RUN(rnd_mat_alloc(&x,dim1 , dim2, 0));

        galloc(&ids, dim1);
        for(int i = 0;i < dim1;i++){
                ids[i] = i;
        }

        TLD_START_TIMER
                kdtree_build(kd, x, ids, dim1,  dim2);
        /* kdtree_build(x, dim1,  dim2, 0, &root); */

        TLD_END_TIMER
        kdtree_debug_print(kd->root, 0);

        fprintf(stdout,"%d\tmin d\n%d\tmax_d\n", kd->min_depth, kd->max_depth);
        /* kdtree_node_free(root); */
        kdtree_reset(kd);
        kdtree_build(kd, x, ids, dim1,  dim2);
        fprintf(stdout,"%d\tmin d\n%d\tmax_d\n", kd->min_depth, kd->max_depth);
        kdtree_reset(kd);
        for(int i = 0; i < dim1;i++){
                kdtree_insert(kd, x[i],i, dim2);
        }
        /* kdtree_debug_print(kd->root, 0); */
        fprintf(stdout,"%d\tmin d\n%d\tmax_d\n", kd->min_depth, kd->max_depth);

        kdtree_rebalance(kd);
        /* kdtree_debug_print(kd->root, 0); */
        fprintf(stdout,"%d\tmin d\n%d\tmax_d\n", kd->min_depth, kd->max_depth);



        rnd_mat_free(x, dim1);
        gfree(ids);
        kdtree_free(kd);
        return OK;
ERROR:
        return FAIL;
}

/* int test_median_sort(void) */
/* { */
/*         struct rng_state* rng = NULL; */

/*         float* tmp = NULL; */
/*         float** x = NULL; */
/*         int dim1 = 10; */
/*         int dim2 = 10; */

/*         int test_d = 0; */

/*         galloc(&tmp, dim1); */

/*         RUN(init_rng(&rng,0)); */


/*         RUN(rnd_mat_alloc(&x,dim1 , dim2, 0)); */


/*         for(int i = 0; i < dim1;i++){ */
/*                 tmp[i] = x[i][test_d]; */
/*         } */

/*         TLD_SORT_DESC(tmp, dim1); */

/*         LOG_MSG("Median: %f", tmp[dim1 /2]); */

/*         fprintf(stdout,"\n"); */
/*         for(int i = 0; i < dim1;i++){ */

/*                 for(int j = 0; j < dim2;j++){ */
/*                         fprintf(stdout,"%0.4f ", x[i][j]); */

/*                 } */
/*                 fprintf(stdout,"\n"); */
/*         } */
/*         fprintf(stdout,"\n"); */



/*         quickselect(x, 0, dim1-1, dim1 /2 , test_d); */

/*         fprintf(stdout,"\n"); */
/*         for(int i = 0; i < dim1;i++){ */

/*                 for(int j = 0; j < dim2;j++){ */
/*                         fprintf(stdout,"%0.4f ", x[i][j]); */

/*                 } */
/*                 fprintf(stdout,"\n"); */
/*         } */
/*         fprintf(stdout,"\n"); */


/*         rnd_mat_free(x, dim1); */
/*         gfree(tmp); */
/*         free_rng(rng); */
/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */


int rnd_mat_alloc(double ***mat, int dim1, int dim2, int seed)
{
        struct rng_state* rng = NULL;
        double** x = NULL;

        RUN(init_rng(&rng,seed));


        MMALLOC(x, sizeof(double* ) * dim1);
        for(int i = 0; i < dim1;i++){
                x[i] = NULL;
                MMALLOC(x[i], sizeof(double) * dim2);
                for(int j = 0; j < dim2;j++){
                        x[i][j] = tl_random_double(rng);
                }
        }

        *mat = x;
        free_rng(rng);
        return OK;
ERROR:
        return FAIL;
}

int rnd_mat_free(double **x, int dim1)
{
        for(int i = 0; i < dim1;i++){
                /* x[i] = NULL; */
                MFREE(x[i]);
        }
        MFREE(x);
        return OK;
}

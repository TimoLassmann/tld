#include "tld.h"
#include <stdio.h>


int test_build_opt_tree(void);
/* int test_median_sort(void); */

int rnd_mat_alloc(float ***mat, int dim1, int dim2, int seed);
int rnd_mat_free(float **x, int dim1);

int main(void)
{
        /* RUN(test_median_sort()); */
        LOG_MSG("All ok");
        test_build_opt_tree();

        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}


int test_build_opt_tree(void)
{
        kdtree* kd = NULL;

        float** x = NULL;
        kd_node* root = NULL;
        int dim1 = 1032;
        int dim2 = 1000;

        RUN(kdtree_alloc(&kd, dim2));

        RUN(rnd_mat_alloc(&x,dim1 , dim2, 0));

        TLD_START_TIMER
         kdtree_build(kd,&kd->root, x, dim1,  dim2, 0);
        /* kdtree_build(x, dim1,  dim2, 0, &root); */

        TLD_END_TIMER
        kdtree_debug_print(kd->root, 0);
        /* kdtree_node_free(root); */
        rnd_mat_free(x, dim1);

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


int rnd_mat_alloc(float ***mat, int dim1, int dim2, int seed)
{
        struct rng_state* rng = NULL;
        float** x = NULL;

        RUN(init_rng(&rng,seed));


        MMALLOC(x, sizeof(float* ) * dim1);
        for(int i = 0; i < dim1;i++){
                x[i] = NULL;
                MMALLOC(x[i], sizeof(float) * dim2);
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

int rnd_mat_free(float **x, int dim1)
{
        for(int i = 0; i < dim1;i++){
                /* x[i] = NULL; */
                MFREE(x[i]);
        }
        MFREE(x);
        return OK;
}

#include "tld.h"
#include <stdio.h>


int test_median_sort(void);
int main(void)
{
        RUN(test_median_sort());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}


int test_median_sort(void)
{
        struct rng_state* rng = NULL;

        float* tmp = NULL;
        float** x = NULL;
        int dim1 = 10;
        int dim2 = 10;

        int test_d = 0;

        galloc(&tmp, dim1);

        RUN(init_rng(&rng,0));


        MMALLOC(x, sizeof(float* ) * dim1);
        for(int i = 0; i < dim1;i++){
                x[i] = NULL;
                MMALLOC(x[i], sizeof(float) * dim2);
                for(int j = 0; j < dim2;j++){
                        x[i][j] = tl_random_double(rng);
                }
        }

        for(int i = 0; i < dim1;i++){
                tmp[i] = x[i][test_d];
        }

        TLD_SORT_DESC(tmp, dim1);

        LOG_MSG("Median: %f", tmp[dim1 /2]);

        fprintf(stdout,"\n");
        for(int i = 0; i < dim1;i++){

                for(int j = 0; j < dim2;j++){
                        fprintf(stdout,"%0.2f ", x[i][j]);

                }
                fprintf(stdout,"\n");
        }
        fprintf(stdout,"\n");


        quickselect(x, 0, dim1-1, dim1 /2 , test_d);

        fprintf(stdout,"\n");
        for(int i = 0; i < dim1;i++){

                for(int j = 0; j < dim2;j++){
                        fprintf(stdout,"%0.2f ", x[i][j]);

                }
                fprintf(stdout,"\n");
        }
        fprintf(stdout,"\n");


        for(int i = 0; i < dim1;i++){
                /* x[i] = NULL; */
                MFREE(x[i]);
        }

        MFREE(x);
        gfree(tmp);
        free_rng(rng);
        return OK;
ERROR:
        return FAIL;
}

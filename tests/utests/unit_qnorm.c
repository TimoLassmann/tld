#include "tld.h"
#include <stdio.h>

int qnorm_test(void);
int qnorm_big_test(void);
int main(void)
{
        LOG_MSG("qnorm test");
        RUN(qnorm_test());
        LOG_MSG("qnorm big test");
        RUN(qnorm_big_test());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}


int qnorm_test(void)
{
        double** data = NULL;

        int n = 4;
        int m = 3;

        galloc(&data,n,m);

        data[0][0] = 5.0;
        data[0][1] = 4.0;
        data[0][2] = 3.0;
        LOG_MSG("1");
        data[1][0] = 2.0;
        data[1][1] = 1.0;
        data[1][2] = 4.0;
        LOG_MSG("2");
        data[2][0] = 3.0;
        data[2][1] = 4.0;
        data[2][2] = 6.0;
        LOG_MSG("3");
        data[3][0] = 4.0;
        data[3][1] = 2.0;
        data[3][2] = 8.0;

        for(int i = 0; i < n;i++){
                for(int j = 0; j < m;j++){
                        data[i][j] = log(data[i][j]+1.0);
                }

        }

        RUN(norm_quantile(data,  n, m));

        gfree(data);
        return OK;
ERROR:
        return FAIL;

}

int qnorm_big_test(void)
{
        struct rng_state* rng = NULL;


        double** data = NULL;

        int n = 50000;
        int m = 10;

        RUN(init_rng(&rng, 0));

        LOG_MSG("ONE");
        galloc(&data,n,m);
        for(int i = 0; i < n;i++){
                for(int j = 0; j < m;j++){
                        data[i][j] = tl_random_gaussian(rng, (double) (j + 1), (double)(j + 1));
                }
        }

        RUN(norm_quantile(data,  n, m));

        free_rng(rng);
        gfree(data);
        return OK;
ERROR:
        if(rng){
                free_rng(rng);
        }
        if(data){
                gfree(data);
        }
        return FAIL;

}

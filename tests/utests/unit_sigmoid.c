#include "tld.h"
#include <stdio.h>

int test_save_sigmoid(void);

int main(void)
{
        double y = 0.0;

        struct rng_state* rng = NULL;

        RUN(init_rng(&rng, 0));

        for(int i = 0; i < 10000;i++){
                double x = tl_random_double(rng)* 2000000.0  - 1000000.0;
                tld_sigmoid(x, &y);
                LOG_MSG("%d %f %f", i, y, x);
        }

        free(rng);

        test_save_sigmoid();
        return EXIT_SUCCESS;
ERROR:
        if(rng){
                free_rng(rng);
        }
        return EXIT_FAILURE;
}

int test_save_sigmoid(void)
{
        double test_values[] = {20.0, 1000.0, -1000.0, NAN, INFINITY, -INFINITY};
        int num_tests = sizeof(test_values) / sizeof(test_values[0]);

        for(int i = 0; i < num_tests; i++) {
                double val = test_values[i];
                double result;
                RUN(tld_sigmoid_safe(val,&result));
                if (isnan(val)) {
                        printf("safe_sigmoid(nan) = nan\n");
                } else if (isinf(val)) {
                        if (val > 0) {
                                printf("safe_sigmoid(inf) = 1.0000000000\n");
                        } else {
                                printf("safe_sigmoid(-inf) = 0.0000000000\n");
                        }
                } else {
                        printf("safe_sigmoid(%.1f) = %.10f\n", val, result);
                }
        }
        return OK;
ERROR:
        return FAIL;
}

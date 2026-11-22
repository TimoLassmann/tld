#include "tld.h"
#include "test.h"
#include <stdio.h>

int test_save_sigmoid(void);

/* unit-sigmoid X ...: tld_sigmoid and tld_sigmoid_safe of each X. */
static int demo(int n, char **args)
{
        SECTION_MSG("sigmoid of %d value%s", n, n == 1 ? "" : "s");
        for(int i = 0; i < n; i++){
                char *end = NULL;
                double x = strtod(args[i], &end);
                double y = 0.0;
                double ys = 0.0;
                ASSERT(end != args[i] && *end == 0, "not a number: %s", args[i]);
                RUN(tld_sigmoid_safe(x, &ys));
                if(isfinite(x)){
                        /* tld_sigmoid rejects inf and nan; tld_sigmoid_safe takes them */
                        RUN(tld_sigmoid(x, &y));
                        fprintf(stdout, "x %g\tsigmoid %.10f\tsafe %.10f\n", x, y, ys);
                } else {
                        fprintf(stdout, "x %g\tsigmoid -\tsafe %.10f\n", x, ys);
                }
        }
        OK_MSG("done");
        return OK;
ERROR:
        return FAIL;
}

int main(int argc, char *argv[])
{
        double y = 0.0;

        struct tld_rng* rng = NULL;

        test_init(&argc, argv, "[X ...]",
                  "With no arguments: tld_sigmoid on 10000 random values in [-1e6, 1e6], then\n"
                  "tld_sigmoid_safe on 20, 1000, -1000, nan, inf and -inf.\n"
                  "With X values: prints tld_sigmoid and tld_sigmoid_safe of each (inf and nan\n"
                  "only through tld_sigmoid_safe).\n"
                  "Negative numbers need -- first. Example: unit-sigmoid -- -2 0 0.5 40 inf");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }

        RUN(tld_rng_alloc(&rng, 0));

        SECTION_MSG("tld_sigmoid on 10000 random values");
        for(int i = 0; i < 10000;i++){
                double x = tld_rng_double(rng)* 2000000.0  - 1000000.0;
                tld_sigmoid(x, &y);
                DEBUG_MSG("%d %f %f", i, y, x);
                tld_progress(i + 1, 10000, "sigmoid");
        }

        tld_rng_free(rng);
        rng = NULL;

        SECTION_MSG("tld_sigmoid_safe on edge values");
        RUN(test_save_sigmoid());
        OK_MSG("all values computed");
        return EXIT_SUCCESS;
ERROR:
        if(rng){
                tld_rng_free(rng);
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
                        KV_MSG("safe(nan)", "nan");
                } else if (isinf(val)) {
                        if (val > 0) {
                                KV_MSG("safe(inf)", "1.0000000000");
                        } else {
                                KV_MSG("safe(-inf)", "0.0000000000");
                        }
                } else {
                        char key[32];
                        snprintf(key, sizeof key, "safe(%.1f)", val);
                        KV_MSG(key, "%.10f", result);
                }
        }
        return OK;
ERROR:
        return FAIL;
}

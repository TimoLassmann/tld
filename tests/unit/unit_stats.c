#include "tld.h"
#include "test.h"
#include <stdio.h>

int test_standardisation(void);
int test_covariance(void);

/* unit-stats X ...: summary statistics of the numbers. */
static int demo(int n, char **args)
{
        double *v = NULL;
        double mean = 0.0;
        double var = 0.0;
        double svar = 0.0;
        double sd = 0.0;
        double h = 0.0;
        galloc(&v, n);
        for(int i = 0; i < n; i++){
                char *end = NULL;
                v[i] = strtod(args[i], &end);
                ASSERT(end != args[i] && *end == 0, "not a number: %s", args[i]);
        }
        SECTION_MSG("statistics of %d value%s", n, n == 1 ? "" : "s");
        RUN(tld_mean(v, n, &mean));
        RUN(tld_pop_variance(v, n, &var));
        fprintf(stdout, "n:                 %d\n", n);
        fprintf(stdout, "mean:              %g\n", mean);
        fprintf(stdout, "pop variance:      %g\n", var);
        if(n > 1){
                RUN(tld_sample_variance(v, n, &svar));
                fprintf(stdout, "sample variance:   %g\n", svar);
        }
        /* tld_shannon wants a distribution: only for non-negative values */
        int nonneg = 1;
        for(int i = 0; i < n; i++){
                if(v[i] < 0.0){
                        nonneg = 0;
                }
        }
        if(nonneg && tld_shannon(v, n, &h) == OK){
                fprintf(stdout, "shannon entropy:   %g\n", h);
        }
        if(n > 1 && var > 0.0){
                RUN(tld_standardise(v, n, &mean, &sd));
                fprintf(stdout, "standardised:     ");
                for(int i = 0; i < n; i++){
                        fprintf(stdout, " %g", v[i]);
                }
                fprintf(stdout, "\n");
                DEBUG_MSG("tld_standardise: mean %g, stdev %g", mean, sd);
        }
        OK_MSG("done");
        gfree(v);
        return OK;
ERROR:
        gfree(v);
        return FAIL;
}

int main(int argc, char *argv[])
{
        double* vec = NULL;
        double var = 0.0;

        test_init(&argc, argv, "[X ...]",
                  "With no arguments: sample variance of 1..10, tld_standardise applied twice,\n"
                  "and population and sample covariance of two random vectors.\n"
                  "With numbers: prints n, mean, population and sample variance, Shannon\n"
                  "entropy (non-negative input) and the standardised values.\n"
                  "Example: unit-stats 2 4 4 4 5 5 7 9");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }
        galloc(&vec,10);

        for(int i = 0; i < 10;i++){
                vec[i] = (double) (i+1) ;
        }

        SECTION_MSG("sample variance of 1..10");
        tld_sample_variance(vec, 10, &var);
        for(int i = 0; i < 10;i++){
                DEBUG_MSG("%d %f",i, vec[i]);
        }
        KV_MSG("var", "%f", var);
        gfree(vec);

        SECTION_MSG("standardisation");
        test_standardisation();
        SECTION_MSG("covariance");
        test_covariance();
        OK_MSG("statistics computed");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;

}

int test_standardisation(void)
{
        double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
        int size = sizeof(data) / sizeof(data[0]);
        double mean = 0.0;
        double stdev = 0.0;

        RUN(tld_standardise(data, size, &mean, &stdev));
        DEBUG_MSG("After:");
        for(int i = 0; i < size;i++){
                DEBUG_MSG("%d %f",i, data[i]);
        }
        KV_MSG("input", "mean %f stdev %f", mean, stdev);
        RUN(tld_standardise(data, size, &mean, &stdev));
        DEBUG_MSG("After2:");
        for(int i = 0; i < size;i++){
                DEBUG_MSG("%d %f",i, data[i]);
        }
        KV_MSG("output", "mean %f stdev %f (standardised again: 0 and 1)", mean, stdev);
        return OK;
ERROR:
        return FAIL;

}

int test_covariance(void)
{

        struct tld_rng* rng = NULL;

        double* v1 = NULL;
        double* v2 = NULL;
        double cov;
        int n = 10;

        RUN(tld_rng_alloc(&rng, 43));
        galloc(&v1, n);
        galloc(&v2, n);
        for(int i = 0; i < n;i++){
                v1[i] = tld_rng_double(rng);
                v2[i] = tld_rng_double(rng);
        }

        tld_pop_covariance(v1, v2, n, &cov);

        KV_MSG("cov", "%f (population)", cov);
        tld_sample_covariance(v1, v2, n, &cov);
        KV_MSG("cov", "%f (sample)", cov);

        /* R code to check the values (-vv) */
        char line[1024];
        int o = 0;
        for(int i = 0; i < n;i++){
                o += snprintf(line + o, sizeof line - o, "%s%.17g", i ? "," : "", v1[i]);
        }
        DEBUG_MSG("vec_a <- c(%s)", line);
        o = 0;
        for(int i = 0; i < n;i++){
                o += snprintf(line + o, sizeof line - o, "%s%.17g", i ? "," : "", v2[i]);
        }
        DEBUG_MSG("vec_b <- c(%s)", line);
        DEBUG_MSG("cov(x = vec_a, y = vec_b)");

        gfree(v2);
        gfree(v1);

        tld_rng_free(rng);

        return OK;
ERROR:

        if(rng){
                tld_rng_free(rng);
        }
        if(v2){
                gfree(v2);
        }
        if(v1){
                gfree(v1);
        }
        return FAIL;
}

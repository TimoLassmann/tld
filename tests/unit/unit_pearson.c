#include "tld.h"
#include "test.h"
#include <stdio.h>

int test_pearson(void);

/* Parse "1,2,3" into a new array; returns the count or -1. */
static int parse_list(const char *s, double **out)
{
        double *v = NULL;
        int n = 1;
        for(const char *p = s; *p; p++){
                if(*p == ','){
                        n++;
                }
        }
        galloc(&v, n);
        for(int i = 0; i < n; i++){
                char *end = NULL;
                v[i] = strtod(s, &end);
                if(end == s){
                        ERROR_MSG("not a number list: %s", s);
                }
                s = (*end == ',') ? end + 1 : end;
        }
        *out = v;
        return n;
ERROR:
        gfree(v);
        return -1;
}

/* unit-pearson X1,X2,... Y1,Y2,...: correlation and covariances of two vectors. */
static int demo(char *xs, char *ys)
{
        double *a = NULL;
        double *b = NULL;
        double cor = 0.0;
        double cov = 0.0;
        int n = parse_list(xs, &a);
        int m = parse_list(ys, &b);
        if(n < 0 || m < 0){
                goto ERROR;
        }
        ASSERT(n == m, "X has %d values, Y has %d", n, m);
        SECTION_MSG("Pearson correlation of %d pairs", n);
        KV_MSG("x", "%s", xs);
        KV_MSG("y", "%s", ys);
        RUN(tld_pearson_corr(a, b, n, &cor));
        fprintf(stdout, "pearson r:         %f\n", cor);
        RUN(tld_sample_covariance(a, b, n, &cov));
        fprintf(stdout, "sample covariance: %f\n", cov);
        RUN(tld_pop_covariance(a, b, n, &cov));
        fprintf(stdout, "pop covariance:    %f\n", cov);
        OK_MSG("done");
        gfree(a);
        gfree(b);
        return OK;
ERROR:
        gfree(a);
        gfree(b);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[X1,X2,... Y1,Y2,...]",
                  "With no arguments: tld_pearson_corr on a linear, a sine and a constant pair\n"
                  "of vectors.\n"
                  "With X and Y (comma-separated numbers, same count): prints Pearson's r and\n"
                  "the sample and population covariance.\n"
                  "Example: unit-pearson 1,2,3,4 2,4,5,9");
        if(argc == 3){
                RUN(demo(argv[1], argv[2]));
                return EXIT_SUCCESS;
        }
        if(argc != 1){
                fprintf(stderr, "unit-pearson takes two lists X Y (see -h)\n");
                return 2;
        }
        RUN(test_pearson());
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;

}

int test_pearson(void)
{
        double* a = NULL;
        double* b = NULL;
        int n;

        double cor = 0.0;
        n = 10;
        galloc(&a,n);
        galloc(&b,n);

        SECTION_MSG("linear: b = 2a");
        for(int i = 0; i < n;i++){
                a[i] = i+1;
                b[i] = (i+1) * 2;
        }

        RUN(tld_pearson_corr(a, b, n, &cor));
        KV_MSG("corr", "%f", cor);

        SECTION_MSG("b = sin(a)");
        for(int i = 0; i < n;i++){
                a[i] = i+1;
                b[i] = sin( (double)(i+1));
        }
        RUN(tld_pearson_corr(a, b, n, &cor));
        KV_MSG("corr", "%f", cor);

        SECTION_MSG("constant vectors");
        for(int i = 0; i < n;i++){
                a[i] = 42.0;
                b[i] = 42.0;
        }
        RUN(tld_pearson_corr(a, b, n, &cor));
        KV_MSG("corr", "%f", cor);
        OK_MSG("three correlations computed");
        gfree(a);
        gfree(b);
        return OK;
ERROR:
        return FAIL;

}

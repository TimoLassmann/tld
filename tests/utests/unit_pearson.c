#include "tld.h"



int test_pearson(void);

int main(void)
{
        test_pearson();
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

        for(int i = 0; i < n;i++){
                a[i] = i+1;
                b[i] = (i+1) * 2;
        }

        RUN(tld_pearson_corr(a, b, n, &cor));
        LOG_MSG("%*s: %f", 10,"Corr", cor);

        for(int i = 0; i < n;i++){
                a[i] = i+1;
                b[i] = sin( (double)(i+1));
        }
        RUN(tld_pearson_corr(a, b, n, &cor));
        LOG_MSG("%*s: %f", 10,"Corr", cor);


        for(int i = 0; i < n;i++){
                a[i] = 42.0;
                b[i] = 42.0;
        }
        RUN(tld_pearson_corr(a, b, n, &cor));
        LOG_MSG("%*s: %f", 10,"Corr", cor);

        return OK;
ERROR:
        return FAIL;

}

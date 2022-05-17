#include "../core/tld-core.h"
#include <math.h>
#define BASIC_IMPORT
#include "basic.h"

int tld_mean(double *vec, int n, double *mean)
{
        ASSERT(vec != NULL, "No vector");
        double m = 0.0;
        for(int i = 0; i < n;i++){
                m += vec[i];
        }
        m /= (double) n;
        /* LOG_MSG("Mean: %f",m); */
        *mean = m;
        return OK;
ERROR:
        return FAIL;
}

int tld_pop_variance(double *vec, int n, double *variance)
{
        ASSERT(vec != NULL, "No vector");
        double m = 0.0;
        double v = 0.0;
        RUN(tld_mean(vec, n, &m));
        v = 0.0;
        for(int i = 0; i < n;i++){
                v += pow(vec[i] - m, 2.0);
        }
        v /= (double) n;
        *variance = v;
        return OK;
ERROR:
        return FAIL;
}

int tld_sample_variance(double *vec, int n, double *variance)
{
        ASSERT(vec != NULL, "No vector");
        double m = 0.0;
        double v = 0.0;
        RUN(tld_mean(vec, n, &m));
        v = 0.0;
        for(int i = 0; i < n;i++){
                v += pow(vec[i] - m, 2.0);
        }
        v /= (double) (n-1);
        *variance = v;
        return OK;
ERROR:
        return FAIL;
}

#include "tld.h"


int test_standardisation(void);

int main(void)
{
        double* vec = NULL;
        double var = 0.0;
        galloc(&vec,10);

        for(int i = 0; i < 10;i++){
                vec[i] = (double) (i+1) ;
        }

        tld_sample_variance(vec, 10, &var);
        for(int i = 0; i < 10;i++){
                LOG_MSG("%d %f",i, vec[i]);
        }
        LOG_MSG("Var: %f", var);
        gfree(vec);


        test_standardisation();
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
        LOG_MSG("After:");
        for(int i = 0; i < size;i++){
                LOG_MSG("%d %f",i, data[i]);
        }
        LOG_MSG("%*s: %f", 10,"Mean", mean);
        LOG_MSG("%*s: %f", 10,"Stdev",stdev);
        RUN(tld_standardise(data, size, &mean, &stdev));
        LOG_MSG("After2:");
        for(int i = 0; i < size;i++){
                LOG_MSG("%d %f",i, data[i]);
        }
        LOG_MSG("%*s: %f", 10,"Mean", mean);
        LOG_MSG("%*s: %f", 10,"Stdev",stdev);
        return OK;
ERROR:
        return FAIL;

}

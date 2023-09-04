#include "tld.h"



int test_standardisation(void);
int test_covariance(void);

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
        test_covariance();
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


int test_covariance(void)
{

        struct rng_state* rng = NULL;


        double* v1 = NULL;
        double* v2 = NULL;
        double cov;
        int n = 10;




        RUN(init_rng(&rng, 43));
        galloc(&v1, n);
        galloc(&v2, n);
        for(int i = 0; i < n;i++){
                v1[i] = tl_random_double(rng);
                v2[i] = tl_random_double(rng);
        }

        tld_pop_covariance(v1, v2, n, &cov);


        LOG_MSG("%f: covariance pop",cov);
        tld_sample_covariance(v1, v2, n, &cov);
        LOG_MSG("%f: covariance sample",cov);

        fprintf(stdout,"vec_a <- c(");
        for(int i = 0; i < n;i++){
                if(i){
                        fprintf(stdout,",%.17g",v1[i]);
                }else{
                        fprintf(stdout,"%.17g",v1[i]);
                }
        }
        fprintf(stdout,")\n");

        fprintf(stdout,"vec_b <- c(");
        for(int i = 0; i < n;i++){
                if(i){
                        fprintf(stdout,",%.17g",v2[i]);
                }else{
                        fprintf(stdout,"%.17g",v2[i]);
                }
        }
        fprintf(stdout,")\n");

        fprintf(stdout,"cov(x = vec_a, y = vec_b)\n");


        gfree(v2);
        gfree(v1);

        free_rng(rng);


        return OK;
ERROR:

        if(rng){
                free_rng(rng);
        }
        if(v2){
                gfree(v2);
        }
        if(v1){
                gfree(v1);
        }
        return FAIL;
}

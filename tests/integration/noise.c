/* int-noise: Gaussian noise scaled to the variance of uniform data (rng + stats). */
#include "rng/tld-rng.h"
#include "tld.h"
#include "test.h"
#include <math.h>

/* n uniform draws + 10, then noise with half the variance, then with a quarter of the
 * noise's variance. seed 0 picks a seed from the clock. */
static int run(uint64_t seed, int n, int print)
{
        struct tld_rng* rng = NULL;
        double* vec = NULL;
        double* noise = NULL;
        double var = 0.0;
        double mu = 0.0;

        RUN(tld_rng_alloc(&rng, seed));

        galloc(&vec,n);
        galloc(&noise,n);

        SECTION_MSG("noise on %d uniform draws", n);
        KV_MSG("seed", "%llu", (unsigned long long) seed);
        for(int i = 0; i < n;i++){
                vec[i] = tld_rng_double(rng)+10.0;
        }

        RUN(tld_sample_variance(vec, n, &var));
        KV_MSG("data", "variance %f", var);
        if(print){
                fprintf(stdout, "data variance:    %f\n", var);
        }

        mu = sqrt(var*0.5);
        for(int i = 0; i < n;i++){
                noise[i] = tld_rng_gaussian(rng, 0.0 , mu);
        }
        RUN(tld_sample_variance(noise, n, &var));
        KV_MSG("noise", "variance %f (sd %f)", var, mu);
        if(print){
                fprintf(stdout, "noise variance:   %f\n", var);
        }

        mu = sqrt(var);
        for(int i = 0; i < n;i++){
                noise[i] = tld_rng_gaussian(rng, 0.0 ,0.5 *  mu);
        }
        RUN(tld_sample_variance(noise, n, &var));
        KV_MSG("noise/2", "variance %f (sd %f)", var, 0.5 * mu);
        if(print){
                fprintf(stdout, "noise/2 variance: %f\n", var);
        }
        OK_MSG("done");
        tld_rng_free(rng);
        gfree(noise);
        gfree(vec);
        return OK;
ERROR:
        if(rng){
                tld_rng_free(rng);
        }
        if(noise){
                gfree(noise);
        }
        if(vec){
                gfree(vec);
        }
        return FAIL;
}

int main(int argc, char *argv[])
{
        uint64_t seed = 0;
        int n = 1000;
        test_init(&argc, argv, "[SEED [N]]",
                  "Draws N (default 1000) uniform numbers + 10, then Gaussian noise with half\n"
                  "their variance, then noise with a quarter of that; reports the sample\n"
                  "variances. With no arguments the seed comes from the clock and nothing is\n"
                  "printed; with SEED (0 = clock) the variances go to stdout.\n"
                  "Example: int-noise 42 100000");
        if(argc > 3){
                fprintf(stderr, "int-noise: too many arguments (see -h)\n");
                return 2;
        }
        if(argc > 1){
                seed = strtoull(argv[1], NULL, 10);
        }
        if(argc > 2){
                n = atoi(argv[2]);
                if(n < 2){
                        ERROR_MSG("N must be at least 2, got %s", argv[2]);
                }
        }
        RUN(run(seed, n, argc > 1));
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

#include "tld.h"
#include "test.h"
#include <stdio.h>

int rng_dist_test(void);
static void printHistogram(double* values, int n);

/* unit-rng SEED N [KIND]: N draws from a generator seeded with SEED. */
static int demo(int argc, char **argv)
{
        struct tld_rng *rng = NULL;
        char *end = NULL;
        const char *kind = argc > 3 ? argv[3] : "double";
        uint64_t seed = strtoull(argv[1], &end, 10);
        ASSERT(end != argv[1] && *end == 0, "SEED must be a number: %s", argv[1]);
        long n = strtol(argv[2], &end, 10);
        ASSERT(end != argv[2] && *end == 0 && n >= 0, "N must be a non-negative number: %s", argv[2]);
        int k = 0;
        if(strncmp(kind, "int:", 4) == 0){
                k = atoi(kind + 4);
                ASSERT(k > 0, "int:K needs K > 0: %s", kind);
        } else {
                ASSERT(strcmp(kind, "double") == 0 || strcmp(kind, "gaussian") == 0 ||
                       strcmp(kind, "gamma") == 0, "unknown KIND %s (see -h)", kind);
        }
        SECTION_MSG("%ld draws", n);
        KV_MSG("seed", "%llu", (unsigned long long)seed);
        KV_MSG("kind", "%s", kind);
        RUN(tld_rng_alloc(&rng, seed));
        for(long i = 0; i < n; i++){
                if(k){
                        fprintf(stdout, "%d\n", tld_rng_int(rng, k));
                } else if(strcmp(kind, "double") == 0){
                        fprintf(stdout, "%f\n", tld_rng_double(rng));
                } else if(strcmp(kind, "gaussian") == 0){
                        fprintf(stdout, "%f\n", tld_rng_gaussian(rng, 0.0, 1.0));
                } else {
                        fprintf(stdout, "%f\n", tld_rng_gamma(rng, 2.0, 1.0));
                }
        }
        OK_MSG("done");
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

int main(int argc, char *argv[])
{
        struct tld_rng* rng = NULL;
        double* values = NULL;
        double mean = 0.0;
        double stdev = 1.0;

        int num_val = 1000000;
        int i,j;

        test_init(&argc, argv, "[SEED N [KIND]]",
                  "With no arguments: a histogram of 1e6 gaussian draws (shown with -vv),\n"
                  "normalised gamma draws, ten doubles, and 1e7 samples from tl_random_sdist\n"
                  "against their weights.\n"
                  "With SEED and N: prints N draws, one per line. KIND is double (default,\n"
                  "uniform in [0,1)), gaussian (mean 0, sd 1), gamma (shape 2, scale 1) or\n"
                  "int:K (uniform integers 0..K-1).\n"
                  "Example: unit-rng 42 5 int:6");
        if(argc == 3 || argc == 4){
                RUN(demo(argc, argv));
                return EXIT_SUCCESS;
        }
        if(argc != 1){
                fprintf(stderr, "unit-rng takes SEED N [KIND] (see -h)\n");
                return 2;
        }

        RUN(tld_rng_alloc(&rng,0));

        MMALLOC(values,sizeof(double) * num_val);

        SECTION_MSG("Gaussian");
        for(i = 0; i < num_val;i++){
                values[i] = tld_rng_gaussian(rng,mean,stdev);
                if((i + 1) % 10000 == 0){
                        tld_progress(i + 1, num_val, "gaussian");
                }
        }

        printHistogram(values,num_val);
        OK_MSG("%d gaussian draws", num_val);
        SECTION_MSG("Gamma");

        for(i = 0; i < 100;i+=5){
                mean = 0.0;
                for(j = 0; j < 10;j++){
                        values[j] = tld_rng_gamma(rng, (double) i + 0.01,1.0);
                        mean+=values[j];
                }
                for(j = 0; j < 10;j++){
                        values[j] /= mean;
                }
                char line[128];
                int o = 0;
                for(j = 0; j < 10;j++){
                        o += snprintf(line + o, sizeof line - o, "%0.2f ", values[j]);
                }
                DEBUG_MSG("shape %5.2f: %s", (double) i + 0.01, line);
        }
        OK_MSG("gamma draws normalised");

        MFREE(values);

        SECTION_MSG("Double");
        for(int i = 0; i < 10;i++){
                double r = tld_rng_double( rng);
                DEBUG_MSG("%d %f",i, r);
        }
        tld_rng_free(rng);

        SECTION_MSG("Test rngdist");
        RUN(rng_dist_test());
        OK_MSG("sampled from the weighted distribution");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int rng_dist_test(void)
{
        double* w = NULL;
        int* out = NULL;
        struct tld_rng_dist* d = NULL;
        double sum;
        galloc(&w, 6);
        galloc(&out,6);
        for(int i = 0; i < 6;i++){
                w[i] = 1.0;
                out[i] = 0;
        }
        w[2] = 2.0;

        sum = 0.0;
        for(int i = 0; i < 6;i++){
                sum += w[i];
        }
        RUN(tld_rng_dist_alloc(&d, w, 6, 0));
        for(int i = 0; i < 10000000;i++){
                out[tld_rng_dist_sample(d)]++;
                if((i + 1) % 100000 == 0){
                        tld_progress(i + 1, 10000000, "sampling");
                }
        }

        for(int i = 0; i < 6;i++){
                char key[16];
                snprintf(key, sizeof key, "bin %d", i);
                KV_MSG(key, "observed %f expected %f", (double) out[i] /  10000000.0, w[i]/sum);
        }
        tld_rng_dist_free(d);
        gfree(w);
        gfree(out);
        return OK;
ERROR:
        gfree(w);
        gfree(out);
        return FAIL;
}

/* from RosettaCode */
// https://rosettacode.org/wiki/Statistics/Normal_distribution#C

void printHistogram(double* values, int n)
{
        const int width = 50;
        int max = 0;

        const double low   = -3.05;
        const double high  =  3.05;
        const double delta =  0.1;

        int i,j,k;
        int nbins = (int)((high - low) / delta);
        int* bins = (int*)calloc(nbins,sizeof(int));
        if ( bins != NULL )
        {
                for ( i = 0; i < n; i++ )
                {
                        int j = (int)( (values[i] - low) / delta );
                        if ( 0 <= j  &&  j < nbins )
                                bins[j]++;
                }

                for ( j = 0; j < nbins; j++ )
                        if ( max < bins[j] )
                                max = bins[j];

                for ( j = 0; j < nbins; j++ )
                {
                        char bar[64];
                        k = (int)( (double)width * (double)bins[j] / (double)max );
                        bar[k] = 0;
                        while(k-- > 0) bar[k] = '*';
                        DEBUG_MSG("(%5.2f, %5.2f) |%s  %-.1f%%", low + j * delta, low + (j + 1) * delta,
                                  bar, bins[j] * 100.0 / (double)n);
                }

                free(bins);
        }
}

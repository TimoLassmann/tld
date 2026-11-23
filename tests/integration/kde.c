/* int-kde: kernel density estimates (tld_kde_pdf, tld_kde_count_pdf) on a mixture of
 * two Gaussians drawn with the rng module. */
#include "tld.h"
#include "test.h"

static int checks(void)
{
        struct tld_rng* rng = NULL;
        double* x = NULL;
        double r = 0.0;
        int len = 10000;

        RUN(tld_rng_alloc(&rng,0));
        galloc(&x, len);

        SECTION_MSG("tld_kde_pdf on %d draws from 0.3 N(1, 0.2) + 0.7 N(5, 1)", len);
        for(int i = 0;i < len;i++){
                r = tld_rng_double(rng);
                if(r < 0.3){
                        x[i] = tld_rng_gaussian(rng, 1.0, 0.2);
                }else{
                        x[i] = tld_rng_gaussian(rng, 5.0, 1.0);
                }

        }

        double** ladder = NULL;
        RUN(tld_kde_pdf(x, len, 20, &ladder));

        for(int i = 0; i < 20;i++){
                DEBUG_MSG("%2d x %f density %f", i, ladder[i][0], ladder[i][1]);
        }
        OK_MSG("density at 20 points");

        gfree(ladder);
        gfree(x);
        x = NULL;

        double* y = NULL;
        len = 10;
        galloc(&x,len);
        galloc(&y,len);
        for(int i = 0;i < len;i++){
                x[i] = i;
                y[i] = 0.0;
        }

        SECTION_MSG("tld_kde_count_pdf on counts of 10000 rounded draws in 0..9");
        for(int i = 0;i < 10000;i++){
                r = tld_rng_double(rng);
                int a = 0;
                if(r < 0.3){
                        a = (int) round(tld_rng_gaussian(rng, 1.0, 0.2));

                }else{
                        a = (int) round(tld_rng_gaussian(rng, 5.0, 1.0));
                }
                a = TLD_MAX(0, a);
                a = TLD_MIN(9, a);

                y[a]++;
        }

        ladder = NULL;
        RUN(tld_kde_count_pdf(x,y,len ,&ladder));

        for(int i = 0; i < len;i++){
                DEBUG_MSG("%f count %f -> %f,%f",x[i],y[i], ladder[i][0],ladder[i][1]);
        }
        OK_MSG("density at %d values", len);

        gfree(ladder);

        if(x){
                gfree(x);
        }
        if(y){
                gfree(y);
        }

        tld_rng_free(rng);
        return OK;
ERROR:
        return FAIL;
}

/* int-kde NUMBER ...: density of the user's numbers at 20 points. */
static int demo(int n, char **args)
{
        double *x = NULL;
        double **ladder = NULL;
        int steps = 20;
        int len = 0;
        galloc(&x, n);
        for(int i = 0; i < n; i++){
                char *end = NULL;
                x[len] = strtod(args[i], &end);
                if(end == args[i] || *end){
                        ERROR_MSG("not a number: %s", args[i]);
                }
                len++;
        }
        SECTION_MSG("tld_kde_pdf on %d numbers", len);
        KV_MSG("steps", "%d", steps);
        RUN(tld_kde_pdf(x, len, steps, &ladder));
        fprintf(stdout, "x\tdensity\n");
        for(int i = 0; i < steps; i++){
                fprintf(stdout, "%f\t%f\n", ladder[i][0], ladder[i][1]);
        }
        OK_MSG("done");
        gfree(ladder);
        gfree(x);
        return OK;
ERROR:
        if(x){
                gfree(x);
        }
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[NUMBER ...]",
                  "With no arguments: kernel density estimates of 10000 draws from a mixture\n"
                  "of two Gaussians (tld_kde_pdf) and of their rounded counts (tld_kde_count_pdf).\n"
                  "With NUMBERs: prints the density of those numbers at 20 points (x, density).\n"
                  "Example: int-kde 1 1.2 0.9 5 5.5 4.8 6");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
        }else{
                RUN(checks());
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

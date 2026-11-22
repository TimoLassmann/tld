#include "tld.h"
#include "test.h"
#include <stdio.h>

/* unit-logsum P ...: sum probabilities in scaled log space. */
static int demo(int n, char **args)
{
        float acc = -INFINITY;      /* log(0) */
        double direct = 0.0;
        SECTION_MSG("logsum over %d probabilities", n);
        for(int i = 0; i < n; i++){
                char *end = NULL;
                double p = strtod(args[i], &end);
                if(end == args[i] || *end || p < 0.0){
                        ERROR_MSG("not a probability: %s", args[i]);
                }
                float s = logf((float)p);
                acc = tld_logsum(acc, s);
                direct += p;
                KV_MSG(args[i], "scaled %f", s);
                DEBUG_MSG("running sum %f", expf(acc));
        }
        fprintf(stdout, "logsum:  %f\n", expf(acc));
        fprintf(stdout, "direct:  %f\n", direct);
        OK_MSG("done");
        return OK;
ERROR:
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[P ...]",
                  "With no arguments: adds 0.4 + 0.4 with logsum in scaled log space.\n"
                  "With P ...: converts each probability with logf, sums them with\n"
                  "logsum and prints the result next to the plain sum.\n"
                  "Example: unit-logsum 0.1 0.25 0.3");
        tld_logsum_init();
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }

        float a,b;
        ASSERT(1 != 0,"How odd");

        SECTION_MSG("logsum(0.4, 0.4)");
        a = logf(0.4f);
        b = logf(0.4f);

        KV_MSG("sum", "%f", expf(tld_logsum(a, b)));
        OK_MSG("done");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

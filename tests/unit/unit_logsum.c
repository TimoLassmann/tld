#include "tld.h"
#include "test.h"
#include <stdio.h>

/* unit-logsum P ...: sum probabilities in scaled log space. */
static int demo(int n, char **args)
{
        float acc = prob2scaledprob(0.0f);
        double direct = 0.0;
        SECTION_MSG("logsum over %d probabilities", n);
        for(int i = 0; i < n; i++){
                char *end = NULL;
                double p = strtod(args[i], &end);
                if(end == args[i] || *end || p < 0.0){
                        ERROR_MSG("not a probability: %s", args[i]);
                }
                float s = prob2scaledprob((float)p);
                acc = logsum(acc, s);
                direct += p;
                KV_MSG(args[i], "scaled %f", s);
                DEBUG_MSG("running sum %f", scaledprob2prob(acc));
        }
        fprintf(stdout, "logsum:  %f\n", scaledprob2prob(acc));
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
                  "With P ...: converts each probability with prob2scaledprob, sums them with\n"
                  "logsum and prints the result next to the plain sum.\n"
                  "Example: unit-logsum 0.1 0.25 0.3");
        init_logsum();
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }

        float a,b;
        ASSERT(1 != 0,"How odd");

        SECTION_MSG("logsum(0.4, 0.4)");
        a = prob2scaledprob(0.4f);
        b = prob2scaledprob(0.4f);

        KV_MSG("sum", "%f", scaledprob2prob(logsum(a, b)));
        OK_MSG("done");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

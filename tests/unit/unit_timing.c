#include "tld.h"
#include "test.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
        long n = 10000000;

        test_init(&argc, argv, "[N]",
                  "Times a loop of N additions (default 10000000) with TLD_START_TIMER and\n"
                  "TLD_END_TIMER (the timer prints the time on stdout). With N the sum is\n"
                  "printed too.\n"
                  "Example: unit-timing -v 50000000");
        if(argc > 1){
                char *end = NULL;
                n = strtol(argv[1], &end, 10);
                if(end == argv[1] || *end || n < 0){
                        fprintf(stderr, "N must be a non-negative number: %s\n", argv[1]);
                        return 2;
                }
        }
        SECTION_MSG("sum of i + sqrt(i) for %ld values", n);
        TLD_START_TIMER;

        double x = 0.0;

        for(long i = 0; i < n;i++){
                x+= (double) i + sqrt((double) i );
        }
        KV_MSG("sum", "%f", x);

        TLD_END_TIMER;
        if(argc > 1){
                fprintf(stdout, "%f\n", x);
        }
        OK_MSG("done");

        return EXIT_SUCCESS;
}

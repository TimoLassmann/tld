#include "tld.h"
#include "test.h"
#include <stdio.h>

/* unit-sort X ...: the numbers sorted ascending and descending. */
static int demo(int n, char **args)
{
        double *x = NULL;
        galloc(&x, n);
        for(int i = 0; i < n; i++){
                char *end = NULL;
                x[i] = strtod(args[i], &end);
                ASSERT(end != args[i] && *end == 0, "not a number: %s", args[i]);
        }
        SECTION_MSG("sorting %d number%s", n, n == 1 ? "" : "s");
        RUN(TLD_SORT_ASC(x, n));
        fprintf(stdout, "ascending: ");
        for(int i = 0; i < n; i++){
                fprintf(stdout, " %g", x[i]);
        }
        fprintf(stdout, "\n");
        RUN(TLD_SORT_DESC(x, n));
        fprintf(stdout, "descending:");
        for(int i = 0; i < n; i++){
                fprintf(stdout, " %g", x[i]);
        }
        fprintf(stdout, "\n");
        OK_MSG("done");
        gfree(x);
        return OK;
ERROR:
        gfree(x);
        return FAIL;
}

int main(int argc, char *argv[])
{
        struct rng_state* rng = NULL;
        char line[256];
        int o;

        test_init(&argc, argv, "[X ...]",
                  "With no arguments: TLD_SORT_DESC and TLD_SORT_ASC on 10 random floats and\n"
                  "10 random letters (the arrays are shown with -v).\n"
                  "With numbers: prints them sorted ascending and descending.\n"
                  "Negative numbers need -- first. Example: unit-sort -- 5 -3 9 0.5");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }
        RUN(init_rng(&rng,0));

        float* x_flt = NULL;
        galloc(&x_flt, 10);

        for(int i = 0; i < 10;i++){
                x_flt[i] = (float) tl_random_double(rng);
        }

        SECTION_MSG("floats");
        TLD_SORT_DESC(x_flt,10);
        o = 0;
        for(int i = 0; i < 10;i++){
                o += snprintf(line + o, sizeof line - o, "%f ", x_flt[i]);
        }
        KV_MSG("desc", "%s", line);

        TLD_SORT_ASC(x_flt,10);
        o = 0;
        for(int i = 0; i < 10;i++){
                o += snprintf(line + o, sizeof line - o, "%f ", x_flt[i]);
        }
        KV_MSG("asc", "%s", line);

        gfree(x_flt);

        char* x_char = NULL;
        galloc(&x_char, 10);

        for(int i = 0; i < 10;i++){
                x_char[i] = (char) tl_random_int(rng, 26) + 65;
        }

        SECTION_MSG("chars");
        TLD_SORT_DESC(x_char,10);
        KV_MSG("desc", "%.10s", x_char);

        TLD_SORT_ASC(x_char,10);
        KV_MSG("asc", "%.10s", x_char);

        gfree(x_char);


        free_rng(rng);
        OK_MSG("sorted");

        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

#include "tld.h"
#include "test.h"
#include <stdio.h>

/* unit-shuffle SEED ITEM ...: the items in TLD_SHUFFLE order. */
static int demo(int n, char **args)
{
        struct rng_state *rng = NULL;
        char *end = NULL;
        uint64_t seed = strtoull(args[0], &end, 10);
        int32_t *idx = NULL;
        ASSERT(end != args[0] && *end == 0, "seed must be a number: %s", args[0]);
        n--;
        args++;
        RUN(init_rng(&rng, seed));
        galloc(&idx, n);
        for(int i = 0; i < n; i++){
                idx[i] = i;
        }
        SECTION_MSG("shuffle %d items with seed %llu", n, (unsigned long long)seed);
        RUN(TLD_SHUFFLE(idx, n, rng));
        for(int i = 0; i < n; i++){
                fprintf(stdout, "%s%s", i ? " " : "", args[idx[i]]);
        }
        fprintf(stdout, "\n");
        OK_MSG("done");
        gfree(idx);
        free_rng(rng);
        return OK;
ERROR:
        gfree(idx);
        if(rng){
                free_rng(rng);
        }
        return FAIL;
}

int main(int argc, char *argv[])
{
        struct rng_state* rng = NULL;
        int len = 15;
        double* d_arr = NULL;
        uint8_t* small_arr = NULL;

        test_init(&argc, argv, "[SEED ITEM ...]",
                  "With no arguments: shuffles 15 random doubles ten times with TLD_SHUFFLE.\n"
                  "With SEED and ITEMs: prints the items in shuffled order (same SEED, same order).\n"
                  "Example: unit-shuffle 42 ace king queen jack ten");
        if(argc > 2){
                RUN(demo(argc - 1, argv + 1));
                return EXIT_SUCCESS;
        }
        if(argc == 2){
                fprintf(stderr, "unit-shuffle needs a SEED and at least one ITEM (see -h)\n");
                return 2;
        }
        RUN(init_rng(&rng, 42));

        galloc(&d_arr,len);
        galloc(&small_arr,len);
        for(int i = 0;i < len;i++){
                d_arr[i] = tl_random_double(rng)*10.0;
        }

        SECTION_MSG("10 rounds of TLD_SHUFFLE on %d doubles", len);
        for(int i = 0;i < 10;i++){
                char line[1024];
                int o = 0;
                for(int j = 0; j < len;j++){
                        o += snprintf(line + o, sizeof line - o, "%f ", d_arr[j]);
                }
                DEBUG_MSG("ORG    : %s", line);

                TLD_SHUFFLE(d_arr,len, rng);
                o = 0;
                for(int j = 0; j < len;j++){
                        o += snprintf(line + o, sizeof line - o, "%f ", d_arr[j]);
                }
                DEBUG_MSG("SHUFFLE: %s", line);
        }
        OK_MSG("10 shuffles");

        gfree(d_arr);
        gfree(small_arr);

        free_rng(rng);
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

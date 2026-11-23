/* Regression test for the shuffle bias. The old step
 *     r = tld_rng_int(rng, n); j = i + r % (n - i);
 * is not uniform when (n - i) does not divide n. For n = 3 the second step
 * picks one of two positions with probability 2/3, so two of the six
 * permutations come up twice as often as two others. A Fisher-Yates shuffle
 * gives each permutation 1/6. */
#include "tld.h"
#include "test.h"
#include <string.h>

static int perm_index(const int *a)
{
        /* the six permutations of {0,1,2} */
        static const int p[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
        for (int k = 0; k < 6; k++) {
                if (a[0] == p[k][0] && a[1] == p[k][1] && a[2] == p[k][2]) {
                        return k;
                }
        }
        return -1;
}

static int test_uniform(void)
{
        struct tld_rng *rng = NULL;
        const int n_trials = 120000;
        const double expect = n_trials / 6.0;
        int count[6] = {0};
        double chi2 = 0.0;

        RUN(tld_rng_alloc(&rng, 42));
        for (int t = 0; t < n_trials; t++) {
                int a[3] = {0, 1, 2};
                RUN(tld_shuffle_int32_t(a, 3, rng));
                int k = perm_index(a);
                ASSERT(k >= 0, "shuffle lost an element");
                count[k]++;
                if((t + 1) % 1000 == 0){
                        tld_progress(t + 1, n_trials, "shuffles");
                }
        }
        for (int k = 0; k < 6; k++) {
                double d = count[k] - expect;
                chi2 += d * d / expect;
                DEBUG_MSG("permutation %d: %d (expected %.0f)", k, count[k], expect);
        }
        KV_MSG("chi2", "%.2f (limit 30, 5 df)", chi2);
        /* 5 degrees of freedom: P(chi2 > 30) is about 1.5e-5. The biased
           shuffle gives chi2 in the thousands here. */
        ASSERT(chi2 < 30.0, "permutations not uniform: chi2 = %f", chi2);
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

static int test_lengths(void)
{
        struct tld_rng *rng = NULL;
        RUN(tld_rng_alloc(&rng, 42));
        /* other lengths still permute */
        int b[10];
        for (int i = 0; i < 10; i++) {
                b[i] = i;
        }
        RUN(TLD_SHUFFLE(b, 10, rng));
        int seen = 0;
        for (int i = 0; i < 10; i++) {
                seen |= 1 << b[i];
        }
        ASSERT(seen == 0x3FF, "shuffle lost an element");
        RUN(TLD_SHUFFLE(b, 1, rng));
        RUN(TLD_SHUFFLE(b, 0, rng));

        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"uniform", test_uniform, "120000 shuffles of {0,1,2}: chi2 over the 6 permutations"},
        {"lengths", test_lengths, "shuffles of length 10, 1 and 0 keep every element"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression check for the shuffle bias: all six permutations of three elements\n"
                  "come up equally often (Fisher-Yates).\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  uniform  120000 shuffles of {0,1,2}: chi2 over the 6 permutations\n"
                  "  lengths  shuffles of length 10, 1 and 0 keep every element\n"
                  "\n"
                  "Example: regress-shuffle uniform");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-shuffle: no check named '%s' (see -h)\n", argv[i]);
                        return 2;
                }
        }
        for(int k = 0; k < n_checks; k++){
                int want = argc == 1;
                for(int i = 1; i < argc; i++){
                        want |= strcmp(argv[i], checks[k].name) == 0;
                }
                if(!want){
                        continue;
                }
                SECTION_MSG("%s: %s", checks[k].name, checks[k].about);
                RUN(checks[k].fn());
                OK_MSG("%s passed", checks[k].name);
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

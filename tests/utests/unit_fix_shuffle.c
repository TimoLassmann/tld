/* Regression test for the shuffle bias. The old step
 *     r = tl_random_int(rng, n); j = i + r % (n - i);
 * is not uniform when (n - i) does not divide n. For n = 3 the second step
 * picks one of two positions with probability 2/3, so two of the six
 * permutations come up twice as often as two others. A Fisher-Yates shuffle
 * gives each permutation 1/6. */
#include "tld.h"

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

int main(void)
{
        struct rng_state *rng = NULL;
        const int n_trials = 120000;
        const double expect = n_trials / 6.0;
        int count[6] = {0};
        double chi2 = 0.0;

        RUN(init_rng(&rng, 42));
        for (int t = 0; t < n_trials; t++) {
                int a[3] = {0, 1, 2};
                RUN(tld_shuffle_int32_t(a, 3, rng));
                int k = perm_index(a);
                ASSERT(k >= 0, "shuffle lost an element");
                count[k]++;
        }
        for (int k = 0; k < 6; k++) {
                double d = count[k] - expect;
                chi2 += d * d / expect;
                LOG_MSG("permutation %d: %d (expected %.0f)", k, count[k], expect);
        }
        /* 5 degrees of freedom: P(chi2 > 30) is about 1.5e-5. The biased
           shuffle gives chi2 in the thousands here. */
        ASSERT(chi2 < 30.0, "permutations not uniform: chi2 = %f", chi2);

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

        free_rng(rng);
        return EXIT_SUCCESS;
ERROR:
        free_rng(rng);
        return EXIT_FAILURE;
}

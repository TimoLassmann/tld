/* Regression tests for the logsum fixes.
 * - nearly_equal_float/double divided by MIN(|a|+|b|, FLT_MIN/DBL_MIN), which
 *   is FLT_MIN for any normal input, so they behaved like ==.
 * - logsum() used an all-zero table unless init_logsum() had been called
 *   (this test never calls it before the first logsum), and the init was not
 *   thread-safe.
 * - NaN input reached the (int) table index (UB; UBSan float-cast-overflow).
 */
#include "tld.h"
#include <math.h>
#include <float.h>
#include <pthread.h>

static int test_nearly_equal(void)
{
        float f1 = 1.0F;
        float f2 = nextafterf(1.0F, 2.0F);
        double d1 = 1.0;
        double d2 = nextafter(1.0, 2.0);

        ASSERT(nearly_equal_float(f1, f2) == 1, "adjacent floats not nearly equal");
        ASSERT(nearly_equal_double(d1, d2) == 1, "adjacent doubles not nearly equal");
        ASSERT(nearly_equal_float(1000.0F, nextafterf(1000.0F, 0.0F)) == 1, "adjacent floats not nearly equal");
        ASSERT(nearly_equal_double(-1e10, nextafter(-1e10, 0.0)) == 1, "adjacent doubles not nearly equal");
        ASSERT(TLSAFE_EQ(f1, f2) == 1, "TLSAFE_EQ float");
        ASSERT(TLSAFE_EQ(d1, d2) == 1, "TLSAFE_EQ double");

        ASSERT(nearly_equal_float(1.0F, 1.001F) == 0, "1 and 1.001 nearly equal");
        ASSERT(nearly_equal_double(1.0, 1.0 + 1e-9) == 0, "1 and 1+1e-9 nearly equal");
        ASSERT(nearly_equal_float(1.0F, -1.0F) == 0, "1 and -1 nearly equal");
        ASSERT(nearly_equal_float(FLT_MAX, -FLT_MAX) == 0, "FLT_MAX and -FLT_MAX nearly equal");
        ASSERT(nearly_equal_double(0.0, 1e-300) == 0, "0 and 1e-300 nearly equal");
        ASSERT(nearly_equal_float(0.0F, 0.0F) == 1, "0 and 0");
        ASSERT(nearly_equal_float(-INFINITY, -INFINITY) == 1, "-inf and -inf");
        ASSERT(nearly_equal_float(-INFINITY, 1.0F) == 0, "-inf and 1");
        ASSERT(nearly_equal_float(NAN, NAN) == 0, "NaN and NaN");
        return OK;
ERROR:
        return FAIL;
}

static int test_logsum_values(void)
{
        /* deliberately no init_logsum() before this point */
        float h = prob2scaledprob(0.5F);
        float r = logsum(h, h);
        ASSERT(fabsf(r) < 1e-3F, "logsum(log .5, log .5) = %f, expected 0", (double)r);

        r = logsum(prob2scaledprob(0.25F), prob2scaledprob(0.5F));
        ASSERT(fabsf(scaledprob2prob(r) - 0.75F) < 1e-3F, "0.25 + 0.5 = %f", (double)scaledprob2prob(r));

        init_logsum();
        init_logsum();

        ASSERT(logsum(-INFINITY, h) == h, "-inf identity");
        ASSERT(logsum(h, -INFINITY) == h, "-inf identity");
        ASSERT(logsum(INFINITY, INFINITY) == INFINITY, "+inf + +inf");
        ASSERT(logsum(INFINITY, 1.0F) == INFINITY, "+inf + 1");
        ASSERT(isnan(logsum(NAN, 1.0F)), "NaN not propagated");
        ASSERT(isnan(logsum(1.0F, NAN)), "NaN not propagated");
        ASSERT(isnan(logsum(NAN, NAN)), "NaN not propagated");
        return OK;
ERROR:
        return FAIL;
}

static void *thread_fn(void *arg)
{
        int *ok = arg;
        float h = prob2scaledprob(0.5F);
        *ok = 1;
        for (int i = 0; i < 1000; i++) {
                init_logsum();
                if (fabsf(logsum(h, h)) > 1e-3F) {
                        *ok = 0;
                }
        }
        return NULL;
}

static int test_threads(void)
{
        pthread_t t[8];
        int ok[8] = {0};
        for (int i = 0; i < 8; i++) {
                ASSERT(pthread_create(&t[i], NULL, thread_fn, &ok[i]) == 0, "pthread_create");
        }
        for (int i = 0; i < 8; i++) {
                pthread_join(t[i], NULL);
                ASSERT(ok[i] == 1, "thread %d saw a wrong logsum", i);
        }
        return OK;
ERROR:
        return FAIL;
}

int main(void)
{
        RUN(test_logsum_values());
        RUN(test_nearly_equal());
        RUN(test_threads());
        LOG_MSG("unit_fix_logsum: all passed");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

/* Regression tests for the logsum fixes.
 * - nearly_equal_float/double divided by MIN(|a|+|b|, FLT_MIN/DBL_MIN), which
 *   is FLT_MIN for any normal input, so they behaved like ==.
 * - logsum() used an all-zero table unless init_logsum() had been called
 *   (this test never calls it before the first logsum), and the init was not
 *   thread-safe.
 * - NaN input reached the (int) table index (UB; UBSan float-cast-overflow).
 */
#include "tld.h"
#include "test.h"
#include <string.h>
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
        float h = logf(0.5F);
        float r = logsum(h, h);
        ASSERT(fabsf(r) < 1e-3F, "logsum(log .5, log .5) = %f, expected 0", (double)r);

        r = logsum(logf(0.25F), logf(0.5F));
        ASSERT(fabsf(expf(r) - 0.75F) < 1e-3F, "0.25 + 0.5 = %f", (double)expf(r));

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
        float h = logf(0.5F);
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

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"values", test_logsum_values, "logsum values without init_logsum, NaN input"},
        {"nearly-equal", test_nearly_equal, "nearly_equal_float/double use a relative tolerance"},
        {"threads", test_threads, "8 threads call logsum at once"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the logsum fixes. A plain run calls logsum() before\n"
                  "init_logsum() on purpose (values first).\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  values        logsum values without init_logsum, NaN input\n"
                  "  nearly-equal  nearly_equal_float/double use a relative tolerance\n"
                  "  threads       8 threads call logsum at once\n"
                  "\n"
                  "Example: regress-logsum nearly-equal");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-logsum: no check named '%s' (see -h)\n", argv[i]);
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

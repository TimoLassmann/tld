/* Regression tests for the core / alloc macro fixes.
 *
 * - RUN, ASSERT, MMALLOC, MMALLOC_ALIGNED and MFREE are single statements
 *   (do { } while (0)), so they work as the body of an unbraced if/else.
 *   Before the fix this file did not compile.
 * - MFREE(NULL) is silent (free(NULL) is legal) and leaves the pointer NULL.
 * - 2D galloc: the row loop no longer captures a caller variable named i
 *   (before: rows 1.. were never set, writing m[1][0] crashed), the size is
 *   computed in size_t and checked, and a failed allocation is reported
 *   instead of dereferencing NULL.
 * - Library log messages go to stderr, not stdout.
 * - The library no longer exports a global error(): defining one here used to
 *   be a duplicate-symbol link error.
 */
#include "tld.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(__SANITIZE_ADDRESS__)
#define TLD_FIX_HAVE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define TLD_FIX_HAVE_ASAN 1
#endif
#endif
#ifndef TLD_FIX_HAVE_ASAN
#define TLD_FIX_HAVE_ASAN 0
#endif

/* Large allocations must fail fast (NULL) instead of aborting under ASan. */
const char *__asan_default_options(void);
const char *__asan_default_options(void)
{
        return "allocator_may_return_null=1:max_allocation_size_mb=2048";
}

/* Must not clash with the library (it used to define a global error()). */
int error(const char *format, ...);
int error(const char *format, ...)
{
        (void)format;
        return 0;
}

static int returns(int v)
{
        return v;
}

static int test_statement_macros(int flag)
{
        char *p = NULL;
        char *q = NULL;

        if (flag)
                RUN(returns(OK));
        else
                RUN(returns(OK));

        if (flag)
                ASSERT(flag == 1, "flag is %d", flag);
        else
                ASSERT(flag == 0, "flag is %d", flag);

        if (flag)
                MMALLOC(p, 16);
        else
                MMALLOC(q, 16);

        if (flag)
                MFREE(p);
        else
                MFREE(q);

        if (flag)
                MMALLOC_ALIGNED(p, 100, 64);
        else
                MMALLOC_ALIGNED(q, 100, 64);

        if (((uintptr_t)(flag ? p : q)) % 64 != 0) {
                ERROR_MSG("MMALLOC_ALIGNED returned a misaligned block");
        }
        if (flag)
                MFREE(p);
        else
                MFREE(q);

        if (p != NULL || q != NULL) {
                ERROR_MSG("MFREE did not reset the pointer");
        }
        return OK;
ERROR:
        free(p);
        free(q);
        return FAIL;
}

/* Run fn with stdout and stderr redirected to temp files; return how many
 * bytes landed on each. */
static int capture(void (*fn)(void), long *n_out, long *n_err)
{
        char out_name[] = "/tmp/tld_fix_out_XXXXXX";
        char err_name[] = "/tmp/tld_fix_err_XXXXXX";
        int fo = mkstemp(out_name);
        int fe = mkstemp(err_name);
        int save_out = dup(STDOUT_FILENO);
        int save_err = dup(STDERR_FILENO);

        ASSERT(fo >= 0 && fe >= 0 && save_out >= 0 && save_err >= 0, "could not set up capture");
        fflush(stdout);
        fflush(stderr);
        dup2(fo, STDOUT_FILENO);
        dup2(fe, STDERR_FILENO);

        fn();

        fflush(stdout);
        fflush(stderr);
        dup2(save_out, STDOUT_FILENO);
        dup2(save_err, STDERR_FILENO);
        close(save_out);
        close(save_err);

        *n_out = lseek(fo, 0, SEEK_END);
        *n_err = lseek(fe, 0, SEEK_END);
        close(fo);
        close(fe);
        unlink(out_name);
        unlink(err_name);
        return OK;
ERROR:
        return FAIL;
}

static void emit_messages(void)
{
        LOG_MSG("a log message");
        WARNING_MSG("a warning");
        tld_message(MESSAGE_TYPE_ERROR, "an error");
}

static void free_null(void)
{
        char *p = NULL;
        MFREE(p);
        MFREE(p);
}

static int test_messages_on_stderr(void)
{
        long n_out = -1;
        long n_err = -1;
        RUN(capture(emit_messages, &n_out, &n_err));
        ASSERT(n_out == 0, "library messages wrote %ld bytes to stdout", n_out);
        ASSERT(n_err > 0, "library messages wrote nothing to stderr");

        RUN(capture(free_null, &n_out, &n_err));
        ASSERT(n_out == 0 && n_err == 0, "MFREE(NULL) printed something (%ld/%ld bytes)", n_out, n_err);
        return OK;
ERROR:
        return FAIL;
}

static int galloc_2d(double ***m, int i, int j)
{
        /* the row-count argument is literally the identifier i */
        galloc(m, i, j);
        return OK;
ERROR:
        return FAIL;
}

static int galloc_2d_char(char ***m, int a, int b)
{
        galloc(m, a, b);
        return OK;
ERROR:
        return FAIL;
}

static int test_galloc_2d(void)
{
        double **m = NULL;
        char **c = NULL;
        int i = 7;
        int j = 5;
        uint32_t d1 = 0;
        uint32_t d2 = 0;

        RUN(galloc_2d(&m, i, j));
        RUN(get_dim1(m, &d1));
        RUN(get_dim2(m, &d2));
        ASSERT(d1 == 7 && d2 == 5, "dims are %u x %u", d1, d2);
        for (int r = 0; r < i; r++) {
                ASSERT(m[r] == m[0] + (size_t)r * j, "row %d points to the wrong place", r);
                for (int k = 0; k < j; k++) {
                        m[r][k] = r * 100 + k;
                }
        }
        ASSERT(m[6][4] == 604.0, "wrong value");
        gfree(m);
        m = NULL;

        /* dim1 < 1: tld__galloc returns NULL; the macro must fail, not crash */
        ASSERT(galloc_2d(&m, 0, 5) == FAIL, "2D galloc with 0 rows succeeded");
        ASSERT(m == NULL, "m was set on failure");

        /* byte count does not fit in size_t: rejected before allocating anything */
        ASSERT(galloc_2d_char(&c, (1 << 30) + 1, 2147483647) == FAIL, "overflowing 2D galloc succeeded");
        ASSERT(c == NULL, "c was set on failure");

#if TLD_FIX_HAVE_ASAN
        /* dim1 * dim2 overflows int (2^32; UB before the fix) but not size_t; the
           data block is above max_allocation_size_mb, so the data allocation fails
           and the call must fail cleanly. Only under ASan: elsewhere the system
           might really hand out 32 GB. */
        ASSERT(galloc_2d_char(&c, 65536, 65536) == FAIL, "huge 2D galloc succeeded");
        ASSERT(c == NULL, "c was set on failure");
#endif

        /* gfree of a NULL pointer is a no-op, like free(NULL); gfree clears its
           argument, so a second gfree of the same pointer is harmless too */
        {
                double *z = NULL;
                gfree(z);
                galloc(&z, 8);
                gfree(z);
                ASSERT(z == NULL, "gfree did not clear the pointer");
                gfree(z);
        }
        return OK;
ERROR:
        if (m) {
                gfree(m);
        }
        return FAIL;
}

static int test_macros(void)
{
        RUN(test_statement_macros(1));
        RUN(test_statement_macros(0));
        return OK;
ERROR:
        return FAIL;
}

/* Library messages must print here, so run at TLD_VERBOSE=1 whatever -v says. */
static int test_messages(void)
{
        char level[8];
        int r;
        snprintf(level, sizeof level, "%d", tld_verbosity());
        setenv("TLD_VERBOSE", "1", 1);
        r = test_messages_on_stderr();
        setenv("TLD_VERBOSE", level, 1);
        return r;
}

static int test_error_symbol(void)
{
        ASSERT(error("x") == 0, "wrong error()");
        return OK;
ERROR:
        return FAIL;
}


/* MACRO_MIN / MACRO_MAX evaluate each argument once (they used to evaluate the
 * chosen one twice), keep the usual arithmetic conversions, and nest. */
static int test_minmax(void)
{
        int i = 0;
        int n = 5;
        int m = MACRO_MAX(i++, n);
        ASSERT(i == 1 && m == 5, "MACRO_MAX(i++, n): i = %d, result %d", i, m);
        m = MACRO_MIN(n--, 10);
        ASSERT(n == 4 && m == 5, "MACRO_MIN(n--, 10): n = %d, result %d", n, m);
        double d = MACRO_MAX(1, 2.5);           /* int and double: double */
        ASSERT(d == 2.5, "mixed types gave %g", d);
        m = MACRO_MAX(MACRO_MIN(i, 3), MACRO_MAX(n, 2));
        ASSERT(m == 4, "nested gave %d", m);
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
        {"macros", test_macros, "RUN/ASSERT/MMALLOC/MFREE are single statements (if/else)"},
        {"messages", test_messages, "library messages go to stderr, MFREE(NULL) is silent"},
        {"galloc-2d", test_galloc_2d, "2D galloc sets every row and checks its size"},
        {"error-symbol", test_error_symbol, "a program may define its own error()"},
        {"minmax", test_minmax, "MACRO_MIN/MACRO_MAX evaluate each argument once"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the core/alloc macro fixes: statement macros, messages\n"
                  "on stderr, 2D galloc, and no global error() symbol.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  macros        RUN/ASSERT/MMALLOC/MFREE are single statements (if/else)\n"
                  "  messages      library messages go to stderr, MFREE(NULL) is silent\n"
                  "  galloc-2d     2D galloc sets every row and checks its size\n"
                  "  error-symbol  a program may define its own error()\n"
                  "  minmax        MACRO_MIN/MACRO_MAX evaluate each argument once\n"
                  "\n"
                  "Example: regress-alloc galloc-2d");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-alloc: no check named '%s' (see -h)\n", argv[i]);
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

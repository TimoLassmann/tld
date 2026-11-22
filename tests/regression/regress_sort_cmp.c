/* Regression test for the sort comparators: they never returned 0 for equal
 * elements, which breaks qsort's contract (cmp(x, x) must be 0 and
 * cmp(a, b) == -cmp(b, a)); qsort's behaviour is then undefined.
 *
 * The comparators are static, so this test compiles tld-sort.c into itself.
 * It uses nothing else from tld (no RUN/ASSERT, no message functions), so the
 * tld-dev archive member is never pulled in and no symbol is defined twice. */
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../src/sort/tld-sort.c"

static int failures = 0;

/* tld's message functions sit in the same tld-dev object as the sort code, so -v
 * output is printed here, in the plain format of SECTION_MSG, OK_MSG and DEBUG_MSG. */
#undef SECTION_MSG
#undef OK_MSG
#undef DEBUG_MSG
static int level(void)
{
        const char *v = getenv("TLD_VERBOSE");
        return v && *v ? atoi(v) : 1;
}
#define SAY_(MIN, PRE, POST, ...) do {                                  \
                if (level() >= (MIN)) {                                 \
                        fprintf(stderr, PRE);                           \
                        fprintf(stderr, __VA_ARGS__);                   \
                        fprintf(stderr, POST);                          \
                }                                                       \
        } while (0)
#define SECTION_MSG(...) SAY_(1, "== ", " ==\n", __VA_ARGS__)
#define OK_MSG(...) SAY_(1, " OK ", "\n", __VA_ARGS__)
#define DEBUG_MSG(...) SAY_(2, " . ", "\n", __VA_ARGS__)

#define CHECK(cond)                                                     \
        do {                                                            \
                if (!(cond)) {                                          \
                        fprintf(stderr, "FAILED: %s (line %d)\n", #cond, __LINE__); \
                        failures++;                                     \
                }                                                       \
        } while (0)

#define CHECK_CMP(type, lo, hi)                                         \
        do {                                                            \
                type x = (lo);                                          \
                type y = (hi);                                          \
                type z = (lo);                                          \
                CHECK(cmp_asc_##type(&x, &z) == 0);                     \
                CHECK(cmp_desc_##type(&x, &z) == 0);                    \
                CHECK(cmp_asc_##type(&x, &y) < 0);                      \
                CHECK(cmp_asc_##type(&y, &x) > 0);                      \
                CHECK(cmp_desc_##type(&x, &y) > 0);                     \
                CHECK(cmp_desc_##type(&y, &x) < 0);                     \
        } while (0)

static int test_cmp(void)
{
        int before = failures;
        CHECK_CMP(char, 'a', 'b');
        CHECK_CMP(int8_t, -3, 4);
        CHECK_CMP(uint8_t, 1, 200);
        CHECK_CMP(int16_t, -300, 300);
        CHECK_CMP(uint16_t, 3, 60000);
        CHECK_CMP(int32_t, -70000, 70000);
        CHECK_CMP(uint32_t, 1, 4000000000u);
        CHECK_CMP(int64_t, INT64_MIN, INT64_MAX);
        CHECK_CMP(uint64_t, 0, UINT64_MAX);
        CHECK_CMP(float, -1.5f, 2.5f);
        CHECK_CMP(double, -1e300, 1e300);
        DEBUG_MSG("11 types, 6 comparisons each");
        return failures == before ? 0 : 1;
}

static int test_ties(void)
{
        int before = failures;
        /* many ties */
        int32_t a[1000];
        for (int i = 0; i < 1000; i++) {
                a[i] = (i * 7919) % 5;
        }
        sort_asc_int32_t(a, 1000);
        for (int i = 1; i < 1000; i++) {
                CHECK(a[i - 1] <= a[i]);
        }
        sort_desc_int32_t(a, 1000);
        for (int i = 1; i < 1000; i++) {
                CHECK(a[i - 1] >= a[i]);
        }

        return failures == before ? 0 : 1;
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"cmp", test_cmp, "comparators of all 11 types return 0 on equal elements"},
        {"ties", test_ties, "sort_asc/sort_desc of 1000 values with 5 distinct keys"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression check for the sort comparators: cmp(x, x) == 0 and\n"
                  "cmp(a, b) == -cmp(b, a) for every element type; sorting with many ties.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  cmp   comparators of all 11 types return 0 on equal elements\n"
                  "  ties  sort_asc/sort_desc of 1000 values with 5 distinct keys\n"
                  "\n"
                  "Example: regress-sort-cmp cmp");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-sort-cmp: no check named '%s' (see -h)\n", argv[i]);
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
                if(checks[k].fn() != 0){
                        goto ERROR;
                }
                OK_MSG("%s passed", checks[k].name);
        }
        return EXIT_SUCCESS;
ERROR:
        fprintf(stderr, "regress-sort-cmp: %d failures\n", failures);
        return EXIT_FAILURE;
}

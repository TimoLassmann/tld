/* Regression test for the sort comparators: they never returned 0 for equal
 * elements, which breaks qsort's contract (cmp(x, x) must be 0 and
 * cmp(a, b) == -cmp(b, a)); qsort's behaviour is then undefined.
 *
 * The comparators are static, so this test compiles tld-sort.c into itself.
 * It uses nothing else from tld (no RUN/ASSERT), so the tld-dev archive
 * member is never pulled in and no symbol is defined twice. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../src/sort/tld-sort.c"

static int failures = 0;

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

int main(void)
{
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

        if (failures) {
                fprintf(stderr, "unit_fix_sort_cmp: %d failures\n", failures);
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}

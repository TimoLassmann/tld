/* Regression tests for the hash and bitfield fixes.
 * - tld_hash_add did not check its arguments or the return codes of the calls
 *   it makes (NULL key: SEGV), and leaked a table it allocated when a later
 *   step failed.
 * - tld_hash_rename ignored every return code: a failing add after the delete
 *   lost the entry (and a NULL new key crashed).
 * - tld_hash_alloc with a size above the largest prime left table_size 0
 *   (modulo by zero on the first add).
 * - tld_bitfield_hamming never checked the mask length (heap over-read).
 */
#include "tld.h"
#include "test.h"
#include <string.h>

static int test_hash_add_errors(void)
{
        tld_hash *h = NULL;
        const char *v = NULL;

        /* new table + failure: nothing handed back */
        ASSERT(tld_hash_add(&h, NULL, "v") == FAIL, "NULL key accepted");
        ASSERT(h == NULL, "table handed back after a failed add");
        ASSERT(tld_hash_add(&h, "k", NULL) == FAIL, "NULL value accepted");
        ASSERT(h == NULL, "table handed back after a failed add");

        RUN(tld_hash_add(&h, "k", "v"));
        ASSERT(h != NULL, "no table");
        RUN(tld_hash_get(h, "k", &v));
        ASSERT(strcmp(v, "v") == 0, "got %s", v);
        /* replace */
        RUN(tld_hash_add(&h, "k", "w"));
        RUN(tld_hash_get(h, "k", &v));
        ASSERT(strcmp(v, "w") == 0, "got %s", v);
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

static int test_hash_rename(void)
{
        tld_hash *h = NULL;
        const char *v = NULL;

        RUN(tld_hash_add(&h, "a", "1"));
        RUN(tld_hash_add(&h, "b", "2"));

        RUN(tld_hash_rename(h, "a", "c"));
        ASSERT(tld_hash_get(h, "a", &v) == FAIL, "old key still present");
        RUN(tld_hash_get(h, "c", &v));
        ASSERT(strcmp(v, "1") == 0, "got %s", v);

        /* rename to itself keeps the entry */
        RUN(tld_hash_rename(h, "c", "c"));
        RUN(tld_hash_get(h, "c", &v));
        ASSERT(strcmp(v, "1") == 0, "got %s", v);

        /* a failing rename leaves the entry alone */
        ASSERT(tld_hash_rename(h, "b", NULL) == FAIL, "NULL new key accepted");
        RUN(tld_hash_get(h, "b", &v));
        ASSERT(strcmp(v, "2") == 0, "got %s", v);

        ASSERT(tld_hash_rename(h, "zzz", "y") == FAIL, "renamed a missing key");
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

/* A value from tld_hash_get written back under its own key was cleared before it
   was copied: the value silently became "". */
static int test_hash_self_assign(void)
{
        tld_hash *h = NULL;
        const char *v = NULL;
        RUN(tld_hash_add(&h, "a", "value of a"));
        RUN(tld_hash_get(h, "a", &v));
        RUN(tld_hash_add(&h, "a", v));
        RUN(tld_hash_get(h, "a", &v));
        ASSERT(strcmp(v, "value of a") == 0, "self-assign gave '%s'", v);
        RUN(tld_hash_add(&h, "a", v + 9));              /* a suffix of the old value */
        RUN(tld_hash_get(h, "a", &v));
        ASSERT(strcmp(v, "a") == 0, "suffix assign gave '%s'", v);
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

static int test_hash_alloc_size(void)
{
        tld_hash *h = NULL;
        ASSERT(tld_hash_alloc(&h, 2000000000) == FAIL, "table of size 0 accepted");
        ASSERT(h == NULL, "table handed back");

        /* many entries: exercise resize */
        char key[32];
        const char *v = NULL;
        for (int i = 0; i < 5000; i++) {
                snprintf(key, sizeof(key), "key%d", i);
                RUN(tld_hash_add(&h, key, key));
        }
        RUN(tld_hash_get(h, "key4321", &v));
        ASSERT(strcmp(v, "key4321") == 0, "got %s", v);
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

static int test_bitfield_mask(void)
{
        tld_bitfield *a = NULL;
        tld_bitfield *b = NULL;
        tld_bitfield *small = NULL;
        tld_bitfield *mask = NULL;
        int32_t dist = -1;

        RUN(tld_bitfield_alloc(&a, 1000));
        RUN(tld_bitfield_alloc(&b, 1000));
        RUN(tld_bitfield_alloc(&mask, 1000));
        RUN(tld_bitfield_alloc(&small, 10));
        RUN(tld_bitfield_set(a, 3));
        RUN(tld_bitfield_set(a, 900));
        RUN(tld_bitfield_set(mask, 900));

        RUN(tld_bitfield_hamming(a, b, NULL, &dist));
        ASSERT(dist == 2, "dist %d", dist);
        RUN(tld_bitfield_hamming(a, b, mask, &dist));
        ASSERT(dist == 1, "dist %d", dist);

        /* mask shorter than the bitfields: reading it was a heap over-read */
        dist = -1;
        ASSERT(tld_bitfield_hamming(a, b, small, &dist) == FAIL, "short mask accepted");
        ASSERT(dist == -1, "dist written on failure");

        tld_bitfield_free(a);
        tld_bitfield_free(b);
        tld_bitfield_free(mask);
        tld_bitfield_free(small);
        return OK;
ERROR:
        tld_bitfield_free(a);
        tld_bitfield_free(b);
        tld_bitfield_free(mask);
        tld_bitfield_free(small);
        return FAIL;
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"add-errors", test_hash_add_errors, "tld_hash_add checks arguments and frees on failure"},
        {"rename", test_hash_rename, "tld_hash_rename keeps the entry when a step fails"},
        {"alloc-size", test_hash_alloc_size, "tld_hash_alloc above the largest prime"},
        {"self-assign", test_hash_self_assign, "a value from tld_hash_get added back under its key"},
        {"bitfield-mask", test_bitfield_mask, "tld_bitfield_hamming checks the mask length"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the hash and bitfield fixes.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  add-errors     tld_hash_add checks arguments and frees on failure\n"
                  "  rename         tld_hash_rename keeps the entry when a step fails\n"
                  "  alloc-size     tld_hash_alloc above the largest prime\n"
                  "  self-assign    a value from tld_hash_get added back under its key\n"
                  "  bitfield-mask  tld_bitfield_hamming checks the mask length\n"
                  "\n"
                  "Example: regress-hash rename alloc-size");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-hash: no check named '%s' (see -h)\n", argv[i]);
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
                test_expect_errors();
                RUN(checks[k].fn());
                test_expect_errors_end();
                OK_MSG("%s passed", checks[k].name);
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

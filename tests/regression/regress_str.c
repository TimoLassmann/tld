/* Regression tests for the string fixes.
 * - tld_strbuf_resize never updated alloc_len, so every append after the first
 *   growth reallocated and copied the whole buffer.
 * - Growing a buffer near 1-2 GB overflowed its int size, returned OK and wrote past
 *   the end; now the append fails.
 */
#include "tld.h"
#include "test.h"
#include <string.h>

static int test_strbuf_growth(void)
{
        tld_strbuf *b = NULL;
        int32_t last_alloc = 0;
        int n_growth = 0;
        RUN(tld_strbuf_alloc(&b, 16));
        last_alloc = b->alloc_len;
        for (int i = 0; i < 100000; i++) {
                RUN(tld_append_char(b, 'a' + (i % 26)));
                ASSERT(b->alloc_len > b->len, "alloc_len %d not above len %d", b->alloc_len, b->len);
                if (b->alloc_len != last_alloc) {
                        n_growth++;
                        last_alloc = b->alloc_len;
                }
        }
        ASSERT(b->len == 100000, "len is %d", b->len);
        ASSERT(n_growth > 0 && n_growth < 32, "%d growths for 1e5 appends", n_growth);
        ASSERT(TLD_STR(b)[b->len] == 0, "not terminated");
        ASSERT(TLD_STR(b)[26] == 'a', "wrong content");

        RUN(tld_strbuf_clear(b));
        RUN(tld_append(b, "xyz"));
        RUN(tld_prepend(b, "abc"));
        ASSERT(strcmp(TLD_STR(b), "abcxyz") == 0, "got %s", TLD_STR(b));
        tld_strbuf_free(b);
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

/* A buffer near the int32_t size limit refuses to grow instead of wrapping its size
 * and writing past the end. Only the bookkeeping is faked (the real block is 64
 * bytes): the old arithmetic wrapped, returned OK and copied past the block, which
 * ASan reports as a heap overflow. Prints one expected ERROR line. */
static int test_strbuf_limit(void)
{
        tld_strbuf *b = NULL;
        int32_t real_alloc = 0;
        RUN(tld_strbuf_alloc(&b, 64));
        real_alloc = b->alloc_len;
        b->alloc_len = INT32_MAX - 8;        /* as if it had grown to 2 GB ... */
        b->len = INT32_MAX - 20;             /* ... and was nearly full */
        int rc = tld_append(b, "0123456789012345678901234567890123456789");
        ASSERT(rc == FAIL, "append past INT32_MAX succeeded");
        ASSERT(b->len == INT32_MAX - 20, "len changed to %d", b->len);
        b->alloc_len = real_alloc;
        b->len = 0;
        tld_strbuf_free(b);
        return OK;
ERROR:
        if (b) {
                b->alloc_len = real_alloc;
                b->len = 0;
        }
        tld_strbuf_free(b);
        return FAIL;
}

/* Text taken from the buffer itself (TLD_STR(b), or a suffix of it) was read after
   the buffer had grown and freed its old storage (ASan: heap-use-after-free in
   tld_append / tld_prepend). */
static int test_self_append(void)
{
        tld_strbuf *b = NULL;
        RUN(tld_strbuf_alloc(&b, 4));
        RUN(tld_append(b, "abc"));
        RUN(tld_append(b, TLD_STR(b)));                 /* grows: 3 + 3 + 1 > 4 */
        ASSERT(strcmp(TLD_STR(b), "abcabc") == 0, "self-append gave '%s'", TLD_STR(b));
        RUN(tld_append(b, TLD_STR(b) + 4));             /* a suffix of itself */
        ASSERT(strcmp(TLD_STR(b), "abcabcbc") == 0, "suffix append gave '%s'", TLD_STR(b));
        for(int i = 0; i < 6; i++){                     /* doubles each time */
                RUN(tld_append(b, TLD_STR(b)));
        }
        ASSERT(TLD_STRLEN(b) == 8 * 64, "length %d after six doublings", TLD_STRLEN(b));
        RUN(tld_strbuf_clear(b));
        RUN(tld_append(b, "xyz"));
        RUN(tld_prepend(b, TLD_STR(b)));
        ASSERT(strcmp(TLD_STR(b), "xyzxyz") == 0, "self-prepend gave '%s'", TLD_STR(b));
        RUN(tld_prepend(b, TLD_STR(b) + 4));
        ASSERT(strcmp(TLD_STR(b), "yzxyzxyz") == 0, "suffix prepend gave '%s'", TLD_STR(b));
        RUN(tld_strbuf_copy(b, b));
        ASSERT(strcmp(TLD_STR(b), "yzxyzxyz") == 0, "self-copy gave '%s'", TLD_STR(b));
        tld_strbuf_free(b);
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"growth", test_strbuf_growth, "tld_strbuf grows geometrically (alloc_len updated)"},
        {"limit", test_strbuf_limit, "an append past INT32_MAX fails instead of overflowing"},
        {"self", test_self_append, "append, prepend and copy of a buffer's own text"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the string fixes.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  growth  tld_strbuf grows geometrically (alloc_len updated)\n"
                  "  limit   an append past INT32_MAX fails instead of overflowing\n"
                  "  self    append, prepend and copy of a buffer's own text\n"
                  "\n"
                  "Example: regress-str growth");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-str: no check named '%s' (see -h)\n", argv[i]);
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

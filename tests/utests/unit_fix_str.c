/* Regression tests for the string fixes.
 * - tld_strmk wrote one byte past its buffer (ASan heap-buffer-overflow) and
 *   counted the NUL in len.
 * - tld_strbuf_resize never updated alloc_len, so every append after the first
 *   growth reallocated and copied the whole buffer.
 */
#include "tld.h"
#include <string.h>

static int test_strmk(void)
{
        tld_str *s = NULL;
        tld_str *e = NULL;
        RUN(tld_strmk(&s, "hello"));
        ASSERT(s->len == 5, "len is %d, expected 5", s->len);
        ASSERT(strcmp(TLD_STR(s), "hello") == 0, "wrong content");
        ASSERT(TLD_STR(s)[5] == 0, "not terminated");
        tld_str_free(s);
        s = NULL;

        RUN(tld_strmk(&e, ""));
        ASSERT(e->len == 0, "len is %d, expected 0", e->len);
        ASSERT(TLD_STR(e)[0] == 0, "not terminated");
        tld_str_free(e);
        return OK;
ERROR:
        tld_str_free(s);
        tld_str_free(e);
        return FAIL;
}

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

int main(void)
{
        RUN(test_strmk());
        RUN(test_strbuf_growth());
        LOG_MSG("unit_fix_str: all passed");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

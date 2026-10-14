#include "tld.h"
#include <stdio.h>
#include <stdlib.h>

/* Regression test: tld_strbuf_resize must update alloc_len, otherwise every
 * append after the first growth reallocates and copies the whole buffer. */
int main(void)
{
        tld_strbuf* b = NULL;
        int reallocs = 0;
        int last_alloc = 0;
        const int n = 1000000;

        RUN(tld_strbuf_alloc(&b, 16));
        last_alloc = b->alloc_len;
        for(int i = 0; i < n; i++){
                RUN(tld_append_char(b, 'a'));
                ASSERT(b->alloc_len >= b->len, "alloc_len %d < len %d", b->alloc_len, b->len);
                if(b->alloc_len != last_alloc){
                        reallocs++;
                        last_alloc = b->alloc_len;
                }
        }
        ASSERT(b->len == n, "len is %d", b->len);
        ASSERT(reallocs < 64, "too many reallocations: %d", reallocs);
        LOG_MSG("reallocs: %d", reallocs);
        tld_strbuf_free(b);
        return EXIT_SUCCESS;
ERROR:
        tld_strbuf_free(b);
        return EXIT_FAILURE;
}

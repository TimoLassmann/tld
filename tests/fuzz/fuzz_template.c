/* fuzz-template: tld_template_extract_var and tld_template_apply on arbitrary text. */
#define TLD_IMPLEMENTATION
#include "tld.h"
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        if (n > 1 << 14) return 0;
        char *s = malloc(n + 1); memcpy(s, d, n); s[n] = 0;
        tld_strbuf *b = NULL; tld_hash *h = NULL, *v = NULL;
        tld_strbuf_alloc(&b, (int)n + 1); tld_append(b, s);
        if (tld_template_extract_var(b, &v) == OK) tld_hash_free(v);
        tld_hash_alloc(&h, 16);
        tld_hash_add(&h, "name", "World"); tld_hash_add(&h, "x", "1"); tld_hash_add(&h, "list", "a,b,c"); tld_hash_add(&h, "empty", "");
        tld_template_apply(b, h);
        tld_hash_free(h); tld_strbuf_free(b); free(s);
        return 0;
}

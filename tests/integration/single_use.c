/* single-header test, TU 2 of 2: declarations only. */
#include "tld.h"

int single_use(void)
{
        tld_hash *h = NULL;
        const char *v = NULL;
        RUN(tld_hash_alloc(&h, 16));
        RUN(tld_hash_add(&h, "key", "value"));
        RUN(tld_hash_get(h, "key", &v));
        if (v == NULL || strcmp(v, "value") != 0) ERROR_MSG("hash lookup failed");
        tld_hash_free(h);
        return OK;
ERROR:
        return FAIL;
}

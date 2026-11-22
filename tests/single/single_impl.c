/* single-header test, TU 1 of 2: holds the implementation. Links with single_use.c, so a
 * non-static helper defined in the header half, or a missing implementation, fails here. */
#define TLD_IMPLEMENTATION
#include "tld.h"

int single_use(void);

int main(void)
{
        tld_strbuf *b = NULL;
        struct rng_state *rng = NULL;
        RUN(tld_strbuf_alloc(&b, 16));
        RUN(tld_append(b, "single"));
        RUN(tld_append(b, "-header"));
        if (strcmp(TLD_STR(b), "single-header") != 0) ERROR_MSG("append gave %s", TLD_STR(b));
        RUN(init_rng(&rng, 42));
        double r = tl_random_double(rng);
        if (!(r > 0.0 && r < 1.0)) ERROR_MSG("random double %f out of range", r);
        free_rng(rng);
        tld_strbuf_free(b);
        RUN(single_use());
        fprintf(stdout, "single-header: ok\n");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

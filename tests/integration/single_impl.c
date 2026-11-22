/* single-header test, TU 1 of 2: holds the implementation. Links with single_use.c, so a
 * non-static helper defined in the header half, or a missing implementation, fails here. */
#define TLD_IMPLEMENTATION
#include "tld.h"
#include "test.h"

int single_use(void);

int main(int argc, char *argv[])
{
        tld_strbuf *b = NULL;
        struct rng_state *rng = NULL;
        test_init(&argc, argv, "[WORD ...]",
                  "Built from the single-header ./tld.h in two translation units (implementation\n"
                  "here, declarations only in single_use.c). With no arguments checks a string\n"
                  "buffer, the rng and a hash lookup across the two units. With WORDs also joins\n"
                  "them with '-' in a tld_strbuf and prints the result.\n"
                  "Example: single-header one two three");
        SECTION_MSG("tld.h single header");
        RUN(tld_strbuf_alloc(&b, 16));
        RUN(tld_append(b, "single"));
        RUN(tld_append(b, "-header"));
        if (strcmp(TLD_STR(b), "single-header") != 0) ERROR_MSG("append gave %s", TLD_STR(b));
        OK_MSG("strbuf: %s", TLD_STR(b));
        RUN(init_rng(&rng, 42));
        double r = tl_random_double(rng);
        if (!(r > 0.0 && r < 1.0)) ERROR_MSG("random double %f out of range", r);
        OK_MSG("rng: %f", r);
        free_rng(rng);
        RUN(single_use());
        OK_MSG("hash lookup in the second translation unit");
        if (argc > 1) {
                RUN(tld_strbuf_clear(b));
                for (int i = 1; i < argc; i++) {
                        if (i > 1) RUN(tld_append_char(b, '-'));
                        RUN(tld_append(b, argv[i]));
                }
                fprintf(stdout, "%s\n", TLD_STR(b));
        }
        tld_strbuf_free(b);
        OK_MSG("single-header: ok");
        return EXIT_SUCCESS;
ERROR:
        tld_strbuf_free(b);
        return EXIT_FAILURE;
}

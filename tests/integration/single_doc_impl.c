/* single-header-doc test, TU 1 of 2: the implementations of tld.h and tld_doc.h, from
 * the single headers ./tld_doc.h and ./tld.h. Links with single_doc_use.c, so a
 * non-static helper in a header half, or a missing implementation, fails here. ctest
 * builds it twice: as single-header-doc with the build's flags, and as
 * single-header-doc-strict with -std=c11 -Wall -Wextra -pedantic -Werror. */
#define TLD_IMPLEMENTATION
#define TLD_DOC_IMPLEMENTATION
#include "tld_doc.h"
#include "tld_doc.h" /* a second include must be harmless */
#include "test.h"

int single_doc_use(void);

int main(int argc, char *argv[])
{
        tld_doc_answer *a = NULL;
        tld_doc_opts o = {.max_bytes = 2048};
        test_init(&argc, argv, "",
                  "Built from the single headers ./tld_doc.h and ./tld.h in two translation\n"
                  "units (implementations here, declarations only in single_doc_use.c). Asks\n"
                  "tld_doc_ask one question and reads the corpus and tld.h from the second unit.");
        SECTION_MSG("tld_doc.h single header");
        RUN(tld_doc_ask("how do I parse a JSON string", &o, &a));
        ASSERT(a->n_hits > 0, "no hits");
        ASSERT(a->text != NULL && strlen(a->text) <= 2048, "text over the budget");
        OK_MSG("%d hits, first %s", a->n_hits, a->hits[0].entry->names);
        tld_doc_answer_free(a);
        a = NULL;
        RUN(single_doc_use());
        OK_MSG("corpus and tld.h in the second translation unit");
        OK_MSG("single-header-doc: ok");
        return EXIT_SUCCESS;
ERROR:
        tld_doc_answer_free(a);
        return EXIT_FAILURE;
}

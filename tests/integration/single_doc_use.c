/* single-header-doc test, TU 2 of 2: declarations only, of both headers. tld_doc.h
 * does not include tld.h here, so this file includes it for the names it uses. */
#include "tld_doc.h"
#include "tld.h"

int single_doc_use(void)
{
        const tld_doc_entry *e = NULL;
        tld_hash *h = NULL;
        const char *v = NULL;
        ASSERT(tld_doc_n_entries() > 0, "empty corpus");
        e = tld_doc_entry_at(tld_doc_n_entries() - 1);
        ASSERT(e != NULL && strcmp(e->header, "tld_ai.h") == 0, "last record is not from tld_ai.h");
        ASSERT(strlen(tld_doc_corpus_id()) > 17 && tld_doc_corpus_id()[16] == '-', "corpus id %s",
               tld_doc_corpus_id());
        RUN(tld_hash_alloc(&h, 16));
        RUN(tld_hash_add(&h, "key", "value"));
        RUN(tld_hash_get(h, "key", &v));
        ASSERT(v != NULL && strcmp(v, "value") == 0, "hash lookup failed");
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

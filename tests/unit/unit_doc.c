/* unit-doc: tld_doc.h. Built from the single headers ./tld_doc.h and ./tld.h.
 *
 * The corpus, checked against the files it was generated from rather than against
 * tools/tlddoc.c (PRD section 5):
 *   - one record per ### entry of docs/tld.md and docs/tld_ai.md, in manual order, with
 *     the names the heading shows (headings with a <param> are expanded families);
 *   - every line of each record's declaration appears verbatim in its header, with two
 *     exceptions the manual makes: a macro longer than 7 lines is shown as its signature
 *     "#define NAME(params) ...", whose part before " ..." must appear verbatim; and a
 *     line expanded from a one-parameter macro invocation (TLD__SORT_HEADER,
 *     TLD__SHUFFLE_HEADER) needs that invocation in the header for the line's first
 *     parameter type;
 *   - tld_doc_corpus_id() is the FNV-1a 64 of the records, as documented, plus the
 *     AI_VERSION of tld_ai.h, and the same string on every call;
 *   - the corpus API: tld_doc_n_entries, tld_doc_entry_at in and out of range.
 *
 * The search and the answer text (tld_doc_ask):
 *   - fixed questions: identifiers (exact, with a typo, without '_', written as a name
 *     with its case: OK but not ok), the order and number of hits, the words found and
 *     not found ("the weather in paris"), typos searched again with edits through the
 *     stem ("alocations") or as written ("alocation", "sortting") and their nearest
 *     forms, a repeated word in two spellings in either order, no words, no match, the
 *     same text twice;
 *   - every text: at most max_bytes or its head, the head at most 2300 bytes, the
 *     entries a prefix of the hits in rank order with the rest listed under "Not shown",
 *     no declaration cut, a doc text whole or cut after a sentence, an example whole, a
 *     See also name one the doc text mentions, the module description once per module,
 *     every nearest form letters, digits and '_' only;
 *   - the budget for many max_bytes values;
 *   - edge cases: an empty question, only stopwords, a very long question, the longest
 *     head this corpus can give, bytes >= 0x80 and control bytes, bad arguments.
 *
 *   unit-doc [ROOT]     ROOT: the repository root (default: the source tree it was built from)
 */
#define TLD_IMPLEMENTATION
#define TLD_DOC_IMPLEMENTATION
#include "tld_doc.h"
#include "test.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef TLD_DOC_TEST_ROOT
#define TLD_DOC_TEST_ROOT "."
#endif

static int failed = 0;

#define CHECK(cond, ...) do {                                           \
                if(!(cond)){                                            \
                        fprintf(stderr, "FAIL  " __VA_ARGS__);          \
                        fputc('\n', stderr);                            \
                        failed++;                                       \
                }                                                       \
        } while(0)

static int read_root_file(const char *root, const char *name, tld_strbuf **out)
{
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", root, name);
        RUN(tld_path_read(path, out));
        return OK;
ERROR:
        return FAIL;
}

/* The ### headings of a manual, in order, each as its names joined with ", " (the
 * backticked names of the heading); *generic set for a heading with a <param>. */
typedef struct {
        char names[8192];
        int generic;
        const char *header;
} heading;

static int read_headings(const char *text, const char *header, heading *h, int max, int *n)
{
        const char *s = text;
        while(*s){
                const char *nl = strchr(s, '\n');
                size_t len = nl ? (size_t)(nl - s) : strlen(s);
                if(len > 4 && strncmp(s, "### ", 4) == 0){
                        heading *x;
                        size_t o = 0;
                        int k = 0;
                        ASSERT(*n < max, "More than %d headings", max);
                        x = &h[(*n)++];
                        x->header = header;
                        x->generic = 0;
                        for(size_t i = 4; i < len; i++){
                                if(s[i] != '`'){
                                        continue;
                                }
                                {
                                        size_t j = i + 1;
                                        while(j < len && s[j] != '`'){
                                                j++;
                                        }
                                        ASSERT(o + (j - i) + 3 < sizeof x->names, "Heading too long");
                                        if(k++){
                                                memcpy(x->names + o, ", ", 2);
                                                o += 2;
                                        }
                                        memcpy(x->names + o, s + i + 1, j - i - 1);
                                        o += j - i - 1;
                                        i = j;
                                }
                        }
                        x->names[o] = 0;
                        x->generic = strchr(x->names, '<') != NULL;
                }
                s += len + (nl ? 1 : 0);
        }
        return OK;
ERROR:
        return FAIL;
}

/* Is every line of decl in the header text src? See the top comment for the macro
 * signatures (counted in *sigs) and the lines expanded from a macro invocation (counted
 * in *expanded). */
static void check_decl(const tld_doc_entry *e, const char *src, int i, int *sigs, int *expanded)
{
        const char *s = e->decl;
        while(*s){
                const char *nl = strchr(s, '\n');
                size_t len = nl ? (size_t)(nl - s) : strlen(s);
                char line[4096];
                if(len >= sizeof line){
                        CHECK(0, "record %d (%s): declaration line too long", i, e->names);
                        return;
                }
                memcpy(line, s, len);
                line[len] = 0;
                if(strstr(src, line) == NULL && len > 12 && strncmp(line, "#define ", 8) == 0 &&
                   strcmp(line + len - 4, " ...") == 0){
                        line[len - 4] = 0;
                        if(strstr(src, line)){
                                (*sigs)++;
                        }else{
                                CHECK(0, "record %d (%s): macro signature not in %s: %s", i,
                                      e->names, e->header, line);
                        }
                }else if(strstr(src, line) == NULL){
                        /* expanded: "int tld_sort_desc_char(char *a, int len);" comes
                           from TLD__SORT_HEADER(char) */
                        const char *p = strchr(line, '(');
                        char inv[512];
                        size_t t = 0;
                        if(p){
                                p++;
                                while(p[t] && p[t] != ' ' && p[t] != '*' && p[t] != ','){
                                        t++;
                                }
                        }
                        if(p && t && t < 256){
                                snprintf(inv, sizeof inv, "_HEADER(%.*s)", (int)t, p);
                        }
                        if(p && t && t < 256 && strstr(src, inv)){
                                (*expanded)++;
                        }else{
                                CHECK(0, "record %d (%s): declaration line not in %s: %s", i,
                                      e->names, e->header, line);
                        }
                }
                s += len + (nl ? 1 : 0);
        }
}

static uint64_t fnv1a(uint64_t h, const char *p, size_t n)
{
        for(size_t i = 0; i < n; i++){
                h ^= (unsigned char)p[i];
                h *= 0x100000001b3ULL;
        }
        return h;
}

static int test_corpus(const char *root)
{
        static heading h[1024];
        tld_strbuf *man[2] = {NULL, NULL};
        tld_strbuf *src[2] = {NULL, NULL};
        static const char *hdr[2] = {"tld.h", "tld_ai.h"};
        static const char *mds[2] = {"docs/tld.md", "docs/tld_ai.md"};
        int n_h = 0;
        int n = tld_doc_n_entries();
        int expanded = 0;
        int sigs = 0;
        int with_example = 0;
        uint64_t hash = 0xcbf29ce484222325ULL;
        char want[64];
        const char *id = tld_doc_corpus_id();

        for(int f = 0; f < 2; f++){
                RUN(read_root_file(root, mds[f], &man[f]));
                RUN(read_root_file(root, hdr[f], &src[f]));
                RUN(read_headings(TLD_STR(man[f]), hdr[f], h, 1024, &n_h));
        }

        SECTION_MSG("records versus the manuals");
        CHECK(n == n_h, "%d records, but docs/tld.md and docs/tld_ai.md have %d ### entries", n, n_h);
        KV_MSG("records", "%d (### entries: %d)", n, n_h);
        for(int i = 0; i < n && i < n_h; i++){
                const tld_doc_entry *e = tld_doc_entry_at(i);
                CHECK(e != NULL, "tld_doc_entry_at(%d) is NULL", i);
                if(e == NULL){
                        continue;
                }
                CHECK(e->names && e->header && e->module && e->module_desc && e->decl && e->doc,
                      "record %d: a field other than example is NULL", i);
                if(!(e->names && e->header && e->module && e->module_desc && e->decl && e->doc)){
                        continue;
                }
                CHECK(strcmp(e->header, h[i].header) == 0, "record %d (%s): header %s, manual entry in %s",
                      i, e->names, e->header, h[i].header);
                if(!h[i].generic){
                        CHECK(strcmp(e->names, h[i].names) == 0, "record %d: names \"%s\", heading \"%s\"",
                              i, e->names, h[i].names);
                }
                CHECK(e->names[0] && e->module[0] && e->decl[0], "record %d: empty names, module or decl", i);
                with_example += e->example != NULL;
                CHECK(e->example == NULL || e->example[0], "record %d (%s): empty example", i, e->names);
        }
        KV_MSG("with examples", "%d", with_example);
        CHECK(with_example > 0, "no record has a code example");

        SECTION_MSG("declarations versus the headers");
        for(int i = 0; i < n; i++){
                const tld_doc_entry *e = tld_doc_entry_at(i);
                if(e == NULL || e->decl == NULL || e->header == NULL){
                        continue;
                }
                CHECK(strcmp(e->header, "tld.h") == 0 || strcmp(e->header, "tld_ai.h") == 0,
                      "record %d: header %s", i, e->header);
                check_decl(e, TLD_STR(src[strcmp(e->header, "tld.h") == 0 ? 0 : 1]), i, &sigs,
                           &expanded);
        }
        KV_MSG("macro signatures", "%d (macros longer than 7 lines)", sigs);
        KV_MSG("expanded lines", "%d (from macro invocations)", expanded);

        SECTION_MSG("corpus identifier");
        for(int i = 0; i < n; i++){
                const tld_doc_entry *e = tld_doc_entry_at(i);
                const char *f[7];
                if(e == NULL){
                        continue;
                }
                f[0] = e->names;
                f[1] = e->header;
                f[2] = e->module;
                f[3] = e->module_desc;
                f[4] = e->decl;
                f[5] = e->doc;
                f[6] = e->example;
                for(int k = 0; k < 7; k++){
                        hash = f[k] ? fnv1a(hash, f[k], strlen(f[k])) : fnv1a(hash, "\001", 1);
                        hash = fnv1a(hash, "", 1);
                }
        }
        {
                /* the AI_VERSION of ROOT/tld_ai.h, read from its text (tld_doc.h does not
                   include tld_ai.h) */
                const char *v = strstr(TLD_STR(src[1]), "\n#define AI_VERSION \"");
                const char *q = v ? strchr(v + 21, '"') : NULL;
                CHECK(q != NULL, "no #define AI_VERSION line in tld_ai.h");
                snprintf(want, sizeof want, "%016" PRIx64 "-%.*s", hash, q ? (int)(q - v - 21) : 0, v ? v + 21 : "");
        }
        KV_MSG("corpus id", "%s", id ? id : "(NULL)");
        CHECK(id != NULL && strcmp(id, want) == 0, "tld_doc_corpus_id() is %s, the records give %s",
              id ? id : "(NULL)", want);
        CHECK(tld_doc_corpus_id() == id, "tld_doc_corpus_id() changes between calls");

        SECTION_MSG("corpus API");
        CHECK(tld_doc_entry_at(-1) == NULL, "tld_doc_entry_at(-1) is not NULL");
        CHECK(tld_doc_entry_at(n) == NULL, "tld_doc_entry_at(n) is not NULL");
        CHECK(n > 0 && tld_doc_entry_at(0) == tld_doc_entry_at(0), "tld_doc_entry_at(0) changes");
        CHECK(strcmp(TLD_DOC_VERSION, "0.1.0") == 0, "TLD_DOC_VERSION is %s", TLD_DOC_VERSION);
        tld_doc_answer_free(NULL);

        for(int f = 0; f < 2; f++){
                tld_strbuf_free(man[f]);
                tld_strbuf_free(src[f]);
        }
        return OK;
ERROR:
        for(int f = 0; f < 2; f++){
                tld_strbuf_free(man[f]);
                tld_strbuf_free(src[f]);
        }
        return FAIL;
}

/* ---- the search and the answer text (PRD sections 7, 8, 10) ------------------ */

/* The length of the head of a text: up to and including the first blank line's first
 * newline, or all of it. */
static size_t head_len(const char *t)
{
        const char *e = strstr(t, "\n\n");
        return e ? (size_t)(e - t) + 1 : strlen(t);
}

/* The 4-space-indented form of s, as the text shows a declaration, doc or example. */
static void indented(const char *s, char *out, size_t cap)
{
        size_t o = 0;
        while(*s && o + 8 < cap){
                const char *nl = strchr(s, '\n');
                size_t len = nl ? (size_t)(nl - s) : strlen(s);
                if(len && o + len + 6 < cap){
                        memcpy(out + o, "    ", 4);
                        o += 4;
                        memcpy(out + o, s, len);
                        o += len;
                }
                out[o++] = '\n';
                s += len + (nl ? 1 : 0);
        }
        out[o] = 0;
}

/* Is the ", "-separated name list names all mentioned in text (as words)? The first that
 * is not goes to bad. */
static int all_mentioned(const char *names, size_t n, const char *text, char *bad, size_t cap)
{
        const char *s = names;
        const char *end = names + n;
        while(s < end){
                const char *c = strstr(s, ", ");
                size_t len = c && c < end ? (size_t)(c - s) : (size_t)(end - s);
                int found = 0;
                for(const char *y = text; *y; y++){
                        if(strncmp(y, s, len) == 0 && (y == text || !(isalnum((unsigned char)y[-1]) || y[-1] == '_')) &&
                           !(isalnum((unsigned char)y[len]) || y[len] == '_')){
                                found = 1;
                                break;
                        }
                }
                if(!found){
                        snprintf(bad, cap, "%.*s", (int)len, s);
                        return 0;
                }
                s += len + 2;
        }
        return 1;
}

/* Check the text of answer a, made with budget max_bytes (0: 4096): the budget, the head
 * bound and lines, the entries a prefix of the hits in rank order, each with its whole
 * declaration, its doc text whole or cut at a sentence end or "not shown", its example
 * whole or not shown, See also names from its doc text, and every other hit listed under
 * Not shown. */
static void check_text(const tld_doc_answer *a, int max_bytes, const char *what)
{
        static char ind[65536];
        static char ent[1024];
        static char bad[256];
        const char *t = a->text;
        size_t n = strlen(t);
        size_t h = head_len(t);
        size_t budget = max_bytes ? (size_t)max_bytes : 4096;
        int shown = 0;
        const char *ns;
        if(n > (budget > h ? budget : h)){
                /* only the head and the shortest Not shown line may go over the budget */
                int more = 0;
                int end = 0;
                CHECK(sscanf(t + h, "\nNot shown (byte budget): ... and %d more\n%n", &more, &end) == 1 &&
                      (size_t)end == n - h && n - h <= 43,
                      "%s, max_bytes %d: %zu bytes, head %zu, and more than the shortest Not shown line", what,
                      max_bytes, n, h);
        }
        CHECK(h <= 2300, "%s: head of %zu bytes (more than 2300)", what, h);
        CHECK(strncmp(t, "tld_doc answer\nQuestion: ", 25) == 0, "%s: text does not start with the title", what);
        CHECK((strstr(t, "\nWords found in the manuals: ") != NULL && strstr(t, "\nWords found in the manuals: ") < t + h) ||
              (strstr(t, "\nThe question has no words to search for") != NULL &&
               strstr(t, "\nThe question has no words to search for") < t + h),
              "%s: neither a Words found line nor a no-words line in the head", what);
        /* a nearest form in the head is letters, digits and '_' only */
        for(const char *x = strstr(t, "(nearest: \""); x && x < t + h; x = strstr(x + 1, "(nearest: \"")){
                const char *y = x + 11;
                while(*y && *y != '"' && (isalnum((unsigned char)*y) || *y == '_')){
                        y++;
                }
                CHECK(*y == '"' && y > x + 11, "%s: the nearest form at \"%.40s\" is not word bytes only", what, x);
        }
        ns = strstr(t, "\nNot shown (byte budget): ");
        for(int i = 0; i < a->n_hits; i++){
                const tld_doc_entry *e = a->hits[i].entry;
                const char *p;
                const char *block_end;
                const char *see;
                snprintf(ent, sizeof ent, "\nENTRY %d: %s\n", i + 1, e->names);
                p = strstr(t, ent);
                if(p == NULL){
                        break;
                }
                shown++;
                block_end = strstr(p + 1, "\nENTRY ");
                if(block_end == NULL){
                        block_end = ns ? ns : t + n;
                }
                /* the module description only with the first entry of its module */
                if(e->module_desc[0]){
                        int first = 1;
                        const char *desc;
                        for(int x = 0; x < i; x++){
                                first &= !(strcmp(a->hits[x].entry->module, e->module) == 0 &&
                                           strcmp(a->hits[x].entry->header, e->header) == 0);
                        }
                        snprintf(ent, sizeof ent, " %s (%s)\n", e->module, e->module_desc);
                        desc = strstr(p, ent);
                        CHECK(first == (desc != NULL && desc < block_end), "%s, max_bytes %d: entry %d (%s) %s the "
                              "module description", what, max_bytes, i + 1, e->names, first ? "lacks" : "repeats");
                }
                CHECK(strstr(p, "\nMatched: ") != NULL && strstr(p, "\nMatched: ") < block_end,
                      "%s: no Matched line for %s", what, e->names);
                indented(e->decl, ind, sizeof ind);
                p = strstr(p, "\nDeclaration:\n");
                CHECK(p && strncmp(p + 14, ind, strlen(ind)) == 0, "%s, max_bytes %d: declaration of %s cut or missing",
                      what, max_bytes, e->names);
                if(p == NULL){
                        continue;
                }
                p += 14 + strlen(ind);
                if(strncmp(p, "Documentation: not shown (byte budget).\n", 40) == 0){
                        p += 40;
                }else{
                        CHECK(strncmp(p, "Documentation:\n", 15) == 0, "%s: no Documentation line for %s", what,
                              e->names);
                        p += 15;
                        indented(e->doc, ind, sizeof ind);
                        if(strncmp(p, ind, strlen(ind)) != 0){
                                /* cut: a prefix of the doc text ending in a sentence and " ..." */
                                const char *c = strstr(p, " ...\n");
                                size_t k = c ? (size_t)(c - p) : 0;
                                CHECK(c && k > 0 && p[k - 1] == '.' && strncmp(p, ind, k) == 0,
                                      "%s, max_bytes %d: doc text of %s neither whole nor cut at a sentence", what,
                                      max_bytes, e->names);
                                p = c ? c + 5 : p;
                        }else{
                                p += strlen(ind);
                        }
                }
                if(e->example == NULL){
                        CHECK(strncmp(p, "Example: none in the manual.\n", 29) == 0, "%s: %s has no example, but the "
                              "text does not say so", what, e->names);
                }else if(strncmp(p, "Example: not shown (byte budget).\n", 34) != 0){
                        indented(e->example, ind, sizeof ind);
                        CHECK(strncmp(p, "Example:\n", 9) == 0 && strncmp(p + 9, ind, strlen(ind)) == 0,
                              "%s, max_bytes %d: example of %s cut or missing", what, max_bytes, e->names);
                }
                see = strstr(p, "See also: ");
                if(see && see < block_end){
                        const char *nl = strchr(see, '\n');
                        CHECK(see[-1] == '\n' && nl &&
                              all_mentioned(see + 10, (size_t)(nl - see - 10), e->doc, bad, sizeof bad),
                              "%s: See also of %s names %s, which its doc text does not mention", what, e->names, bad);
                }
        }
        if(shown < a->n_hits){
                const char *end = ns ? strchr(ns + 1, '\n') : NULL;
                const char *more = ns ? strstr(ns, "... and ") : NULL;
                int listed = 0;
                snprintf(ent, sizeof ent, "\nENTRY %d: ", shown + 2);
                CHECK(strstr(t, ent) == NULL, "%s, max_bytes %d: entry %d is not shown, but a later one is", what,
                      max_bytes, shown + 1);
                CHECK(ns != NULL, "%s, max_bytes %d: %d of %d hits shown, no Not shown line", what, max_bytes, shown,
                      a->n_hits);
                if(ns && end){
                        const char *stop = more && more < end ? more : end;
                        int items = 0;
                        for(const char *x = ns + 26; x < stop; x++){
                                items += x[0] == ';' && x[1] == ' ';
                        }
                        listed = more && more < end ? items + atoi(more + 8) : items + 1;
                        CHECK(listed == a->n_hits - shown, "%s, max_bytes %d: %d shown, %d listed, %d hits", what,
                              max_bytes, shown, listed, a->n_hits);
                }
        }else{
                CHECK(ns == NULL, "%s, max_bytes %d: every hit shown, and a Not shown line", what, max_bytes);
        }
        CHECK(a->n_hits == 0 || (strstr(t, "\nEntries ranked by word match. Everything under ENTRY is copied exactly "
                                        "from the tld manuals.\n") != NULL),
              "%s, max_bytes %d: hits, but the head does not say the entries are exact", what, max_bytes);
}

static int ask(const char *q, int max_bytes, int top_n, tld_doc_answer **a)
{
        tld_doc_opts o = {0};
        o.max_bytes = max_bytes;
        o.top_n = top_n;
        return tld_doc_ask(q, &o, a);
}

static int first_is(const tld_doc_answer *a, const char *names)
{
        return a && a->n_hits > 0 && strcmp(a->hits[0].entry->names, names) == 0;
}

static int test_search(void)
{
        tld_doc_answer *a = NULL;
        tld_doc_answer *b = NULL;
        char stem[64];
        static const char *stems[][2] = {
                {"parse", "pars"}, {"allocation", "alloc"}, {"file", "file"}, {"tags", "tag"},
                {"class", "class"}, {"status", "status"}, {"sorting", "sort"}, {"entries", "entr"},
                {"string", "string"}, {"values", "valu"}
        };

        SECTION_MSG("search");
        for(size_t i = 0; i < sizeof stems / sizeof stems[0]; i++){
                tld__doc_stem(stems[i][0], stem);
                CHECK(strcmp(stem, stems[i][1]) == 0, "stem of %s is %s, not %s", stems[i][0], stem, stems[i][1]);
        }

        RUN(ask("tld_json_parse", 0, 0, &a));
        CHECK(first_is(a, "tld_json_parse, tld_json_parse_n"), "tld_json_parse is not the first hit");
        CHECK(strstr(a->text, "\nMatched: name \"tld_json_parse\"") != NULL, "no name match line for tld_json_parse");
        for(int i = 0; i < a->n_hits; i++){
                CHECK(i == 0 || a->hits[i].score <= a->hits[i - 1].score, "hits not best first at %d", i);
                CHECK(a->hits[i].score > 0.0, "hit %d has score %g", i, a->hits[i].score);
        }
        CHECK(a->n_hits <= 8, "%d hits for the default top_n", a->n_hits);
        tld_doc_answer_free(a);
        a = NULL;

        RUN(ask("tld_jsn_get_str", 0, 0, &a));
        CHECK(first_is(a, "tld_json_get_str, tld_json_get_num, tld_json_get_int, tld_json_get_bool, "
                       "tld_json_get_obj, tld_json_get_arr"), "the typo tld_jsn_get_str does not find the getters first");
        CHECK(strstr(a->text, "\nMatched: close to name \"tld_jsn_get_str\"") != NULL, "no close-name match line");
        tld_doc_answer_free(a);
        a = NULL;

        RUN(ask("how do I resize memory I allocated with MMALLOC", 0, 0, &a));
        CHECK(first_is(a, "MMALLOC"), "MMALLOC (an identifier without '_') is not the first hit");
        tld_doc_answer_free(a);
        a = NULL;

        RUN(ask("levenshtein distance between two strings", 0, 3, &a));
        CHECK(first_is(a, "tld_bpm_distance"), "levenshtein: tld_bpm_distance is not the first hit");
        CHECK(a->n_hits == 3, "top_n 3 gives %d hits", a->n_hits);
        tld_doc_answer_free(a);
        a = NULL;

        /* a word of 6 or more letters found nowhere: searched again with an edit */
        RUN(ask("levenstein distance between two strings", 0, 0, &a));
        CHECK(first_is(a, "tld_bpm_distance"), "levenstein: tld_bpm_distance is not the first hit");
        CHECK(strstr(a->text, " Not found: levenstein (nearest: \"Levenshtein\", 1 edit).\n") != NULL,
              "levenstein: the head does not name the nearest form");
        CHECK(strstr(a->text, "; near levenstein\n") != NULL, "levenstein: no near match on the Matched line");
        tld_doc_answer_free(a);
        a = NULL;

        /* the near search falls back to the word as written: the stem of "alocation" is
           "aloc", too short for a near search */
        RUN(ask("memory alocation", 0, 0, &a));
        check_text(a, 0, "memory alocation");
        CHECK(strstr(a->text, "\nWords found in the manuals: memory. Not found: alocation (nearest: \"allocation\", "
                     "1 edit).\n") != NULL, "memory alocation: the head does not name the nearest form allocation");
        CHECK(first_is(a, "MFREE"), "memory alocation: MFREE is not the first hit");
        {
                int mmalloc = 0;
                for(int i = 0; i < a->n_hits; i++){
                        mmalloc |= strcmp(a->hits[i].entry->names, "MMALLOC") == 0;
                }
                CHECK(mmalloc, "memory alocation: MMALLOC is not among the hits");
        }
        CHECK(strstr(a->text, "\nENTRY 3: MMALLOC\n") != NULL &&
              strstr(strstr(a->text, "\nENTRY 3: MMALLOC\n"), "\nMatched: words memory; near alocation\n") != NULL,
              "memory alocation: MMALLOC is not entry 3 with a near match");
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("sortting", 0, 0, &a));
        CHECK(strstr(a->text, "\nWords found in the manuals: none. Not found: sortting (nearest: \"sorting\", "
                     "1 edit).\n") != NULL && a->n_hits > 0 && strstr(a->text, "\nMatched: near sortting\n") != NULL,
              "sortting (stem sortt): no near search");
        tld_doc_answer_free(a);
        a = NULL;

        /* a later identifier spelling of a repeated word wins, whatever the order */
        RUN(ask("mmalloc MMALLOC", 0, 0, &a));
        RUN(ask("MMALLOC mmalloc", 0, 0, &b));
        CHECK(first_is(a, "MMALLOC") && first_is(b, "MMALLOC"), "mmalloc MMALLOC: MMALLOC is not the first hit");
        CHECK(strstr(a->text, "\nENTRY 1: MMALLOC\n") != NULL &&
              strstr(a->text, "\nMatched: name \"MMALLOC\"\n") != NULL, "mmalloc MMALLOC: no name match");
        CHECK(a->n_hits == b->n_hits && a->hits[0].score == b->hits[0].score && strchr(a->text, '\n') &&
              strchr(b->text, '\n') && strcmp(strchr(a->text + 25, '\n'), strchr(b->text + 25, '\n')) == 0,
              "mmalloc MMALLOC and MMALLOC mmalloc give different answers");
        tld_doc_answer_free(a);
        tld_doc_answer_free(b);
        a = b = NULL;

        /* a word written as a name with its case is an identifier, even of 2 letters: "OK"
           puts OK, FAIL first and "ok" (shorter than 3 letters) is dropped */
        RUN(ask("OK parse json", 0, 0, &a));
        RUN(ask("ok parse json", 0, 0, &b));
        CHECK(first_is(a, "OK, FAIL") && strstr(a->text, "\nMatched: name \"OK\"") != NULL,
              "OK parse json: OK, FAIL is not the first hit with a name match");
        CHECK(!first_is(b, "OK, FAIL") && strstr(b->text, "\nWords found in the manuals: parse, json. ") != NULL,
              "ok parse json: OK, FAIL is the first hit, or ok is searched");
        tld_doc_answer_free(a);
        tld_doc_answer_free(b);
        a = b = NULL;

        /* the nearest form is trimmed to word bytes: the near match of tld_json_get_strx
           runs over the ", " after tld_json_get_str in the names */
        RUN(ask("tld_json_get_strx", 0, 0, &a));
        check_text(a, 0, "tld_json_get_strx");
        CHECK(strstr(a->text, " Not found: tld_json_get_strx (nearest: \"tld_json_get_str\", 1 edit).\n") != NULL,
              "tld_json_get_strx: the nearest form is not tld_json_get_str");
        tld_doc_answer_free(a);
        a = NULL;
        /* a match whose first word is more than k edits away (the span "tld_json_get_str(tld",
           its word tld_json_get_str 3 edits from tld_json_get_strtld) gives no nearest form */
        RUN(ask("tld_json_get_strtld", 0, 0, &a));
        check_text(a, 0, "tld_json_get_strtld");
        CHECK(strstr(a->text, " Not found: tld_json_get_strtld.\n") != NULL,
              "tld_json_get_strtld: a nearest form with a non-word byte inside");
        tld_doc_answer_free(a);
        a = NULL;

        /* the stem comes first: "alocations" stems to "alocation", 1 edit from allocation,
           while the word as written is 2 edits away; the same for "aligments" */
        RUN(ask("memory alocations", 0, 0, &a));
        check_text(a, 0, "memory alocations");
        CHECK(strstr(a->text, "\nWords found in the manuals: memory. Not found: alocations (nearest: \"allocation\", "
                     "1 edit).\n") != NULL && first_is(a, "MFREE") &&
              strstr(a->text, "\nMatched: words memory; near alocations\n") != NULL,
              "memory alocations: no near match through the stem, or MFREE not first");
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("aligments", 0, 0, &a));
        CHECK(strstr(a->text, " Not found: aligments (nearest: \"alignment\", 1 edit).\n") != NULL &&
              first_is(a, "MMALLOC_ALIGNED"), "aligments: no near match through the stem");
        tld_doc_answer_free(a);
        a = NULL;
        /* a match that runs into the next word ("Approximate s") gives its first word, with
           that word's own distance */
        RUN(ask("approximately", 0, 0, &a));
        check_text(a, 0, "approximately");
        CHECK(strstr(a->text, " Not found: approximately (nearest: \"Approximate\", 2 edits).\n") != NULL &&
              a->n_hits > 0, "approximately: the nearest form is not Approximate, 2 edits");
        tld_doc_answer_free(a);
        a = NULL;

        RUN(ask("how do I find out the weather in paris?", 0, 0, &a));
        CHECK(strstr(a->text, "\nWords found in the manuals: find. Not found: weather, paris.\n") != NULL,
              "weather in paris: the head does not say which words are found");
        tld_doc_answer_free(a);
        a = NULL;

        RUN(ask("zzzzqqqx", 0, 0, &a));
        CHECK(a->n_hits == 0 && strstr(a->text, "\nWords found in the manuals: none. Not found: zzzzqqqx.\n"
                                       "No entry of the tld manuals matches these words.\n") != NULL,
              "a word found nowhere: %d hits, or not said", a->n_hits);
        tld_doc_answer_free(a);
        a = NULL;

        /* the same question gives the same answer; o NULL is max_bytes 4096, top_n 8 */
        RUN(ask("how do I read a JSON string?", 4096, 8, &a));
        RUN(tld_doc_ask("how do I read a JSON string?", NULL, &b));
        CHECK(strcmp(a->text, b->text) == 0, "the same question gives two texts");
        CHECK(strstr(a->text, "\nSee also: tld_json_dump\n") != NULL, "JSON: no See also line for TLD_JSON_COMPACT");
        tld_doc_answer_free(a);
        tld_doc_answer_free(b);
        a = b = NULL;

        RUN(ask("a b c", 0, 1000, &a));
        CHECK(a->n_hits == 0 && strstr(a->text, "\nThe question has no words to search for") != NULL,
              "short words only: %d hits", a->n_hits);
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("json", 0, 1000, &a));
        CHECK(a->n_hits > 8 && a->n_hits <= tld_doc_n_entries(), "top_n 1000: %d hits", a->n_hits);
        tld_doc_answer_free(a);
        a = NULL;

        /* more than 16 distinct words: the head names the words that were not searched */
        RUN(ask("alpha bravo charlie delta echo foxtrot golf hotel india juliet kilo lima mike november oscar "
                "papa quebec read a json string romeo sierra tango uniform victor whiskey", 0, 0, &a));
        CHECK(strstr(a->text, "\nNot searched: quebec, read, json, string, romeo, sierra, tango, uniform and more "
                     "(only the first 16 distinct words of a question are searched).\n") != NULL &&
              strstr(a->text, "\nNot searched: ") < a->text + head_len(a->text),
              "17 or more words: no Not searched line in the head naming the words left out");
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("alpha bravo charlie delta echo foxtrot golf hotel india juliet kilo lima mike november oscar "
                "papa Alpha the of quebec PAPA json", 0, 0, &a));
        CHECK(strstr(a->text, "\nNot searched: quebec, json (only the first 16") != NULL,
              "words left out: repeated words, stopwords or the last two not named as expected");
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("alpha bravo charlie delta echo foxtrot golf hotel india juliet kilo lima mike november oscar "
                "papa alpha", 0, 0, &a));
        CHECK(strstr(a->text, "Not searched") == NULL, "16 distinct words: a Not searched line");
        tld_doc_answer_free(a);
        return OK;
ERROR:
        tld_doc_answer_free(a);
        tld_doc_answer_free(b);
        return FAIL;
}

/* The budget for many max_bytes values, on questions with long declarations (the sort
 * family), examples, and words found nowhere. */
static int test_budget(void)
{
        static const char *qs[] = {
                "how do I read a JSON string?",
                "sort an array of doubles in descending order",
                "iterate over a hash map",
                "how do I find out the weather in paris?",
                "rich terminal output with progress",
                "tld_bpm_find"
        };
        tld_doc_answer *a = NULL;
        int runs = 0;
        SECTION_MSG("text budget");
        for(size_t i = 0; i < sizeof qs / sizeof qs[0]; i++){
                for(int mb = 1; mb <= 7000; mb += mb < 400 ? 13 : mb < 3000 ? 37 : 211){
                        RUN(ask(qs[i], mb, 8, &a));
                        check_text(a, mb, qs[i]);
                        tld_doc_answer_free(a);
                        a = NULL;
                        runs++;
                }
                RUN(ask(qs[i], 0, 8, &a));
                check_text(a, 0, qs[i]);
                tld_doc_answer_free(a);
                a = NULL;
                RUN(ask(qs[i], 100000, 171, &a));
                check_text(a, 100000, qs[i]);
                CHECK(strstr(a->text, "Not shown") == NULL, "%s: entries not shown with a budget of 100000", qs[i]);
                tld_doc_answer_free(a);
                a = NULL;
                runs += 2;
        }
        KV_MSG("budgets checked", "%d texts", runs);
        return OK;
ERROR:
        tld_doc_answer_free(a);
        return FAIL;
}

static int test_edges(void)
{
        static char big[200000];
        tld_doc_answer *a = NULL;
        tld_doc_opts o = {0};
        size_t n = 0;
        static const char *odd[] = {
                "", "how do I do it", "?!?", "____", "_", "\t\n\r\v", "{{}} [] () <>",
                "caf\xc3\xa9 json \xff\xfe\x80 string", "\xe2\x82", "json\x01\x02string\x7f",
                "TLD_JSON_PARSE", "Tld_Json_Parse", "ai_text ai_text ai_text"
        };
        static const char *utf8[][2] = {
                /* valid: 2, 3 and 4 bytes, U+D7FF, U+E000, U+10FFFF */
                {"json caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80", "json caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80"},
                {"json \xed\x9f\xbf \xee\x80\x80 \xf4\x8f\xbf\xbf", "json \xed\x9f\xbf \xee\x80\x80 \xf4\x8f\xbf\xbf"},
                /* overlong 3 and 4 bytes, a surrogate, above U+10FFFF, overlong 2 bytes, F5 */
                {"json \xe0\x80\x80 x \xed\xa0\x80 y \xf4\x90\x80\x80 z \xc0\xaf \xf5\x80",
                 "json ??? x ??? y ???? z ?? ??"},
                {"json \xf0\x80\x80\x80 \xe0\x9f\xbf", "json ???? ???"},
                /* cut short, and a lead byte followed by ASCII */
                {"json \xf0\x9f\x98", "json ???"}, {"json \xe2\x82x", "json ??x"}
        };
        SECTION_MSG("edge cases");
        for(size_t i = 0; i < sizeof odd / sizeof odd[0]; i++){
                RUN(ask(odd[i], 0, 0, &a));
                check_text(a, 0, "odd input");
                for(const unsigned char *p = (const unsigned char *)a->text; *p; p++){
                        CHECK(*p >= 0x20 || *p == '\n', "control byte 0x%02x in the text for odd input %zu", *p, i);
                }
                tld_doc_answer_free(a);
                a = NULL;
        }
        RUN(ask("", 0, 0, &a));
        CHECK(a->n_hits == 0 && strstr(a->text, "no words to search for") != NULL, "empty question: not no words");
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("how do I do it", 0, 0, &a));
        CHECK(a->n_hits == 0 && strstr(a->text, "no words to search for") != NULL, "only stopwords: %d hits",
              a->n_hits);
        tld_doc_answer_free(a);
        a = NULL;
        RUN(ask("TLD_JSON_PARSE", 0, 0, &a));
        CHECK(first_is(a, "tld_json_parse, tld_json_parse_n"), "an identifier in capitals does not find its entry");
        tld_doc_answer_free(a);
        a = NULL;

        /* the Question line: only well-formed UTF-8 is kept (RFC 3629: no overlong form, no
           surrogate, nothing above U+10FFFF), every other byte is '?' */
        for(size_t i = 0; i < sizeof utf8 / sizeof utf8[0]; i++){
                char want[256];
                snprintf(want, sizeof want, "\nQuestion: %s\n", utf8[i][1]);
                RUN(ask(utf8[i][0], 0, 0, &a));
                CHECK(strstr(a->text, want) != NULL, "UTF-8 case %zu: the Question line is not \"%s\"", i, utf8[i][1]);
                tld_doc_answer_free(a);
                a = NULL;
        }
        /* the question is cut after 200 bytes: 200 bytes stay whole, 201 give 200 and " ..." */
        for(int len = 198; len <= 206; len++){
                char qs[256];
                const char *line;
                const char *nl;
                for(int i = 0; i < len; i++){
                        qs[i] = i % 2 ? ' ' : 'x';
                }
                qs[len] = 0;
                RUN(ask(qs, 0, 0, &a));
                line = strstr(a->text, "\nQuestion: ");
                nl = line ? strchr(line + 11, '\n') : NULL;
                CHECK(nl && (len <= 200 ? nl - (line + 11) == len :
                             nl - (line + 11) == 204 && strncmp(nl - 4, " ...", 4) == 0 &&
                             strncmp(line + 11, qs, 200) == 0),
                      "a question of %d bytes: Question line of %d bytes", len, nl ? (int)(nl - (line + 11)) : -1);
                tld_doc_answer_free(a);
                a = NULL;
        }

        /* a very long question: the head stays bounded */
        while(n + 40 < sizeof big){
                n += (size_t)snprintf(big + n, sizeof big - n, "parse json string number %zu ", n);
        }
        RUN(ask(big, 0, 0, &a));
        check_text(a, 0, "a very long question");
        CHECK(strstr(a->text, " ...\nWords found in the manuals: ") != NULL, "a long question is not cut with \" ...\"");
        tld_doc_answer_free(a);
        a = NULL;
        /* the longest head: 16 long words that are missing but have near forms, and more
           than 8 long words left out */
        snprintf(big, sizeof big, "%s",
                 "substitutionsqq insertionsqq deletionsqq levenshteinqq concurrentlyqq precisionqq "
                 "probabilityqq distributionqq standardiseqq normalisationqq covarianceqq correlationqq "
                 "embeddingsqq dimensionsqq alignmentqq temperatureqq zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz "
                 "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
                 "wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv "
                 "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu tttttttttttttttttttttttttttttttttttt "
                 "ssssssssssssssssssssssssssssssssssss rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr "
                 "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq");
        RUN(ask(big, 0, 0, &a));
        check_text(a, 0, "16 missing words");
        CHECK(strstr(a->text, " and more (only the first 16 distinct words") != NULL, "no Not searched line");
        KV_MSG("head, 16 missing words", "%zu bytes", head_len(a->text));
        tld_doc_answer_free(a);
        a = NULL;
        /* the longest head this corpus gives: 16 distinct words of 26 bytes that each have
           the longest nearest form (a nearest form is one word of the corpus, and the
           longest, tld_thread_pool_stop_all, has 24 bytes), 9 more words of 32 bytes, a
           question cut after 200 bytes, and max_bytes 1 */
        {
                static const char *const end[] = {"qq", "qx", "qz", "xq", "xx", "xz", "zq", "zx",
                                                  "zz", "jq", "jx", "jz", "kq", "kx", "kz", "jj"};
                const char *p;
                int n_near = 0;
                n = 0;
                for(int i = 0; i < 16; i++){
                        n += (size_t)snprintf(big + n, sizeof big - n, "tld_thread_pool_stop_all%s ", end[i]);
                }
                for(int i = 0; i < 9; i++){
                        memset(big + n, 'z' - i, 32);
                        n += 32;
                        big[n++] = ' ';
                }
                big[n] = 0;
                RUN(ask(big, 1, 0, &a));
                check_text(a, 1, "the longest head");
                for(p = a->text; (p = strstr(p, " (nearest: \"tld_thread_pool_stop_all\", 2 edits)")) != NULL; p++){
                        n_near += p < a->text + head_len(a->text);
                }
                CHECK(n_near == 16 && strstr(a->text, " ...\nWords found in the manuals: none. Not found: ") != NULL &&
                      strstr(a->text, " and more (only the first 16 distinct words") != NULL,
                      "the longest head: %d of 16 nearest forms, or the question not cut, or no Not searched line",
                      n_near);
                KV_MSG("head, the longest", "%zu bytes (at most 2300)", head_len(a->text));
                tld_doc_answer_free(a);
                a = NULL;
        }

        /* bad arguments: FAIL with *out NULL */
        CHECK(tld_doc_ask(NULL, NULL, &a) == FAIL && a == NULL, "a NULL question is not FAIL");
        CHECK(tld_doc_ask("json", NULL, NULL) == FAIL, "a NULL out is not FAIL");
        o.max_bytes = -1;
        CHECK(tld_doc_ask("json", &o, &a) == FAIL && a == NULL, "max_bytes -1 is not FAIL");
        o.max_bytes = 0;
        o.top_n = -1;
        CHECK(tld_doc_ask("json", &o, &a) == FAIL && a == NULL, "top_n -1 is not FAIL");
        RUN(tld_doc_ask("json", NULL, &a));
        CHECK(a != NULL && a->text != NULL, "o NULL gives no answer");
        tld_doc_answer_free(a);
        return OK;
ERROR:
        tld_doc_answer_free(a);
        return FAIL;
}

int main(int argc, char *argv[])
{
        const char *root = TLD_DOC_TEST_ROOT;
        test_init(&argc, argv, "[ROOT]",
                  "Checks the corpus of tld_doc.h against ROOT/docs/tld.md, ROOT/docs/tld_ai.md,\n"
                  "ROOT/tld.h and ROOT/tld_ai.h (ROOT: the repository root; default: the\n"
                  "source tree the program was built from), the corpus API, the search and the\n"
                  "answer text.");
        if(argc > 1){
                root = argv[1];
        }
        if(test_corpus(root) != OK){
                failed++;
        }
        if(test_search() != OK || test_budget() != OK || test_edges() != OK){
                failed++;
        }
        if(failed){
                fprintf(stderr, "unit-doc: %d check%s failed\n", failed, failed == 1 ? "" : "s");
                return EXIT_FAILURE;
        }
        OK_MSG("unit-doc: all checks passed");
        return EXIT_SUCCESS;
}

/* doc-eval: tld_doc.h over the evaluation set tests/doc/questions.tsv. Built from the
 * single headers ./tld_doc.h and ./tld.h.
 *
 * Each line of the set is: id, kind, question, expected, note (tab-separated; '#'
 * starts a comment line). For each covered and partial question it finds the rank of
 * the first hit that has one of the expected names (comma-separated) and prints:
 *   top 1, top 3, top 8   the questions whose first expected hit has rank <= 1, 3, 8
 *   shown                 the questions whose expected entry the text at the default
 *                         budget (4096 bytes) shows as an ENTRY block, not only by name
 *   MRR                   the mean of 1 / rank (0 when no expected entry is in the top 8)
 * For off questions (tld does not cover them) there is nothing to rank; it prints how
 * many answers say that a word of the question is not in the manuals ("Not found:"
 * with a word) and how many have no hits at all.
 *
 * A covered or partial question fails when no expected entry is among the first 8 hits,
 * unless its note starts with "miss:" (a known miss, with the reason). The program
 * fails on an unmarked miss, when more than 20% of the covered and partial questions
 * are marked, or when an expected name is not a name in the corpus (so a renamed entry
 * cannot turn a question into a permanent known miss). A marked question that passes
 * is reported so its mark can be removed. With -v every question is printed: its id,
 * kind, rank (or MISS) and text, the expected names, then the first 8 hits, one per line
 * (rank, score, names; '>' marks a hit with an expected name). Off questions show the
 * "Not found:" line of the answer instead (FOUND: no word is missing). Colour is used
 * when stdout and stderr are terminals.
 *
 *   doc-eval [-v] [FILE]   FILE: the set (default: tests/doc/questions.tsv of the source tree)
 */
#define TLD_IMPLEMENTATION
#define TLD_DOC_IMPLEMENTATION
#include "tld_doc.h"
#include "test.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef TLD_DOC_TEST_ROOT
#define TLD_DOC_TEST_ROOT "."
#endif

#define TOP 8

/* Is name one of the ", "-separated names of e? */
static int has_name(const tld_doc_entry *e, const char *name, size_t n)
{
        const char *s = e->names;
        while(*s){
                const char *c = strstr(s, ", ");
                size_t len = c ? (size_t)(c - s) : strlen(s);
                if(len == n && memcmp(s, name, n) == 0){
                        return 1;
                }
                s += len + (c ? 2 : 0);
        }
        return 0;
}

/* Does e have one of the comma-separated names of expected? */
static int is_expected(const tld_doc_entry *e, const char *expected)
{
        const char *s = expected;
        while(*s){
                const char *c = strchr(s, ',');
                size_t len = c ? (size_t)(c - s) : strlen(s);
                if(has_name(e, s, len)){
                        return 1;
                }
                s += len + (c ? 1 : 0);
        }
        return 0;
}

/* The rank (1 ...) of the first hit of a with an expected name, 0 when none. */
static int expected_rank(const tld_doc_answer *a, const char *expected)
{
        for(int i = 0; i < a->n_hits; i++){
                if(is_expected(a->hits[i].entry, expected)){
                        return i + 1;
                }
        }
        return 0;
}

/* Is each comma-separated name of expected a name of a corpus record? The first that is
 * not goes to *bad (n bytes in *bad_n). */
static int expected_known(const char *expected, const char **bad, size_t *bad_n)
{
        const char *s = expected;
        while(*s){
                const char *c = strchr(s, ',');
                size_t len = c ? (size_t)(c - s) : strlen(s);
                int known = 0;
                for(int r = 0; r < tld_doc_n_entries() && !known; r++){
                        known = has_name(tld_doc_entry_at(r), s, len);
                }
                if(!known){
                        *bad = s;
                        *bad_n = len;
                        return 0;
                }
                s += len + (c ? 1 : 0);
        }
        return 1;
}

/* Is hit i (an expected one) shown as an ENTRY or SECTION block in the text of a
 * ("ENTRY n: " or "SECTION n: " and its names, a line of their own)? */
static int shown_as_entry(const tld_doc_answer *a, int i)
{
        char line[8192];
        snprintf(line, sizeof line, "\n%s %d: %s\n", a->hits[i].entry->kind == TLD_DOC_SECTION ? "SECTION" : "ENTRY",
                 i + 1, a->hits[i].entry->names);
        return strstr(a->text, line) != NULL;
}

/* Split line (changed in place) into at most max tab-separated fields. */
static int split(char *line, char **f, int max)
{
        int n = 0;
        char *s = line;
        while(n < max){
                char *t = strchr(s, '\t');
                f[n++] = s;
                if(t == NULL){
                        break;
                }
                *t = 0;
                s = t + 1;
        }
        return n;
}

#define WIDTH 100 /* the names of a hit wrap at this column */

static int colour; /* stdout is a terminal that gets colour */

static const char *col(const char *code)
{
        return colour ? code : "";
}

#define C_RED col("\033[31m")
#define C_GREEN col("\033[32m")
#define C_YELLOW col("\033[33m")
#define C_BOLD col("\033[1m")
#define C_DIM col("\033[2m")
#define C_OFF col("\033[0m")

/* Is the name s (n bytes) one of the comma-separated names of expected? */
static int name_expected(const char *s, size_t n, const char *expected)
{
        while(*expected){
                const char *c = strchr(expected, ',');
                size_t len = c ? (size_t)(c - expected) : strlen(expected);
                if(len == n && memcmp(s, expected, n) == 0){
                        return 1;
                }
                expected += len + (c ? 1 : 0);
        }
        return 0;
}

/* The comma-separated list (sep: "," or ", ") as "a, b, c" from column indent on,
 * wrapped at WIDTH with the next lines indented to the same column. The names that
 * are in expected (NULL: none) are green. */
static void print_names(const char *list, const char *sep, int indent, const char *expected)
{
        size_t sep_n = strlen(sep);
        int at = indent;
        const char *s = list;
        while(*s){
                const char *c = strstr(s, sep);
                size_t len = c ? (size_t)(c - s) : strlen(s);
                int hit = expected && name_expected(s, len, expected);
                if(s != list){
                        if(at + 2 + (int)len + (c ? 1 : 0) > WIDTH){
                                printf(",\n%*s", indent, "");
                                at = indent;
                        }else{
                                printf(", ");
                                at += 2;
                        }
                }
                printf("%s%.*s%s", hit ? C_GREEN : "", (int)len, s, hit ? C_OFF : "");
                at += (int)len;
                s += len + (c ? sep_n : 0);
        }
        printf("\n");
}

/* text from column indent on, wrapped at WIDTH between words, the next lines indented
 * to the same column. */
static void print_wrapped(const char *text, int indent)
{
        int at = indent;
        const char *s = text;
        while(*s){
                size_t len = strcspn(s, " ");
                if(s != text){
                        if(at + 1 + (int)len > WIDTH){
                                printf("\n%*s", indent, "");
                                at = indent;
                        }else{
                                printf(" ");
                                at++;
                        }
                }
                printf("%.*s", (int)len, s);
                at += (int)len;
                s += len;
                s += strspn(s, " ");
        }
}

/* The first TOP hits of a, one per line: rank, score, names; '>' marks the hits with a
 * name in expected (NULL: none). */
static void print_hits(const tld_doc_answer *a, const char *expected)
{
        if(a->n_hits == 0){
                printf("     %sno hits%s\n", C_DIM, C_OFF);
        }
        for(int i = 0; i < a->n_hits && i < TOP; i++){
                int hit = expected && is_expected(a->hits[i].entry, expected);
                printf("   %s%c%s %2d %s%8.2f%s  ", C_GREEN, hit ? '>' : ' ', C_OFF, i + 1, C_DIM, a->hits[i].score,
                       C_OFF);
                print_names(a->hits[i].entry->names, ", ", 18, expected);
        }
}

int main(int argc, char *argv[])
{
        static const char *kinds[3] = {"covered", "partial", "off"};
        char path[4096];
        tld_strbuf *set = NULL;
        tld_doc_answer *a = NULL;
        tld_doc_opts o = {0};
        int total[3] = {0, 0, 0};
        int top1[2] = {0, 0};
        int top3[2] = {0, 0};
        int top8[2] = {0, 0};
        int shown[2] = {0, 0};
        double rr[2] = {0.0, 0.0};
        int off_missing = 0;
        int off_empty = 0;
        int n_marked = 0;
        int unmarked = 0;
        int stale = 0;
        int verbose;
        char *s;

        test_init(&argc, argv, "[FILE]",
                  "Runs tld_doc.h on every question of FILE (default: tests/doc/questions.tsv\n"
                  "of the source tree), prints the retrieval figures and fails on an unmarked miss.");
        verbose = getenv("TLD_VERBOSE") && atoi(getenv("TLD_VERBOSE")) > 0;
        colour = tld_term_has_color() && isatty(fileno(stdout));
        snprintf(path, sizeof path, "%s/tests/doc/questions.tsv", TLD_DOC_TEST_ROOT);
        RUN(tld_path_read(argc > 1 ? argv[1] : path, &set));
        o.top_n = TOP;

        s = TLD_STR(set);
        while(*s){
                char *nl = strchr(s, '\n');
                char *f[5];
                int nf;
                int k;
                if(nl){
                        *nl = 0;
                }
                if(*s == 0 || *s == '#'){
                        s = nl ? nl + 1 : s + strlen(s);
                        continue;
                }
                nf = split(s, f, 5);
                ASSERT(nf >= 4, "Line \"%s\": fewer than 4 fields", s);
                for(k = 0; k < 3 && strcmp(f[1], kinds[k]) != 0; k++){
                }
                ASSERT(k < 3, "Question %s: unknown kind %s", f[0], f[1]);
                total[k]++;
                RUN(tld_doc_ask(f[2], &o, &a));
                if(k < 2){
                        const char *bad = NULL;
                        size_t bad_n = 0;
                        int marked = nf == 5 && strncmp(f[4], "miss:", 5) == 0;
                        int rank;
                        ASSERT(expected_known(f[3], &bad, &bad_n), "Question %s: expected %.*s is not a name in "
                               "the corpus", f[0], (int)bad_n, bad);
                        rank = expected_rank(a, f[3]);
                        top1[k] += rank == 1;
                        top3[k] += rank >= 1 && rank <= 3;
                        top8[k] += rank >= 1;
                        rr[k] += rank ? 1.0 / rank : 0.0;
                        shown[k] += rank && shown_as_entry(a, rank - 1);
                        n_marked += marked;
                        if(!rank || marked || verbose){
                                char status[16];
                                snprintf(status, sizeof status, rank ? "rank %d" : "MISS", rank);
                                printf("%s%-4s%s %-7s  %s%-6s%s  %s%s%s\n", C_BOLD, f[0], C_OFF, f[1],
                                       !rank ? marked ? C_YELLOW : C_RED : rank == 1 ? C_GREEN : "", status, C_OFF,
                                       C_BOLD, f[2], C_OFF);
                                printf("     expected     ");
                                print_names(f[3], ",", 18, NULL);
                                print_hits(a, f[3]);
                                if(marked && !rank){
                                        printf("     %sknown ", C_YELLOW);
                                        print_wrapped(f[4], 11);
                                        printf("%s\n", C_OFF);
                                }else if(marked){
                                        printf("     %smarked as a known miss, but it passes: remove the mark%s\n",
                                               C_YELLOW, C_OFF);
                                }
                                printf("\n");
                        }
                        unmarked += !rank && !marked;
                        stale += rank && marked;
                }else{
                        const char *nf_line = strstr(a->text, "Not found:");
                        int missing = nf_line && strncmp(nf_line, "Not found: none.", 16) != 0;
                        off_missing += missing;
                        off_empty += a->n_hits == 0;
                        if(verbose){
                                printf("%s%-4s%s %-7s  %s%-6s%s  %s%s%s\n", C_BOLD, f[0], C_OFF, f[1],
                                       missing ? "" : C_YELLOW, missing ? "" : "FOUND", C_OFF, C_BOLD, f[2], C_OFF);
                                if(nf_line){
                                        const char *e = strchr(nf_line, '\n');
                                        printf("     %.*s\n", e ? (int)(e - nf_line) : (int)strlen(nf_line), nf_line);
                                }
                                print_hits(a, NULL);
                                printf("\n");
                        }
                }
                tld_doc_answer_free(a);
                a = NULL;
                s = nl ? nl + 1 : s + strlen(s);
        }
        printf("%-8s %5s %5s %5s %6s %5s\n", "", "top 1", "top 3", "top 8", "shown", "MRR");
        for(int k = 0; k < 2; k++){
                printf("%-8s %2d/%-2d %2d/%-2d %2d/%-2d  %2d/%-2d %5.2f\n", kinds[k], top1[k], total[k], top3[k],
                       total[k], top8[k], total[k], shown[k], total[k], total[k] ? rr[k] / total[k] : 0.0);
        }
        printf("off      %d of %d answers name a word that is not in the manuals; %d have no hits\n",
               off_missing, total[2], off_empty);
        printf("marked misses %d of %d covered and partial questions (at most 20%%)\n", n_marked,
               total[0] + total[1]);
        if(stale){
                printf("%d marked question%s now pass%s: remove the mark\n", stale, stale == 1 ? "" : "s",
                       stale == 1 ? "es" : "");
        }
        tld_strbuf_free(set);
        if(unmarked || total[0] + total[1] == 0 || n_marked * 5 > total[0] + total[1]){
                fprintf(stderr, "doc-eval: %d unmarked miss%s%s\n", unmarked, unmarked == 1 ? "" : "es",
                        n_marked * 5 > total[0] + total[1] ? "; more than 20% of the questions are marked" : "");
                return EXIT_FAILURE;
        }
        OK_MSG("doc-eval: no unmarked miss");
        return EXIT_SUCCESS;
ERROR:
        tld_doc_answer_free(a);
        tld_strbuf_free(set);
        return EXIT_FAILURE;
}

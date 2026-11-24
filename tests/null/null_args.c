/* null-args: every public function called with NULL in each pointer argument, one call
 * per case, each in its own child process (so a crash is a finding, not the end).
 *
 *   NUL  the pointer is required: the call returns its failure value (FAIL, NULL, 0 for
 *        a 1/0 predicate, NaN for a double result, -1 for an index) and prints an
 *        ERROR line on stderr.
 *   OPT  the pointer is documented as optional: the call returns normally.
 *   FRE  a free function: NULL is ignored, silently.
 *
 * covered.sh also fails when a public function with a pointer argument has no case here.
 *
 * Built with TLD_NULL_DOC defined (null-args-doc) the program checks tld_doc.h instead:
 * it is built from the single headers ./tld_doc.h and ./tld.h and runs only the
 * DOC_CASES. Every tld_doc function has a case; those without a pointer argument get
 * their failure case (an index out of range) or a plain call. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifdef TLD_NULL_DOC
#define TLD_IMPLEMENTATION
#define TLD_DOC_IMPLEMENTATION
#include "tld_doc.h"
#else
#include "tld.h"
#endif
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum { NUL, OPT, FRE };

#ifndef TLD_NULL_DOC
/* fixtures, made in the child before its call */
static tld_strbuf *SB, *SB2, *OUTB;
static tld_hash *H, *OUTH;
static tld_json_obj *JO, *OUTO;
static tld_json_arr *JA, *OUTA;
static tld_json_val *JV, *JV2;
static tld_bitfield *BF, *BF2, *BF3;
static tld_kdtree *KD;
static tld_bpm *BP, *OUTBP;
static tld_bpm_hit HIT, *HITS;
static struct tld_rng *R, *OUTR;
static struct tld_rng_dist *D_, *OUTD;
static tld_thread_pool *TP;
static double *G;                       /* galloc'd */
static double **OUTM;
static double V[4] = {1, 2, 3, 4};
static double C[4] = {1, 1, 2, 1};
static double Y[4] = {1, 0, 1, 0};
static double YH[4] = {0.9, 0.2, 0.7, 0.4};
static double P2[2] = {0.5, 0.5};
static double *X2[2] = {V, V + 2};
static uint32_t IDS[2] = {1, 2};
static uint32_t U32;
static int32_t I32;
static uint8_t U8;
static int I;
static int64_t I64;
static double D, D2;
static const char *CS, *CS2;
static char KEY[] = "a";
static char *ARGV[] = {(char *) "prog", NULL};

static void job(tld_thread_pool *p, void *data, int64_t s, int64_t e, int tid)
{
        (void) p; (void) data; (void) s; (void) e; (void) tid;
}

static int setup(int with_pool)
{
        RUN(tld_strbuf_alloc(&SB, 16));
        RUN(tld_strbuf_append(SB, "{{a}} x"));
        RUN(tld_strbuf_alloc(&SB2, 16));
        RUN(tld_hash_add(&H, "a", "b"));
        RUN(tld_hash_iter_init(H));
        RUN(tld_json_parse("{\"a\": 1, \"l\": [1]}", &JO));
        RUN(tld_json_get_arr(JO, "l", &JA));
        RUN(tld_json_val_alloc(&JV));
        RUN(tld_json_val_alloc(&JV2));
        RUN(tld_bitfield_alloc(&BF, 64));
        RUN(tld_bitfield_alloc(&BF2, 64));
        RUN(tld_bitfield_alloc(&BF3, 64));
        RUN(tld_kdtree_alloc(&KD, 2));
        RUN(tld_kdtree_insert(KD, P2, 1));
        RUN(tld_bpm_alloc(&BP, "ab", 2, 0));
        RUN(tld_rng_alloc(&R, 1));
        RUN(tld_rng_dist_alloc(&D_, V, 4));
        galloc(&G, 4);
        if(with_pool){
                RUN(tld_thread_pool_alloc(&TP, 0, 1));
        }
        return OK;
ERROR:
        return FAIL;
}
#else
/* tld_doc.h fixtures: the answer, the options and a buffer for the manual */
static tld_doc_answer *DA;
static tld_doc_opts DOPT = {.max_bytes = 1024};
static tld_strbuf *SB;

static int setup(int with_pool)
{
        (void) with_pool;
        RUN(tld_strbuf_alloc(&SB, 16));
        return OK;
ERROR:
        return FAIL;
}
#endif

#define FAILS(call) ((call) == FAIL)
#define NONE(call) ((call), 1)

/* X(kind, uses a pool, expression: 1 when the call returned its failure value) */
#define CASES(X) \
        X(NUL, 0, FAILS(tld_galloc_dim1(NULL, &U32))) \
        X(NUL, 0, FAILS(tld_galloc_dim1(G, NULL))) \
        X(NUL, 0, FAILS(tld_galloc_dim2(NULL, &U32))) \
        X(NUL, 0, FAILS(tld_galloc_dim2(G, NULL))) \
        X(FRE, 0, NONE(tld__gfree(NULL))) \
        \
        X(NUL, 0, FAILS(tld_bitfield_alloc(NULL, 64))) \
        X(FRE, 0, NONE(tld_bitfield_free(NULL))) \
        X(NUL, 0, FAILS(tld_bitfield_reset(NULL))) \
        X(NUL, 0, FAILS(tld_bitfield_set(NULL, 1))) \
        X(NUL, 0, FAILS(tld_bitfield_get(NULL, 1, &U8))) \
        X(NUL, 0, FAILS(tld_bitfield_get(BF, 1, NULL))) \
        X(NUL, 0, FAILS(tld_bitfield_clear(NULL, 1))) \
        X(NUL, 0, FAILS(tld_bitfield_copy(NULL, BF))) \
        X(NUL, 0, FAILS(tld_bitfield_copy(BF, NULL))) \
        X(NUL, 0, FAILS(tld_bitfield_or(NULL, BF2, BF3))) \
        X(NUL, 0, FAILS(tld_bitfield_or(BF, NULL, BF3))) \
        X(NUL, 0, FAILS(tld_bitfield_or(BF, BF2, NULL))) \
        X(NUL, 0, FAILS(tld_bitfield_hamming(NULL, BF2, NULL, &I32))) \
        X(NUL, 0, FAILS(tld_bitfield_hamming(BF, NULL, NULL, &I32))) \
        X(OPT, 0, NONE(tld_bitfield_hamming(BF, BF2, NULL, &I32))) \
        X(NUL, 0, FAILS(tld_bitfield_hamming(BF, BF2, NULL, NULL))) \
        \
        X(OPT, 0, NONE(tld_progress(1, 1, NULL))) \
        \
        X(NUL, 0, FAILS(tld_hash_alloc(NULL, 16))) \
        X(FRE, 0, NONE(tld_hash_free(NULL))) \
        X(NUL, 0, FAILS(tld_hash_add(NULL, "k", "v"))) \
        X(NUL, 0, FAILS(tld_hash_add(&H, NULL, "v"))) \
        X(NUL, 0, FAILS(tld_hash_add(&H, "k", NULL))) \
        X(NUL, 0, FAILS(tld_hash_get(NULL, "a", &CS))) \
        X(NUL, 0, FAILS(tld_hash_get(H, NULL, &CS))) \
        X(NUL, 0, FAILS(tld_hash_get(H, "a", NULL))) \
        X(NUL, 0, FAILS(tld_hash_del(NULL, "a"))) \
        X(NUL, 0, FAILS(tld_hash_del(H, NULL))) \
        X(NUL, 0, FAILS(tld_hash_rename(NULL, "a", "c"))) \
        X(NUL, 0, FAILS(tld_hash_rename(H, NULL, "c"))) \
        X(NUL, 0, FAILS(tld_hash_rename(H, "a", NULL))) \
        X(NUL, 0, FAILS(tld_hash_iter_init(NULL))) \
        X(NUL, 0, FAILS(tld_hash_iter_next(NULL, &CS, &CS2))) \
        X(NUL, 0, FAILS(tld_hash_iter_next(H, NULL, &CS2))) \
        X(NUL, 0, FAILS(tld_hash_iter_next(H, &CS, NULL))) \
        \
        X(NUL, 0, FAILS(tld_json_parse(NULL, &OUTO))) \
        X(NUL, 0, FAILS(tld_json_parse("{}", NULL))) \
        X(NUL, 0, FAILS(tld_json_parse_n(NULL, 2, &OUTO))) \
        X(NUL, 0, FAILS(tld_json_parse_n("{}", 2, NULL))) \
        X(NUL, 0, FAILS(tld_json_search(NULL, KEY, &OUTO))) \
        X(NUL, 0, FAILS(tld_json_search(JO, NULL, &OUTO))) \
        X(NUL, 0, FAILS(tld_json_search(JO, KEY, NULL))) \
        X(NUL, 0, FAILS(tld_json_get_str(NULL, "a", &CS))) \
        X(NUL, 0, FAILS(tld_json_get_str(JO, NULL, &CS))) \
        X(OPT, 0, NONE(tld_json_get_str(JO, "a", NULL))) \
        X(NUL, 0, FAILS(tld_json_get_num(NULL, "a", &D))) \
        X(NUL, 0, FAILS(tld_json_get_num(JO, NULL, &D))) \
        X(OPT, 0, NONE(tld_json_get_num(JO, "a", NULL))) \
        X(NUL, 0, FAILS(tld_json_get_int(NULL, "a", &I64))) \
        X(NUL, 0, FAILS(tld_json_get_int(JO, NULL, &I64))) \
        X(OPT, 0, NONE(tld_json_get_int(JO, "a", NULL))) \
        X(NUL, 0, FAILS(tld_json_get_bool(NULL, "a", &I))) \
        X(NUL, 0, FAILS(tld_json_get_bool(JO, NULL, &I))) \
        X(OPT, 0, NONE(tld_json_get_bool(JO, "a", NULL))) \
        X(NUL, 0, FAILS(tld_json_get_obj(NULL, "", &OUTO))) \
        X(NUL, 0, FAILS(tld_json_get_obj(JO, NULL, &OUTO))) \
        X(OPT, 0, NONE(tld_json_get_obj(JO, "", NULL))) \
        X(NUL, 0, FAILS(tld_json_get_arr(NULL, "l", &OUTA))) \
        X(NUL, 0, FAILS(tld_json_get_arr(JO, NULL, &OUTA))) \
        X(OPT, 0, NONE(tld_json_get_arr(JO, "l", NULL))) \
        X(NUL, 0, FAILS(tld_json_set_val(NULL, KEY, JV))) \
        X(NUL, 0, FAILS(tld_json_set_val(JO, NULL, JV))) \
        X(NUL, 0, FAILS(tld_json_set_val(JO, KEY, NULL))) \
        X(NUL, 0, FAILS(tld_json_obj_add(NULL, TLD_JINT("a", 1)))) \
        X(NUL, 0, FAILS(tld_json_arr_add(NULL, TLD_JINT(NULL, 1)))) \
        X(OPT, 0, NONE(tld_json_dump(NULL, SB2, 0))) \
        X(NUL, 0, FAILS(tld_json_dump(JO, NULL, 0))) \
        X(OPT, 0, NONE(tld_json_dump_arr(NULL, SB2, 0))) \
        X(NUL, 0, FAILS(tld_json_dump_arr(JA, NULL, 0))) \
        X(NUL, 0, FAILS(tld_json_obj_alloc(NULL))) \
        X(FRE, 0, NONE(tld_json_obj_free(NULL))) \
        X(NUL, 0, FAILS(tld_json_obj_copy(NULL, JO))) \
        X(NUL, 0, FAILS(tld_json_obj_copy(&OUTO, NULL))) \
        X(NUL, 0, FAILS(tld_json_arr_alloc(NULL))) \
        X(FRE, 0, NONE(tld_json_arr_free(NULL))) \
        X(NUL, 0, FAILS(tld_json_arr_copy(NULL, JA))) \
        X(NUL, 0, FAILS(tld_json_arr_copy(&OUTA, NULL))) \
        X(NUL, 0, FAILS(tld_json_val_alloc(NULL))) \
        X(FRE, 0, NONE(tld_json_val_free(NULL))) \
        X(NUL, 0, FAILS(tld_json_val_copy(NULL, JV))) \
        X(NUL, 0, FAILS(tld_json_val_copy(JV2, NULL))) \
        \
        X(NUL, 0, FAILS(tld_kdtree_alloc(NULL, 2))) \
        X(FRE, 0, NONE(tld_kdtree_free(NULL))) \
        X(NUL, 0, FAILS(tld_kdtree_reset(NULL))) \
        X(NUL, 0, FAILS(tld_kdtree_build(NULL, X2, IDS, 2))) \
        X(NUL, 0, FAILS(tld_kdtree_build(KD, NULL, IDS, 2))) \
        X(NUL, 0, FAILS(tld_kdtree_build(KD, X2, NULL, 2))) \
        X(NUL, 0, FAILS(tld_kdtree_insert(NULL, P2, 1))) \
        X(NUL, 0, FAILS(tld_kdtree_insert(KD, NULL, 1))) \
        X(NUL, 0, FAILS(tld_kdtree_rebalance(NULL))) \
        X(NUL, 0, FAILS(tld_kdtree_nn_search(NULL, P2, &U32))) \
        X(NUL, 0, FAILS(tld_kdtree_nn_search(KD, NULL, &U32))) \
        X(NUL, 0, FAILS(tld_kdtree_nn_search(KD, P2, NULL))) \
        X(NUL, 0, FAILS(tld_kdtree_bf_search(NULL, P2, &U32))) \
        X(NUL, 0, FAILS(tld_kdtree_bf_search(KD, NULL, &U32))) \
        X(NUL, 0, FAILS(tld_kdtree_bf_search(KD, P2, NULL))) \
        \
        X(NUL, 0, FAILS(tld_bpm_alloc(NULL, "ab", 2, 0))) \
        X(NUL, 0, FAILS(tld_bpm_alloc(&OUTBP, NULL, 2, 0))) \
        X(FRE, 0, NONE(tld_bpm_free(NULL))) \
        X(NUL, 0, FAILS(tld_bpm_best(NULL, "ab", 2, &HIT))) \
        X(NUL, 0, FAILS(tld_bpm_best(BP, NULL, 2, &HIT))) \
        X(NUL, 0, FAILS(tld_bpm_best(BP, "ab", 2, NULL))) \
        X(NUL, 0, FAILS(tld_bpm_find(NULL, "ab", 2, 0, &HITS, &I64))) \
        X(NUL, 0, FAILS(tld_bpm_find(BP, NULL, 2, 0, &HITS, &I64))) \
        X(NUL, 0, FAILS(tld_bpm_find(BP, "ab", 2, 0, NULL, &I64))) \
        X(NUL, 0, FAILS(tld_bpm_find(BP, "ab", 2, 0, &HITS, NULL))) \
        X(NUL, 0, FAILS(tld_bpm_distance(NULL, "ab", 2, &I))) \
        X(NUL, 0, FAILS(tld_bpm_distance(BP, NULL, 2, &I))) \
        X(NUL, 0, FAILS(tld_bpm_distance(BP, "ab", 2, NULL))) \
        X(NUL, 0, (tld_bpm_kernel(NULL) == NULL)) \
        \
        X(NUL, 0, (tld_path_exists(NULL) == 0)) \
        X(NUL, 0, (tld_path_is_dir(NULL) == 0)) \
        X(NUL, 0, FAILS(tld_path_mkdir(NULL))) \
        X(NUL, 0, FAILS(tld_path_full(NULL, "/"))) \
        X(NUL, 0, FAILS(tld_path_full(&OUTB, NULL))) \
        X(NUL, 0, FAILS(tld_path_filename(NULL, &OUTB))) \
        X(NUL, 0, FAILS(tld_path_filename("/a", NULL))) \
        X(NUL, 0, FAILS(tld_path_dirname(NULL, &OUTB))) \
        X(NUL, 0, FAILS(tld_path_dirname("/a", NULL))) \
        X(NUL, 0, FAILS(tld_path_read(NULL, &OUTB))) \
        X(NUL, 0, FAILS(tld_path_read(__FILE__, NULL))) \
        X(NUL, 0, FAILS(tld_cmdline_capture(NULL, 1, ARGV))) \
        X(NUL, 0, FAILS(tld_cmdline_capture(&OUTB, 1, NULL))) \
        \
        X(NUL, 0, FAILS(tld_rng_alloc(NULL, 1))) \
        X(NUL, 0, FAILS(tld_rng_split(NULL, R))) \
        X(NUL, 0, FAILS(tld_rng_split(&OUTR, NULL))) \
        X(NUL, 0, FAILS(tld_rng_set_seed(NULL, 1))) \
        X(FRE, 0, NONE(tld_rng_free(NULL))) \
        X(NUL, 0, isnan(tld_rng_double(NULL))) \
        X(NUL, 0, isnan(tld_rng_gaussian(NULL, 0.0, 1.0))) \
        X(NUL, 0, isnan(tld_rng_gamma(NULL, 1.0, 1.0))) \
        X(NUL, 0, (tld_rng_int(NULL, 5) == -1)) \
        X(NUL, 0, FAILS(tld_rng_dist_alloc(NULL, V, 4))) \
        X(NUL, 0, FAILS(tld_rng_dist_alloc(&OUTD, NULL, 4))) \
        X(NUL, 0, (tld_rng_dist_sample(NULL, R) == -1)) \
        X(NUL, 0, (tld_rng_dist_sample(D_, NULL) == -1)) \
        X(FRE, 0, NONE(tld_rng_dist_free(NULL))) \
        \
        X(NUL, 0, FAILS(tld_auc(NULL, YH, 4, &D, &D2))) \
        X(NUL, 0, FAILS(tld_auc(Y, NULL, 4, &D, &D2))) \
        X(OPT, 0, NONE(tld_auc(Y, YH, 4, NULL, &D2))) \
        X(OPT, 0, NONE(tld_auc(Y, YH, 4, &D, NULL))) \
        X(NUL, 0, FAILS(tld_quantile_normalise(NULL, 2, 2))) \
        X(NUL, 0, FAILS(tld_shannon(NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_shannon(V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_mean(NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_mean(V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_pop_variance(NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_pop_variance(V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_sample_variance(NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_sample_variance(V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_standardise(NULL, 4, &D, &D2))) \
        X(NUL, 0, FAILS(tld_standardise(V, 4, NULL, &D2))) \
        X(NUL, 0, FAILS(tld_standardise(V, 4, &D, NULL))) \
        X(NUL, 0, FAILS(tld_sample_covariance(NULL, V, 4, &D))) \
        X(NUL, 0, FAILS(tld_sample_covariance(V, NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_sample_covariance(V, V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_pop_covariance(NULL, V, 4, &D))) \
        X(NUL, 0, FAILS(tld_pop_covariance(V, NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_pop_covariance(V, V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_pearson_corr(NULL, V, 4, &D))) \
        X(NUL, 0, FAILS(tld_pearson_corr(V, NULL, 4, &D))) \
        X(NUL, 0, FAILS(tld_pearson_corr(V, V, 4, NULL))) \
        X(NUL, 0, FAILS(tld_sigmoid(0.0, NULL))) \
        X(NUL, 0, FAILS(tld_normal_pdf(0.0, 0.0, 1.0, NULL))) \
        X(NUL, 0, FAILS(tld_kde_pdf(NULL, 4, 10, &OUTM))) \
        X(NUL, 0, FAILS(tld_kde_pdf(V, 4, 10, NULL))) \
        X(NUL, 0, FAILS(tld_kde_count_pdf(NULL, C, 4, &OUTM))) \
        X(NUL, 0, FAILS(tld_kde_count_pdf(V, NULL, 4, &OUTM))) \
        X(NUL, 0, FAILS(tld_kde_count_pdf(V, C, 4, NULL))) \
        \
        X(NUL, 0, FAILS(tld_strbuf_alloc(NULL, 8))) \
        X(FRE, 0, NONE(tld_strbuf_free(NULL))) \
        X(NUL, 0, FAILS(tld_strbuf_clear(NULL))) \
        X(NUL, 0, FAILS(tld_strbuf_copy(NULL, SB))) \
        X(NUL, 0, FAILS(tld_strbuf_copy(SB2, NULL))) \
        X(NUL, 0, FAILS(tld_strbuf_append(NULL, "x"))) \
        X(NUL, 0, FAILS(tld_strbuf_append(SB, NULL))) \
        X(NUL, 0, FAILS(tld_strbuf_append_n(NULL, "x", 1))) \
        X(NUL, 0, FAILS(tld_strbuf_append_n(SB, NULL, 1))) \
        X(OPT, 0, NONE(tld_strbuf_append_n(SB, NULL, 0))) \
        X(NUL, 0, FAILS(tld_strbuf_append_char(NULL, 'x'))) \
        X(NUL, 0, FAILS(tld_strbuf_prepend(NULL, "x"))) \
        X(NUL, 0, FAILS(tld_strbuf_prepend(SB, NULL))) \
        \
        X(NUL, 0, FAILS(tld_template_apply(NULL, H))) \
        X(NUL, 0, FAILS(tld_template_apply(SB, NULL))) \
        X(NUL, 0, FAILS(tld_template_vars(NULL, &OUTH))) \
        X(NUL, 0, FAILS(tld_template_vars(SB, NULL))) \
        \
        X(NUL, 0, FAILS(tld_thread_pool_alloc(NULL, 0, 1))) \
        X(NUL, 1, FAILS(tld_thread_pool_add(NULL, job, NULL, 0, 1))) \
        X(NUL, 1, FAILS(tld_thread_pool_add(TP, NULL, NULL, 0, 1))) \
        X(OPT, 1, NONE(tld_thread_pool_add(TP, job, NULL, 0, 1))) \
        X(OPT, 0, NONE(tld_thread_pool_wait(NULL))) \
        X(FRE, 0, NONE(tld_thread_pool_free(NULL)))

/* tld_doc.h (null-args-doc only). o NULL means every default. An index out of range is
 * tld_doc_entry_at's failure case (NULL and an ERROR line). */
#define DOC_CASES(X) \
        X(NUL, 0, FAILS(tld_doc_ask(NULL, &DOPT, &DA))) \
        X(NUL, 0, FAILS(tld_doc_ask("parse json", &DOPT, NULL))) \
        X(OPT, 0, NONE(tld_doc_ask("parse json", NULL, &DA))) \
        X(OPT, 0, NONE(tld_doc_ask("parse json", &DOPT, &DA))) \
        X(FRE, 0, NONE(tld_doc_answer_free(NULL))) \
        X(NUL, 0, FAILS(tld_doc_manual("tld.h", NULL))) \
        X(NUL, 0, FAILS(tld_doc_manual("no.h", SB))) \
        X(OPT, 0, NONE(tld_doc_manual(NULL, SB))) \
        X(NUL, 0, (tld_doc_entry_at(-1) == NULL)) \
        X(NUL, 0, (tld_doc_entry_at(tld_doc_n_entries()) == NULL)) \
        X(OPT, 0, NONE(tld_doc_n_entries())) \
        X(OPT, 0, NONE(tld_doc_corpus_id()))

#ifdef TLD_NULL_DOC
#define ALL_CASES(X) DOC_CASES(X)
#else
#define ALL_CASES(X) CASES(X)
#endif

#define TABLE(kind, pool, expr) {kind, pool, #expr},
static const struct {
        int kind;
        int pool;
        const char *text;
} cases[] = { ALL_CASES(TABLE) };
#undef TABLE

static int run_case(int i)
{
        int n = 0;
#define RUN_CASE(kind, pool, expr) if(i == n++){ return (expr) ? 1 : 0; }
        ALL_CASES(RUN_CASE)
#undef RUN_CASE
        return -1;
}

/* The module of the call in text for the -v sections: what follows the first "tld_" or
 * "ai_" up to the next '_' or '(' ("tld_bitfield_set(...)": bitfield), else "other".
 * The statistics functions share no prefix and are listed here; galloc and gfree are
 * "alloc". */
static void module_of(const char *text, char *out, size_t size)
{
        static const char *stats[] = {"auc", "quantile", "shannon", "mean", "pop", "sample", "standardise",
                                      "pearson", "sigmoid", "normal", "kde"};
        const char *t = strstr(text, "tld_");
        const char *a = strstr(text, "ai_");
        size_t n = 0;
        if(t == NULL && a == NULL){
                snprintf(out, size, "other");
                return;
        }
        if(t == NULL || (a != NULL && a < t)){
                snprintf(out, size, "ai");
                return;
        }
        t += 4;
        while(*t == '_'){
                t++;
        }
        while(t[n] && t[n] != '_' && t[n] != '(' && n + 1 < size){
                out[n] = t[n];
                n++;
        }
        out[n] = 0;
        for(size_t i = 0; i < sizeof stats / sizeof stats[0]; i++){
                if(strcmp(out, stats[i]) == 0){
                        snprintf(out, size, "stats");
                }
        }
        if(strcmp(out, "galloc") == 0 || strcmp(out, "gfree") == 0){
                snprintf(out, size, "alloc");
        }
}

/* Run case i in a child with stderr in a file; 0 when it behaved, 1 otherwise.
 * With verbose a "fails" line is a NULL that must give the failure value and an ERROR
 * line, "allowed" a NULL the function accepts, "silent" a free function given NULL. */
static int check(int i, int verbose)
{
        static const char *kinds[] = {"NUL", "OPT", "FRE"};
        char err_path[] = "/tmp/null-args-XXXXXX";
        char err[4096];
        int fd = mkstemp(err_path);
        int status = 0;
        ssize_t got = 0;
        pid_t pid;
        const char *why = NULL;
        if(fd < 0){
                perror("mkstemp");
                return 1;
        }
        unlink(err_path);
        fflush(NULL);
        pid = fork();
        if(pid < 0){
                perror("fork");
                close(fd);
                return 1;
        }
        if(pid == 0){
                int failed = 0;
                if(setup(cases[i].pool) != OK){
                        _exit(3);
                }
                fflush(stderr);
                dup2(fd, STDERR_FILENO);
                failed = run_case(i);
                fflush(stderr);
                _exit(failed ? 10 : 11);
        }
        waitpid(pid, &status, 0);
        lseek(fd, 0, SEEK_SET);
        got = read(fd, err, sizeof err - 1);
        close(fd);
        err[got > 0 ? got : 0] = 0;

        if(WIFSIGNALED(status)){
                why = "crashed";
        }else if(!WIFEXITED(status) || (WEXITSTATUS(status) != 10 && WEXITSTATUS(status) != 11)){
                why = WIFEXITED(status) && WEXITSTATUS(status) == 3 ? "fixtures failed" : "exited";
        }else if(cases[i].kind == NUL && WEXITSTATUS(status) != 10){
                why = "did not return its failure value";
        }else if(cases[i].kind == NUL && strstr(err, "ERROR") == NULL){
                why = "printed no ERROR line";
        }else if(cases[i].kind == FRE && err[0]){
                why = "printed a message";
        }
        if(why){
                fprintf(stderr, "null-args: %s %s: %s\n", kinds[cases[i].kind], cases[i].text, why);
                if(err[0]){
                        fprintf(stderr, "    stderr: %.300s\n", err);
                }
                return 1;
        }
        if(verbose){
                static const char *shown[] = {"fails", "allowed", "silent"};
                static char last[32];
                char module[32];
                module_of(cases[i].text, module, sizeof module);
                if(strcmp(module, last) != 0){
                        SECTION_MSG("%s", module);
                        snprintf(last, sizeof last, "%s", module);
                }
                KV_MSG(shown[cases[i].kind], "%s", cases[i].text);
        }
        return 0;
}

int main(int argc, char *argv[])
{
        const int n = (int) (sizeof cases / sizeof cases[0]);
        int bad = 0;
        int n_run = 0;
        const char *v = NULL;
        test_init(&argc, argv, "[TEXT]",
                  "Calls every public function with NULL in each pointer argument, one call\n"
                  "per child process: a required pointer must give the failure value and an\n"
                  "ERROR line, an optional one a normal return, a free function silence.\n"
                  "With TEXT runs only the cases whose call contains TEXT.\n"
                  "Example: null-args tld_hash_");
        v = getenv("TLD_VERBOSE");
        for(int i = 0; i < n; i++){
                if(argc > 1 && strstr(cases[i].text, argv[1]) == NULL){
                        continue;
                }
                n_run++;
                bad += check(i, v && atoi(v) > 0);
        }
        if(bad){
                fprintf(stderr, "null-args: %d of %d cases misbehaved\n", bad, n);
                return EXIT_FAILURE;
        }
        OK_MSG("%d calls behaved", n_run);
        return EXIT_SUCCESS;
}

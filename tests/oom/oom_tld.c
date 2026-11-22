/* oom-tld: allocation-failure sweep over tld.h (see oom.h). Each scenario uses one
 * module the way a program would; every allocation it makes fails in turn. */
#include "oom.h"
#define TLD_IMPLEMENTATION
#include "tld.h"

static int sc_str(void)
{
        tld_strbuf *a = NULL, *b = NULL;
        RUN(tld_strbuf_alloc(&a, 8));
        for(int i = 0; i < 12; i++){
                RUN(tld_append(a, "0123456789"));
                RUN(tld_append_char(a, '-'));
        }
        RUN(tld_prepend(a, "start:"));
        RUN(tld_strbuf_alloc(&b, 4));
        RUN(tld_strbuf_copy(b, a));
        RUN(tld_append(b, TLD_STR(b)));
        tld_strbuf_free(a); tld_strbuf_free(b);
        return OK;
ERROR:
        tld_strbuf_free(a); tld_strbuf_free(b);
        return FAIL;
}

static int sc_hash(void)
{
        tld_hash *h = NULL;
        const char *k, *v;
        char key[32], val[160];
        RUN(tld_hash_alloc(&h, 16));
        for(int i = 0; i < 120; i++){
                snprintf(key, sizeof key, "key%d", i);
                snprintf(val, sizeof val, "value %d %0*d", i, i, 0);      /* values of growing length */
                RUN(tld_hash_add(&h, key, val));
        }
        RUN(tld_hash_add(&h, "key3", "a replacement value that is much longer than the one before it, to force growth"));
        RUN(tld_hash_rename(h, "key5", "renamed"));
        RUN(tld_hash_del(h, "key7"));
        RUN(tld_hash_get(h, "renamed", &v));
        RUN(tld_hash_iter_init(h));
        while(tld_hash_iterator_next(h, &k, &v) == OK){ }
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

static int sc_hash_auto(void)           /* tld_hash_add creates the map */
{
        tld_hash *h = NULL;
        RUN(tld_hash_add(&h, "a", "1"));
        RUN(tld_hash_add(&h, "b", "2"));
        tld_hash_free(h);
        return OK;
ERROR:
        tld_hash_free(h);
        return FAIL;
}

static const char *DOC =
        "{\"name\": \"x\\ty \\u00e9 \\ud83d\\ude00\", \"n\": 3, \"big\": 12345678901234, \"f\": 0.25, \"t\": true, \"z\": null,"
        " \"list\": [1, 2.5, \"three\", [4, 5, {\"deep\": [6, 7]}], {\"k\": \"v\"}],"
        " \"obj\": {\"a\": {\"b\": {\"c\": \"d\", \"name\": \"inner\"}}, \"e\": []},"
        " \"m1\": 1, \"m2\": 2, \"m3\": 3, \"m4\": 4, \"m5\": 5, \"m6\": 6, \"m7\": 7, \"m8\": 8, \"m9\": 9, \"m10\": 10,"
        " \"m11\": 11, \"m12\": 12, \"m13\": 13, \"m14\": 14, \"m15\": 15, \"m16\": 16, \"m17\": 17, \"m18\": 18}";

static int sc_json_parse(void)
{
        tld_strbuf *b = NULL, *out = NULL;
        tld_json_obj *j = NULL, *c = NULL, *found = NULL, *sub = NULL;
        tld_json_arr *arr = NULL;
        tld_json_val *v = NULL;
        const char *s = NULL;
        double d; int64_t i64; int bo;
        RUN(tld_strbuf_alloc(&b, 64));
        RUN(tld_append(b, DOC));
        RUN(tld_json_parse(b, &j));
        RUN(tld_json_get_str(j, "name", &s));
        RUN(tld_json_get_num(j, "f", &d));
        RUN(tld_json_get_int(j, "big", &i64));
        RUN(tld_json_get_bool(j, "t", &bo));
        RUN(tld_json_get_obj(j, "obj.a.b", &sub));
        RUN(tld_json_get_arr(j, "list", &arr));
        RUN(tld_json_search(j, (char *)"name", &found));
        RUN(tld_json_val_alloc(&v));
        RUN(tld_json_val_copy(v, j->v[6]));                     /* the list */
        RUN(tld_json_set_val(j, (char *)"obj.a.copy", v));
        RUN(tld_json_set_val(j, (char *)"list[5]", v));
        RUN(tld_json_set_val(j, (char *)"n", v));
        RUN(tld_json_obj_copy(&c, j));
        RUN(tld_strbuf_alloc(&out, 16));
        RUN(tld_json_dump(c, out, 0));
        tld_json_val_free(v); tld_json_obj_free(found); tld_json_obj_free(c); tld_json_obj_free(j);
        tld_strbuf_free(b); tld_strbuf_free(out);
        return OK;
ERROR:
        tld_json_val_free(v); tld_json_obj_free(found); tld_json_obj_free(c); tld_json_obj_free(j);
        tld_strbuf_free(b); tld_strbuf_free(out);
        return FAIL;
}

static int sc_json_toparr(void)
{
        tld_strbuf *b = NULL, *out = NULL;
        tld_json_obj *j = NULL;
        RUN(tld_strbuf_alloc(&b, 64));
        RUN(tld_append(b, "[{\"a\": [1, 2, 3]}, \"x\", [[\"y\"]]]"));
        RUN(tld_json_parse(b, &j));
        RUN(tld_strbuf_alloc(&out, 16));
        RUN(tld_json_dump(j, out, 0));
        tld_json_obj_free(j); tld_strbuf_free(b); tld_strbuf_free(out);
        return OK;
ERROR:
        tld_json_obj_free(j); tld_strbuf_free(b); tld_strbuf_free(out);
        return FAIL;
}

static int sc_json_build(void)          /* the documented way: JSON_OBJ returns NULL on failure */
{
        tld_strbuf *out = NULL;
        tld_json_obj *o = JSON_OBJ(JSTR("name", "x"), JINT("n", 3), JNUM("f", 0.5), JBOOL("t", 1), JNULL("z"),
                                   JS("guess", "0.25"), JS("word", "hello"),
                                   JA("v", JSON_ARR(JNUM(NULL, 0.5), JNUM(NULL, 2), JSTR(NULL, "s"),
                                                    JO(NULL, JSON_OBJ(JSTR("in", "arr"))),
                                                    JA(NULL, JSON_ARR(JINT(NULL, 1), JINT(NULL, 2))))),
                                   JO("o", JSON_OBJ(JSTR("k", "v"), JO("deeper", JSON_OBJ(JINT("i", 1))))));
        if(o == NULL){
                return FAIL;
        }
        RUN(tld_strbuf_alloc(&out, 16));
        RUN(tld_json_dump(o, out, 0));
        tld_json_obj_free(o); tld_strbuf_free(out);
        return OK;
ERROR:
        tld_json_obj_free(o); tld_strbuf_free(out);
        return FAIL;
}

static int sc_template(void)
{
        tld_strbuf *t = NULL;
        tld_hash *m = NULL, *vars = NULL;
        RUN(tld_strbuf_alloc(&t, 16));
        RUN(tld_append(t, "Dear {{name}},{{IF vip}} thank you for {{years}} years{{IF gold}} (gold){{ELSE}} (silver){{ENDIF}}"
                          "{{ELSE}} welcome{{ENDIF}}. {{a}}{{b}}{{c}}{{d}}{{e}}{{f}}{{g}}{{h}}{{i}}{{j}}{{k}}{{l}} Bye {{name}}."));
        RUN(tld_hash_add(&m, "name", "Ada"));
        RUN(tld_hash_add(&m, "vip", "1"));
        RUN(tld_hash_add(&m, "years", "12"));
        RUN(tld_template_extract_var(t, &vars));
        RUN(tld_template_apply(t, m));
        tld_hash_free(vars); tld_hash_free(m); tld_strbuf_free(t);
        return OK;
ERROR:
        tld_hash_free(vars); tld_hash_free(m); tld_strbuf_free(t);
        return FAIL;
}

static int sc_kdtree(void)
{
        kdtree *t = NULL;
        double **x = NULL;
        uint32_t *ids = NULL;
        uint32_t id = 0;
        double q[3] = {0.5, 0.5, 0.5};
        double p[3];
        RUN(kdtree_alloc(&t, 3));
        t->rebalance_at = 40;                           /* documented as settable */
        for(int i = 0; i < 150; i++){                   /* sorted input: a degenerate tree, so it rebalances */
                p[0] = i; p[1] = i * 0.5; p[2] = -i;
                RUN(kdtree_insert(t, p, (uint32_t)i, 3));
        }
        RUN(kdtree_rebalance(t));
        RUN(kdtree_nn_search(t, q, &id));
        RUN(kdtree_reset(t));
        galloc(&x, 60, 3);
        galloc(&ids, 60);
        for(int i = 0; i < 60; i++){ x[i][0] = i % 7; x[i][1] = i % 5; x[i][2] = i; ids[i] = (uint32_t)i; }
        RUN(kdtree_build(t, x, ids, 60, 3));
        RUN(kdtree_nn_search(t, q, &id));
        gfree(x); gfree(ids); kdtree_free(t);
        return OK;
ERROR:
        gfree(x); gfree(ids); kdtree_free(t);
        return FAIL;
}

static int sc_kdtree_reuse(void)        /* a failed operation, then the tree is used again and freed */
{
        kdtree *t = NULL;
        double p[2];
        uint32_t id = 0;
        int failed = 0;
        RUN(kdtree_alloc(&t, 2));
        t->rebalance_at = 30;
        for(int i = 0; i < 100; i++){
                p[0] = i; p[1] = i;
                if(kdtree_insert(t, p, (uint32_t)i, 2) != OK){
                        failed++;                       /* out of memory: carry on with the tree */
                }
        }
        p[0] = 3; p[1] = 3;
        (void)kdtree_nn_search(t, p, &id);
        kdtree_free(t);
        return failed ? FAIL : OK;
ERROR:
        kdtree_free(t);
        return FAIL;
}

static int sc_kdtree_grow(void)         /* growing past the initial 1024 nodes; failures are carried on from */
{
        kdtree *t = NULL;
        double **x = NULL;
        uint32_t *ids = NULL;
        double p[2];
        uint32_t id = 0, id_bf = 0;
        int failed = 0;
        RUN(kdtree_alloc(&t, 2));
        for(int i = 0; i < 1100; i++){
                p[0] = (i * 37) % 101; p[1] = i % 13;
                if(kdtree_insert(t, p, (uint32_t)i, 2) != OK) failed++;
        }
        galloc(&x, 900, 2);
        galloc(&ids, 900);
        for(int i = 0; i < 900; i++){ x[i][0] = i % 30 + 0.5; x[i][1] = i / 30 + 0.25; ids[i] = (uint32_t)(5000 + i); }
        if(kdtree_build(t, x, ids, 900, 2) != OK) failed++;
        p[0] = 3.3; p[1] = 4.4;
        if(kdtree_nn_search(t, p, &id) == OK && kdtree_bf_search(t, p, &id_bf) == OK && id != id_bf){
                /* a wrong answer must not hide behind the expected failure: crash */
                fprintf(stderr, "kdtree_grow: nn %u != bf %u after failures\n", id, id_bf);
                abort();
        }
        gfree(x); gfree(ids); kdtree_free(t);
        return failed ? FAIL : OK;
ERROR:
        gfree(x); gfree(ids); kdtree_free(t);
        return FAIL;
}

static int sc_rng(void)
{
        struct rng_state *r = NULL, *r2 = NULL;
        struct rng_dist *d = NULL;
        double w[5] = {1, 2, 3, 4, 5};
        RUN(init_rng(&r, 42));
        RUN(init_rng_from_rng(&r2, r));
        RUN(tl_random_sdist_init(&d, w, 5, 7));
        (void)tl_random_sdist_smpl(d);
        tl_random_sdist_free(d); free_rng(r); free_rng(r2);
        return OK;
ERROR:
        tl_random_sdist_free(d); free_rng(r); free_rng(r2);
        return FAIL;
}

static int sc_stats(void)
{
        double data[40], cnt[40], y[40], yh[40];
        double **m = NULL, **kde = NULL, **kde2 = NULL;
        double auc, thr;
        for(int i = 0; i < 40; i++){ data[i] = (i * 37) % 11 + i * 0.01; cnt[i] = 1 + i % 3; y[i] = i % 2; yh[i] = (i % 2) * 0.5 + (i % 7) * 0.1; }
        RUN(tld_kde_pdf(data, 40, 16, &kde));
        RUN(tld_kde_count_pdf(data, cnt, 40, &kde2));
        galloc(&m, 12, 3);
        for(int i = 0; i < 12; i++){ m[i][0] = i; m[i][1] = (i * 5) % 12; m[i][2] = 12 - i; }
        RUN(norm_quantile(m, 12, 3));
        RUN(tld_auc(y, yh, 40, &thr, &auc));
        gfree(m); gfree(kde); gfree(kde2);
        return OK;
ERROR:
        gfree(m); gfree(kde); gfree(kde2);
        return FAIL;
}

static int sc_misc(void)
{
        char path[] = "/usr/lib/libexample.dylib";
        char *argv[] = {(char *)"prog", (char *)"-x", (char *)"file name", NULL};
        char *f = NULL, *d = NULL;
        tld_strbuf *cmd = NULL, *full = NULL;
        tl_bitfield *b1 = NULL, *b2 = NULL, *b3 = NULL;
        int32_t dist;
        RUN(tld_get_filename(path, &f));
        RUN(tld_get_dirname(path, &d));
        RUN(tld_capture_cmd_line(&cmd, 3, argv));
        RUN(tl_bitfield_alloc(&b1, 300));
        RUN(tl_bitfield_alloc(&b2, 300));
        RUN(tl_bitfield_alloc(&b3, 300));
        RUN(tl_bitfield_set(b1, 17));
        RUN(tl_bitfield_or(b1, b2, b3));
        RUN(tl_bitfield_hamming(b1, b2, NULL, &dist));
        MFREE(f); MFREE(d); tld_strbuf_free(cmd); tld_strbuf_free(full);
        tl_bitfield_free(b1); tl_bitfield_free(b2); tl_bitfield_free(b3);
        return OK;
ERROR:
        MFREE(f); MFREE(d); tld_strbuf_free(cmd); tld_strbuf_free(full);
        tl_bitfield_free(b1); tl_bitfield_free(b2); tl_bitfield_free(b3);
        return FAIL;
}

static atomic_int jobs_done;
static void job(tld_thread_pool *p, void *data, int64_t s, int64_t e, int tid)
{
        (void)p; (void)data; (void)s; (void)e; (void)tid;
        atomic_fetch_add(&jobs_done, 1);
}
static int sc_tpool(void)
{
        tld_thread_pool *p = NULL;
        int refused = 0;
        RUN(tld_thread_pool_create(&p, 0, 3));
        for(int i = 0; i < 40; i++){
                if(tld_thread_pool_add(p, job, NULL, i, i + 1) != OK) refused++;
        }
        (void)tld_thread_pool_wait(p);
        tld_thread_pool_free(p);
        return refused ? FAIL : OK;
ERROR:
        tld_thread_pool_free(p);
        return FAIL;
}

static const struct oom_scenario scenarios[] = {
        {"str", sc_str, "string buffer append, prepend, copy", NULL},
        {"hash", sc_hash, "hash add (growing values), replace, rename, delete, iterate", NULL},
        {"hash_auto", sc_hash_auto, "tld_hash_add creating the map", NULL},
        {"json_parse", sc_json_parse, "parse, getters, search, set_val, copy, dump", NULL},
        {"json_toparr", sc_json_toparr, "a top-level array parsed and dumped", NULL},
        {"json_build", sc_json_build, "JSON_OBJ / JSON_ARR with nested containers", NULL},
        {"template", sc_template, "extract variables and apply nested IF/ELSE", NULL},
        {"kdtree", sc_kdtree, "inserts with rebalancing, reset, build, search", NULL},
        {"kdtree_reuse", sc_kdtree_reuse, "inserts that fail, then the tree is searched and freed", NULL},
        {"kdtree_grow", sc_kdtree_grow, "growing past the initial 1024 nodes; nn agrees with bf", NULL},
        {"rng", sc_rng, "generators and a sampling distribution", NULL},
        {"stats", sc_stats, "kde, quantile normalisation, auc", NULL},
        {"misc", sc_misc, "file names, command line, bitfields", NULL},
        {"tpool", sc_tpool, "thread pool with 40 jobs", NULL},
};

int main(int argc, char *argv[])
{
        return oom_main(argc, argv, scenarios, (int) (sizeof scenarios / sizeof scenarios[0]), "tld.h");
}

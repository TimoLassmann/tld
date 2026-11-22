/* tests/migrate: old.c is code written for the old tld, and expected.c is what
 * tools/migrate.py makes of it (checked by run.sh). Neither is compiled. */
#include "tld.h"
#include "string/tld-string.h"
#include "lib/tld/src/stats/tld-auc.h"
#include "tpool/tpool_sig.h"
/* logsum and kdtree in a comment stay */
struct own { int logsum; };
static int own_helper(struct own *o) { return o->logsum; }

#define MY_ERR(...) ERROR_MSG(__VA_ARGS__)
#define MY_LOG(fmt, ...) LOG_MSG(fmt, __VA_ARGS__)

int run(tld_strbuf *b, double *Y, double *Yh, int n, double *w, char *path, char **name, double **pts, uint32_t *ids)
{
        struct tld_rng *rng = NULL;
        struct tld_rng_dist *d = NULL;
        tld_kdtree *t = NULL;
        tld_json_obj *o = NULL;
        double auc, thr, s;
        double x[2] = {0, 0};
        RUN(tld_rng_alloc(&rng, 42));
        ASSERT(rng != NULL, "no generator");
        galloc(&w, 4);
        RUN(tld_rng_dist_alloc(&d, w, n));
        int k = tld_rng_dist_sample(d);
        RUN(tld_kdtree_alloc(&t, 2));
        RUN(tld_kdtree_insert(t, x, 1));
        RUN(tld_kdtree_build(t, pts, ids, 1));
        RUN(tld_json_parse(TLD_STR(b), &o));
        RUN(tld_auc(Y, Yh, n, NULL, &auc));
        RUN(tld_auc(Y, Yh, n, &thr, NULL));
        RUN(tld_sigmoid(auc, &s));
        if(tld_path_exists(path)){ LOG_MSG("found"); }
        if(!tld_path_is_dir(path)){ RUN(tld_path_mkdir(path)); }
        ASSERT(!tld_path_exists(path), "should not exist: %s", path);
        if(tld_path_exists(path)){ LOG_MSG("%s", path); }
        RUN(tld_path_filename(path, name));
        tld_strbuf_append(b, "x");
        LOG_MSG("%s %d", "kdtree", k);
        WARNING_MSG("%s", path);
        tld_thread_pool_stop_all();
        if(tld_global_stop_run){ return FAIL; }
        tld_hash_print(NULL);
        TLD_JSON_OBJECT(TLD_JSTR("a", "b"), TLD_JAUTO("c", "1"));
        tld_rng_free(rng);
        tld_rng_dist_free(d);
        tld_kdtree_free(t);
        own_helper(NULL);
        return OK;
ERROR:
        return FAIL;
}

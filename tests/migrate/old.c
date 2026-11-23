/* tests/migrate: old.c is code written for the old tld, and expected.c is what
 * tools/migrate.py makes of it (checked by run.sh). Neither is compiled. */
#include "tld.h"
#include "string/str.h"
#include "lib/tld/src/stats/auc.h"
#include "tpool/tpool_sig.h"
/* logsum and kdtree in a comment stay */
struct own { int logsum; };
static int own_helper(struct own *o) { return o->logsum; }

#define MY_ERR(...) ERROR_MSG(__VA_ARGS__)
#define MY_LOG(fmt, ...) LOG_MSG(fmt, __VA_ARGS__)

int run(tld_strbuf *b, double *Y, double *Yh, int n, double *w, char *path, char **name, double **pts, uint32_t *ids)
{
        struct rng_state *rng = NULL;
        struct rng_dist *d = NULL;
        kdtree *t = NULL;
        tld_json_obj *o = NULL;
        double auc, thr, s;
        double x[2] = {0, 0};
        RUN(init_rng(&rng, 42));
        ASSERT(rng != NULL, "no generator")
        galloc(&w, 4)
        RUN(tl_random_sdist_init(&d, w, n, 7));
        int k = tl_random_sdist_smpl(d);
        RUN(kdtree_alloc(&t, 2));
        RUN(kdtree_insert(t, x, 1, 2));
        RUN(kdtree_build(t, pts, ids, 1, 2));
        RUN(tld_json_parse(b, &o));
        RUN(tld_auc_calculate(Y, Yh, n, &auc));
        RUN(tld_auc_best_threshold(Y, Yh, n, &thr));
        RUN(tld_sigmoid_safe(auc, &s));
        if(tld_file_exists(path) == OK){ LOG_MSG("found"); }
        if(tld_dir_exists(path) != OK){ RUN(tld_dir_create(path)); }
        ASSERT(tld_file_exists(path) == FAIL, "should not exist: %s", path);
        if(tld_file_exists(path)){ LOG_MSG(path); }
        RUN(tld_get_filename(path, name));
        tld_append(b, "x");
        LOG_MSG("%s %d", "kdtree", k);
        WARNING_MSG(path);
        tld_global_stop_run = 1;
        if(tld_global_stop_run){ return FAIL; }
        tld_hash_print(NULL);
        JSON_OBJ(JSTR("a", "b"), JS("c", "1"));
        free_rng(rng);
        tl_random_sdist_free(d);
        kdtree_free(t);
        own_helper(NULL);
        return OK;
ERROR:
        return FAIL;
}

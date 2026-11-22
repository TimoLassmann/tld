#include "tld.h"
#include "test.h"
#include <string.h>
#include <math.h>

/* regress-stats: regression tests for the stats fixes (modernisation). */

#define CHECK(TEST, ...) do {                                   \
                if(!(TEST)){                                    \
                        ERROR_MSG(__VA_ARGS__);                 \
                }                                               \
        } while(0)

static int close_to(double a, double b, double tol)
{
        return fabs(a - b) <= tol;
}

static int test_auc_logit_threshold(void);
static int test_auc_single_class(void);
static int test_auc_ties(void);
static int test_variance_n1(void);
static int test_qnorm_ties(void);
static int test_kde_constant(void);
static int test_kde_offset_noise(void);
static int test_kde_count_constant(void);
static int test_normal_pdf_sigma(void);
static int test_shannon(void);

/* Logit scores outside [0,1]: the threshold must not be clamped. */
int test_auc_logit_threshold(void)
{
        double y1[4] = {1, 1, 0, 0};
        double s1[4] = {3.2, 1.5, -0.5, -2.0};
        double y2[4] = {1, 0, 0, 0};
        double s2[4] = {3.2, 1.5, -0.5, -2.0};
        double t = 0.0;
        double auc = 0.0;

        RUN(tld_auc_best_threshold(y1, s1, 4, &t));
        CHECK(t == 1.5, "best threshold %f, expected 1.5", t);
        RUN(tld_auc(y1, s1, 4, &t, &auc));
        CHECK(t == 1.5, "tld_auc threshold %f, expected 1.5", t);
        CHECK(close_to(auc, 1.0, 1e-12), "AUC %f, expected 1.0", auc);

        RUN(tld_auc_best_threshold(y2, s2, 4, &t));
        CHECK(t == 3.2, "best threshold %f, expected 3.2", t);

        /* Score equal to 0.0 must be selectable as the threshold */
        double y3[4] = {1, 1, 0, 0};
        double s3[4] = {0.5, 0.0, -1.0, -2.0};
        RUN(tld_auc_best_threshold(y3, s3, 4, &t));
        CHECK(t == 0.0, "best threshold %f, expected 0.0", t);
        return OK;
ERROR:
        return FAIL;
}

int test_auc_single_class(void)
{
        double pos[3] = {1, 1, 1};
        double neg[3] = {0, 0, 0};
        double s[3] = {0.1, 0.5, 0.9};
        double t = 0.0;
        double auc = 0.0;

        CHECK(tld_auc(pos, s, 3, &t, &auc) == FAIL, "tld_auc accepted single-class input");
        CHECK(tld_auc(neg, s, 3, &t, &auc) == FAIL, "tld_auc accepted single-class input");
        CHECK(tld_auc_calculate(pos, s, 3, &auc) == FAIL, "tld_auc_calculate accepted single-class input");
        CHECK(tld_auc_calculate(neg, s, 3, &auc) == FAIL, "tld_auc_calculate accepted single-class input");
        CHECK(tld_auc_best_threshold(pos, s, 3, &t) == FAIL, "tld_auc_best_threshold accepted single-class input");
        CHECK(tld_auc_best_threshold(neg, s, 3, &t) == FAIL, "tld_auc_best_threshold accepted single-class input");

        /* Out-of-range label still fails, without touching uninitialised memory */
        double bad[3] = {1, -0.5, 0};
        CHECK(tld_auc_calculate(bad, s, 3, &auc) == FAIL, "out-of-range label accepted");
        return OK;
ERROR:
        return FAIL;
}

int test_auc_ties(void)
{
        double y[5] = {1, 1, 0, 0, 0};
        double s[5] = {0.6125478, 0.3642710, 0.4321361, 0.1402911, 0.3848959};
        double auc = 0.0;
        RUN(tld_auc_calculate(y, s, 5, &auc));
        CHECK(close_to(auc, 4.0 / 6.0, 1e-12), "AUC %f, expected 0.6667", auc);

        /* All scores tied: AUC 0.5 */
        double s2[4] = {0.3, 0.3, 0.3, 0.3};
        double y2[4] = {1, 0, 1, 0};
        RUN(tld_auc_calculate(y2, s2, 4, &auc));
        CHECK(close_to(auc, 0.5, 1e-12), "AUC %f, expected 0.5", auc);
        return OK;
ERROR:
        return FAIL;
}

int test_variance_n1(void)
{
        double v[1] = {3.0};
        double w[1] = {4.0};
        double r = 0.0;
        double mean = 0.0;
        double sd = 0.0;

        CHECK(tld_sample_variance(v, 1, &r) == FAIL, "sample variance n=1 returned OK");
        CHECK(tld_sample_covariance(v, w, 1, &r) == FAIL, "sample covariance n=1 returned OK");
        CHECK(tld_standardise(v, 1, &mean, &sd) == FAIL, "standardise n=1 returned OK");
        CHECK(tld_mean(v, 0, &r) == FAIL, "mean n=0 returned OK");

        double x[3] = {1.0, 2.0, 3.0};
        RUN(tld_sample_variance(x, 3, &r));
        CHECK(close_to(r, 1.0, 1e-12), "sample variance %f, expected 1", r);
        return OK;
ERROR:
        return FAIL;
}

/* col0 = [1,1,2], col1 = [1,2,3]: reference quantiles [1,1.5,2.5].
   The tie in col0 covers positions 0 and 1 -> 1.25; value 2 -> 2.5. */
int test_qnorm_ties(void)
{
        double** d = NULL;
        galloc(&d, 3, 2);
        d[0][0] = 1.0; d[1][0] = 1.0; d[2][0] = 2.0;
        d[0][1] = 1.0; d[1][1] = 2.0; d[2][1] = 3.0;

        RUN(tld_quantile_normalise(d, 3, 2));

        CHECK(close_to(d[0][0], 1.25, 1e-12), "d[0][0] = %f, expected 1.25", d[0][0]);
        CHECK(close_to(d[1][0], 1.25, 1e-12), "d[1][0] = %f, expected 1.25", d[1][0]);
        CHECK(close_to(d[2][0], 2.5, 1e-12), "d[2][0] = %f, expected 2.5", d[2][0]);
        CHECK(close_to(d[0][1], 1.0, 1e-12), "d[0][1] = %f, expected 1.0", d[0][1]);
        CHECK(close_to(d[1][1], 1.5, 1e-12), "d[1][1] = %f, expected 1.5", d[1][1]);
        CHECK(close_to(d[2][1], 2.5, 1e-12), "d[2][1] = %f, expected 2.5", d[2][1]);

        /* Unsorted input with ties in the middle */
        d[0][0] = 5.0; d[1][0] = 3.0; d[2][0] = 5.0;
        d[0][1] = 2.0; d[1][1] = 9.0; d[2][1] = 4.0;
        /* sorted col0: 3,5,5  col1: 2,4,9  ref: 2.5,4.5,7 */
        RUN(tld_quantile_normalise(d, 3, 2));
        CHECK(close_to(d[1][0], 2.5, 1e-12), "d[1][0] = %f, expected 2.5", d[1][0]);
        CHECK(close_to(d[0][0], 5.75, 1e-12), "d[0][0] = %f, expected 5.75", d[0][0]);
        CHECK(close_to(d[2][0], 5.75, 1e-12), "d[2][0] = %f, expected 5.75", d[2][0]);
        CHECK(close_to(d[0][1], 2.5, 1e-12), "d[0][1] = %f, expected 2.5", d[0][1]);
        CHECK(close_to(d[2][1], 4.5, 1e-12), "d[2][1] = %f, expected 4.5", d[2][1]);
        CHECK(close_to(d[1][1], 7.0, 1e-12), "d[1][1] = %f, expected 7", d[1][1]);

        d[1][1] = NAN;
        CHECK(tld_quantile_normalise(d, 3, 2) == FAIL, "NaN input accepted");

        gfree(d);
        return OK;
ERROR:
        if(d){
                gfree(d);
        }
        return FAIL;
}

static int kde_check(double **ladder, int n_steps, double lo, double hi)
{
        double integral = 0.0;
        double step = (ladder[n_steps - 1][0] - ladder[0][0]) / (double)(n_steps - 1);
        CHECK(step > 0.0, "KDE ladder has zero width");
        for(int i = 0; i < n_steps; i++){
                CHECK(isfinite(ladder[i][0]), "ladder x %d not finite", i);
                CHECK(isfinite(ladder[i][1]), "density %d not finite", i);
                CHECK(ladder[i][1] >= 0.0, "density %d negative", i);
                integral += ladder[i][1] * step;
        }
        CHECK(integral >= lo && integral <= hi, "KDE integral %f outside [%f,%f]", integral, lo, hi);
        return OK;
ERROR:
        return FAIL;
}

int test_kde_constant(void)
{
        double x[50];
        double** ladder = NULL;
        int n_steps = 200;

        for(int i = 0; i < 50; i++){
                x[i] = 7.0;
        }
        RUN(tld_kde_pdf(x, 50, n_steps, &ladder));
        RUN(kde_check(ladder, n_steps, 0.98, 1.02));
        gfree(ladder);
        ladder = NULL;

        for(int i = 0; i < 50; i++){
                x[i] = 0.0;
        }
        RUN(tld_kde_pdf(x, 50, n_steps, &ladder));
        RUN(kde_check(ladder, n_steps, 0.98, 1.02));
        gfree(ladder);
        ladder = NULL;

        for(int i = 0; i < 50; i++){
                x[i] = 1e9;
        }
        RUN(tld_kde_pdf(x, 50, n_steps, &ladder));
        RUN(kde_check(ladder, n_steps, 0.98, 1.02));
        gfree(ladder);
        ladder = NULL;

        CHECK(tld_kde_pdf(x, 50, 0, &ladder) == FAIL, "n_steps 0 accepted");
        return OK;
ERROR:
        if(ladder){
                gfree(ladder);
        }
        return FAIL;
}

/* Near-constant data with a large offset: one-pass variance cancels. */
int test_kde_offset_noise(void)
{
        struct tld_rng* rng = NULL;
        double* x = NULL;
        double** ladder = NULL;
        int len = 1000;
        int n_steps = 400;

        RUN(tld_rng_alloc(&rng, 42));
        galloc(&x, len);
        for(int i = 0; i < len; i++){
                x[i] = 1e9 + tld_rng_gaussian(rng, 0.0, 1e-3);
        }
        RUN(tld_kde_pdf(x, len, n_steps, &ladder));
        /* The ladder spans only [min,max], so the tails are cut off. */
        RUN(kde_check(ladder, n_steps, 0.95, 1.02));
        gfree(ladder);
        ladder = NULL;

        gfree(x);
        tld_rng_free(rng);
        return OK;
ERROR:
        if(ladder){
                gfree(ladder);
        }
        if(x){
                gfree(x);
        }
        if(rng){
                tld_rng_free(rng);
        }
        return FAIL;
}

int test_kde_count_constant(void)
{
        double x[3] = {2.0, 2.0, 2.0};
        double c[3] = {1.0, 4.0, 5.0};
        double z[3] = {0.0, 0.0, 0.0};
        double** ladder = NULL;

        RUN(tld_kde_count_pdf(x, c, 3, &ladder));
        for(int i = 0; i < 3; i++){
                CHECK(isfinite(ladder[i][1]) && ladder[i][1] > 0.0, "count KDE density %d = %f", i, ladder[i][1]);
        }
        gfree(ladder);
        ladder = NULL;

        CHECK(tld_kde_count_pdf(x, z, 3, &ladder) == FAIL, "zero total count accepted");
        return OK;
ERROR:
        if(ladder){
                gfree(ladder);
        }
        return FAIL;
}

int test_normal_pdf_sigma(void)
{
        double p = 0.0;
        CHECK(tld_normal_pdf(0.0, 0.0, -1.0, &p) == FAIL, "negative sigma accepted");
        CHECK(tld_normal_pdf(0.0, 0.0, 0.0, &p) == FAIL, "zero sigma accepted");
        RUN(tld_normal_pdf(0.0, 0.0, 1.0, &p));
        CHECK(close_to(p, 0.3989422804014327, 1e-12), "pdf %f", p);
        return OK;
ERROR:
        return FAIL;
}

int test_shannon(void)
{
        double v[4] = {1.0, 1.0, 1.0, 1.0};
        double z[2] = {0.0, 0.0};
        double neg[2] = {1.0, -1.0};
        double e = -1.0;
        RUN(tld_shannon(v, 4, &e));
        CHECK(close_to(e, 2.0, 1e-12), "entropy %f, expected 2", e);
        RUN(tld_shannon(z, 2, &e));
        CHECK(e == 0.0, "entropy of zeros %f, expected 0", e);
        CHECK(tld_shannon(neg, 2, &e) == FAIL, "negative counts accepted");
        return OK;
ERROR:
        return FAIL;
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        int (*fn)(void);
        const char *about;
} checks[] = {
        {"auc-logit", test_auc_logit_threshold, "AUC threshold is not clamped to [0,1]"},
        {"auc-single-class", test_auc_single_class, "AUC fails on single-class labels"},
        {"auc-ties", test_auc_ties, "AUC with tied scores"},
        {"variance-n1", test_variance_n1, "variance of one value"},
        {"qnorm-ties", test_qnorm_ties, "quantile normalisation with ties"},
        {"kde-constant", test_kde_constant, "KDE of constant data"},
        {"kde-offset-noise", test_kde_offset_noise, "KDE of offset data with noise"},
        {"kde-count-constant", test_kde_count_constant, "KDE count on constant data"},
        {"normal-pdf", test_normal_pdf_sigma, "normal pdf rejects sigma <= 0"},
        {"shannon", test_shannon, "Shannon entropy"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the stats fixes (AUC, variance, qnorm, KDE, normal pdf,\n"
                  "Shannon entropy).\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  auc-logit           AUC threshold is not clamped to [0,1]\n"
                  "  auc-single-class    AUC fails on single-class labels\n"
                  "  auc-ties            AUC with tied scores\n"
                  "  variance-n1         variance of one value\n"
                  "  qnorm-ties          quantile normalisation with ties\n"
                  "  kde-constant        KDE of constant data\n"
                  "  kde-offset-noise    KDE of offset data with noise\n"
                  "  kde-count-constant  KDE count on constant data\n"
                  "  normal-pdf          normal pdf rejects sigma <= 0\n"
                  "  shannon             Shannon entropy\n"
                  "\n"
                  "Example: regress-stats auc-ties shannon");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-stats: no check named '%s' (see -h)\n", argv[i]);
                        return 2;
                }
        }
        for(int k = 0; k < n_checks; k++){
                int want = argc == 1;
                for(int i = 1; i < argc; i++){
                        want |= strcmp(argv[i], checks[k].name) == 0;
                }
                if(!want){
                        continue;
                }
                SECTION_MSG("%s: %s", checks[k].name, checks[k].about);
                RUN(checks[k].fn());
                OK_MSG("%s passed", checks[k].name);
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

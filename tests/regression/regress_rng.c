#include "tld.h"
#include "test.h"
#include <string.h>

#include <math.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* regress-rng: regression tests for the rng fixes.

   White-box: struct tld_rng is opaque in the public header. The mirror below
   copies its layout from src/rng/tld-rng.c so a
   test can put the generator into a chosen state. Keep it in sync. */

struct rng_state_mirror {
        uint64_t s[4];
        int has_gauss;
        double gauss;
} __attribute__((aligned(64)));


/* inverse of an odd number modulo 2^64 (Newton iteration) */
static uint64_t inv64(uint64_t a)
{
        uint64_t x = a;
        for(int i = 0; i < 6; i++){
                x *= 2 - a * x;
        }
        return x;
}

static uint64_t rotr64(uint64_t x, unsigned k)
{
        return (x >> k) | (x << (64U - k));
}

/* Sets the state so that the next raw 64-bit draw of xoshiro256** is
   target (the output only depends on s[1]). */
static void force_next(struct tld_rng *rng, uint64_t target)
{
        struct rng_state_mirror *m = (struct rng_state_mirror *) rng;
        m->s[0] = 0x0123456789abcdefULL;
        m->s[1] = inv64(5) * rotr64(target * inv64(9), 7);
        m->s[2] = 0xfedcba9876543210ULL;
        m->s[3] = 0x0f1e2d3c4b5a6978ULL;
}

static int test_force_next_sanity(void)
{
        struct tld_rng *rng = NULL;
        RUN(tld_rng_alloc(&rng, 1));
        /* 2^63 + 2^11: old and new mapping both give exactly 0.5 + 2^-53 */
        force_next(rng, (UINT64_C(1) << 63) | (UINT64_C(1) << 11));
        ASSERT(tld_rng_double(rng) == 0.5 + 0x1.0p-53, "force_next does not control the draw");
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

static int test_double_open_interval(void)
{
        struct tld_rng *rng = NULL;
        double r;
        RUN(tld_rng_alloc(&rng, 1));

        force_next(rng, UINT64_MAX);
        r = tld_rng_double(rng);
        ASSERT(r > 0.0 && r < 1.0, "tld_rng_double returned %a for the largest draw", r);

        force_next(rng, UINT64_MAX - 100);
        r = tld_rng_double(rng);
        ASSERT(r < 1.0, "tld_rng_double returned %a", r);

        /* a draw whose top 53 bits are zero is rejected, not returned as 0 */
        force_next(rng, 0x7ffULL);
        r = tld_rng_double(rng);
        ASSERT(r > 0.0 && r < 1.0, "tld_rng_double returned %a", r);

        tld_rng_set_seed(rng, 7);
        for(int i = 0; i < 1000000; i++){
                r = tld_rng_double(rng);
                ASSERT(r > 0.0 && r < 1.0, "draw %d out of (0,1): %a", i, r);
                if((i + 1) % 10000 == 0){
                        tld_progress(i + 1, 1000000, "draws");
                }
        }
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

static int test_int_range(void)
{
        struct tld_rng *rng = NULL;
        int v;
        RUN(tld_rng_alloc(&rng, 1));

        force_next(rng, UINT64_MAX);
        v = tld_rng_int(rng, 1);
        ASSERT(v == 0, "tld_rng_int(rng,1) returned %d", v);

        force_next(rng, UINT64_MAX);
        v = tld_rng_int(rng, 10);
        ASSERT(v >= 0 && v < 10, "tld_rng_int(rng,10) returned %d", v);

        force_next(rng, UINT64_MAX);
        v = tld_rng_int(rng, 2147483647);
        ASSERT(v >= 0 && v < 2147483647, "tld_rng_int(rng,INT_MAX) returned %d", v);

        ASSERT(tld_rng_int(rng, 0) == 0, "tld_rng_int(rng,0) != 0");

        tld_rng_set_seed(rng, 11);
        for(int i = 0; i < 100000; i++){
                ASSERT(tld_rng_int(rng, 1) == 0, "tld_rng_int(rng,1) != 0");
                v = tld_rng_int(rng, 7);
                ASSERT(v >= 0 && v < 7, "tld_rng_int(rng,7) returned %d", v);
        }
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

static int test_int_uniform(void)
{
        struct tld_rng *rng = NULL;
        const int ns[] = {2, 3, 5, 10};
        RUN(tld_rng_alloc(&rng, 12345));
        for(int k = 0; k < 4; k++){
                const int n = ns[k];
                const int draws = 200000;
                int count[10] = {0};
                double chi2 = 0.0;
                double e = (double) draws / n;
                for(int i = 0; i < draws; i++){
                        count[tld_rng_int(rng, n)]++;
                }
                for(int i = 0; i < n; i++){
                        chi2 += (count[i] - e) * (count[i] - e) / e;
                }
                KV_MSG("chi2", "n = %2d: %.2f (limit 60)", n, chi2);
                /* df <= 9; 60 is far beyond the 0.9999 quantile */
                ASSERT(chi2 < 60.0, "tld_rng_int(rng,%d) not uniform: chi2 = %f", n, chi2);
        }
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

static int test_exponential_finite(void)
{
        struct tld_rng *rng = NULL;
        double g;
        RUN(tld_rng_alloc(&rng, 1));
        /* shape 1 is the exponential: -log(1 - U) */
        force_next(rng, UINT64_MAX);
        g = tld_rng_gamma(rng, 1.0, 1.0);
        ASSERT(isfinite(g) && g > 0.0, "exponential deviate is %f", g);
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_free(rng);
        return FAIL;
}

static int test_child_gauss(void)
{
        struct tld_rng *parent = NULL;
        struct tld_rng *child = NULL;
        struct rng_state_mirror *pm = NULL;
        struct rng_state_mirror *cm = NULL;
        double gp;
        double gc;

        RUN(tld_rng_alloc(&parent, 99));
        pm = (struct rng_state_mirror *) parent;
        pm->has_gauss = 1;
        pm->gauss = 1.5;

        RUN(tld_rng_split(&child, parent));
        cm = (struct rng_state_mirror *) child;
        ASSERT(cm->has_gauss == 0, "child inherited the parent's cached gaussian");

        /* tld_rng_gamma with shape > 1 consumes tl_gauss first; with a
           shared cached value both return b*(1+c*1.5)^3 on acceptance. */
        gp = tld_rng_gamma(parent, 3.0, 1.0);
        gc = tld_rng_gamma(child, 3.0, 1.0);
        ASSERT(gp != gc, "parent and child returned the same gamma deviate %f", gp);

        tld_rng_free(parent);
        tld_rng_free(child);
        return OK;
ERROR:
        tld_rng_free(parent);
        tld_rng_free(child);
        return FAIL;
}

static int test_seed_reproducible(void)
{
        struct tld_rng *a = NULL;
        struct tld_rng *b = NULL;
        RUN(tld_rng_alloc(&a, 2024));
        RUN(tld_rng_alloc(&b, 2024));
        for(int i = 0; i < 1000; i++){
                ASSERT(tld_rng_int(a, 1000) == tld_rng_int(b, 1000), "same seed, different sequence");
                ASSERT(tld_rng_double(a) == tld_rng_double(b), "same seed, different sequence");
        }
        tld_rng_free(a);
        tld_rng_free(b);
        return OK;
ERROR:
        tld_rng_free(a);
        tld_rng_free(b);
        return FAIL;
}

/* Seed 0 used to be made from the time in seconds and clock() only (the process id
   was never mixed in), so generators created in one loop, or by jobs started in the
   same second, often produced the same stream. */
static int test_seed0_distinct(void)
{
        enum { N_RNG = 64, N_PROC = 16 };
        struct tld_rng *r[N_RNG] = {NULL};
        uint64_t first[N_RNG + N_PROC];
        int fd[2] = {-1, -1};

        for(int i = 0; i < N_RNG; i++){
                RUN(tld_rng_alloc(&r[i], 0));
                first[i] = (uint64_t) (tld_rng_double(r[i]) * 9007199254740992.0);
        }
        ASSERT(pipe(fd) == 0, "pipe failed");
        for(int i = 0; i < N_PROC; i++){
                pid_t pid = fork();
                ASSERT(pid >= 0, "fork failed");
                if(pid == 0){
                        struct tld_rng *c = NULL;
                        uint64_t v = 0;
                        if(tld_rng_alloc(&c, 0) == OK){
                                v = (uint64_t) (tld_rng_double(c) * 9007199254740992.0);
                        }
                        _exit(write(fd[1], &v, sizeof v) == (ssize_t) sizeof v ? 0 : 1);
                }
                ASSERT(read(fd[0], &first[N_RNG + i], sizeof first[0]) == (ssize_t) sizeof first[0],
                       "child %d did not report", i);
                waitpid(pid, NULL, 0);
        }
        for(int i = 0; i < N_RNG + N_PROC; i++){
                for(int j = i + 1; j < N_RNG + N_PROC; j++){
                        ASSERT(first[i] != first[j], "seed 0: generators %d and %d start alike", i, j);
                }
        }
        close(fd[0]);
        close(fd[1]);
        for(int i = 0; i < N_RNG; i++){
                tld_rng_free(r[i]);
        }
        return OK;
ERROR:
        if(fd[0] >= 0) close(fd[0]);
        if(fd[1] >= 0) close(fd[1]);
        for(int i = 0; i < N_RNG; i++){
                tld_rng_free(r[i]);
        }
        return FAIL;
}

/* Runs tld_rng_dist_alloc with stdout sent to a temporary file and
   returns the number of bytes it wrote there. */
static int sdist_init_quiet(struct tld_rng_dist **d, double *w, int n, long *n_bytes)
{
        char path[] = "/tmp/tld_sdist_XXXXXX";
        int fd = -1;
        int saved = -1;
        int status;
        fflush(stdout);
        fd = mkstemp(path);
        ASSERT(fd >= 0, "mkstemp failed");
        unlink(path);
        saved = dup(STDOUT_FILENO);
        ASSERT(saved >= 0, "dup failed");
        dup2(fd, STDOUT_FILENO);
        status = tld_rng_dist_alloc(d, w, n);
        fflush(stdout);
        dup2(saved, STDOUT_FILENO);
        close(saved);
        *n_bytes = (long) lseek(fd, 0, SEEK_END);
        close(fd);
        return status;
ERROR:
        if(fd >= 0){
                close(fd);
        }
        return FAIL;
}

static int test_sdist(void)
{
        struct tld_rng_dist *d = NULL;
        struct tld_rng *rng = NULL;
        double w[6] = {1.0, 1.0, 2.0, 1.0, 1.0, 1.0};
        int count[6] = {0};
        const int draws = 700000;
        long n_bytes = -1;
        double chi2 = 0.0;

        ASSERT(sdist_init_quiet(&d, w, 6, &n_bytes) == OK, "sdist init failed");
        ASSERT(n_bytes == 0, "tld_rng_dist_alloc wrote %ld bytes to stdout", n_bytes);
        RUN(tld_rng_alloc(&rng, 42));

        for(int i = 0; i < draws; i++){
                const int k = tld_rng_dist_sample(d, rng);
                ASSERT(k >= 0 && k < 6, "sample %d out of range", k);
                count[k]++;
        }
        for(int i = 0; i < 6; i++){
                const double e = draws * w[i] / 7.0;
                chi2 += (count[i] - e) * (count[i] - e) / e;
        }
        KV_MSG("chi2", "%.2f (limit 60)", chi2);
        ASSERT(chi2 < 60.0, "sdist not proportional to weights: chi2 = %f", chi2);

        /* the largest draw must not index p[n] (out of bounds before) */
        for(int k = 0; k < 2; k++){
                force_next(rng, UINT64_MAX);
                const int s = tld_rng_dist_sample(d, rng);
                ASSERT(s >= 0 && s < 6, "sample %d out of range", s);
        }
        tld_rng_dist_free(d);
        tld_rng_free(rng);
        return OK;
ERROR:
        tld_rng_dist_free(d);
        tld_rng_free(rng);
        return FAIL;
}

static int test_sdist_errors(void)
{
        struct tld_rng_dist *d = NULL;
        double neg[3] = {1.0, -1.0, 1.0};
        double zero[3] = {0.0, 0.0, 0.0};
        double nan_w[2] = {1.0, NAN};
        /* error paths must fail and free what they allocated (ASan/LSan or
           valgrind report a leak otherwise) */
        ASSERT(tld_rng_dist_alloc(&d, neg, 3) == FAIL, "negative weight accepted");
        ASSERT(d == NULL, "output set on failure");
        ASSERT(tld_rng_dist_alloc(&d, zero, 3) == FAIL, "zero sum accepted");
        ASSERT(d == NULL, "output set on failure");
        ASSERT(tld_rng_dist_alloc(&d, nan_w, 2) == FAIL, "NaN weight accepted");
        ASSERT(tld_rng_dist_alloc(&d, neg, 0) == FAIL, "n = 0 accepted");
        ASSERT(d == NULL, "output set on failure");
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
        {"force-next", test_force_next_sanity, "the test's force_next controls the next draw"},
        {"double-open", test_double_open_interval, "tld_rng_double stays in the open (0,1)"},
        {"int-range", test_int_range, "tld_rng_int(rng, n) stays in [0, n)"},
        {"int-uniform", test_int_uniform, "tld_rng_int is uniform (chi2)"},
        {"exponential", test_exponential_finite, "the exponential deviate is finite"},
        {"child-gauss", test_child_gauss, "a child rng does not inherit the cached gaussian"},
        {"seed", test_seed_reproducible, "the same seed gives the same sequence"},
        {"seed0", test_seed0_distinct, "seed 0 gives every generator and process its own stream"},
        {"sdist", test_sdist, "tl_random_sdist samples in proportion, prints nothing"},
        {"sdist-errors", test_sdist_errors, "tld_rng_dist_alloc rejects bad weights"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Regression checks for the rng fixes (white-box: forces the generator into\n"
                  "chosen states).\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  force-next    the test's force_next controls the next draw\n"
                  "  double-open   tld_rng_double stays in the open (0,1)\n"
                  "  int-range     tld_rng_int(rng, n) stays in [0, n)\n"
                  "  int-uniform   tld_rng_int is uniform (chi2)\n"
                  "  exponential   the exponential deviate is finite\n"
                  "  child-gauss   a child rng does not inherit the cached gaussian\n"
                  "  seed          the same seed gives the same sequence\n"
                  "  seed0         seed 0 gives every generator and process its own stream\n"
                  "  sdist         tl_random_sdist samples in proportion, prints nothing\n"
                  "  sdist-errors  tld_rng_dist_alloc rejects bad weights\n"
                  "\n"
                  "Example: regress-rng int-uniform sdist");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-rng: no check named '%s' (see -h)\n", argv[i]);
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

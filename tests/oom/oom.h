/* tests/oom/oom.h - allocation-failure sweep for the single headers (not part of the library).
 *
 * Include this before the implementation of tld.h (and tld_ai.h): every malloc, calloc,
 * realloc, aligned_alloc and free in that translation unit then goes through the
 * wrappers below. A scenario is a function that uses the library and returns 0 when it
 * worked. oom_main runs each scenario once to count its allocations, then once per
 * allocation with exactly that one failing (errno set to ENOMEM, as malloc does), and
 * reports
 *   LEAK       blocks still allocated after the run;
 *   SWALLOWED  the scenario returned 0 although an allocation failed, unless the run
 *              printed the scenario's `allowed` text (a best-effort step that warns
 *              and carries on, such as tld_ai.h's cache write);
 * and a crash shows as a failing test, with the sanitizer's report in oom-NAME.err.
 *
 * Arguments: scenario names (default all), or NAME:INDEX to run one failing index
 * (with OOM_TRACE=1 in the environment an ASan build prints the stack of the failing
 * allocation). Each run's stderr goes to oom-NAME.err in the working directory; the file
 * is removed when a whole scenario passes and kept for NAME:INDEX. */
#ifndef TLD_TEST_OOM_H
#define TLD_TEST_OOM_H

/* This file comes before tld.h's implementation and its system headers, so it asks for
   what tld.h would (POSIX.1-2008 and the BSD/XSI extras such as realpath). */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"

static atomic_long oom_n;               /* allocation calls in the current run */
static atomic_long oom_live;            /* blocks allocated and not yet freed */
static long oom_fail_at = -1;           /* the call that fails, -1 for none */
static int oom_trace = 0;

#if defined(__SANITIZE_ADDRESS__)
#define OOM__ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OOM__ASAN 1
#endif
#endif
#ifdef OOM__ASAN
void __sanitizer_print_stack_trace(void);
#endif

static int oom_fail(void)
{
        long i = atomic_fetch_add(&oom_n, 1);
        if(oom_fail_at >= 0 && i == oom_fail_at){
#ifdef OOM__ASAN
                if(oom_trace){
                        __sanitizer_print_stack_trace();
                }
#endif
                errno = ENOMEM;
                return 1;
        }
        return 0;
}

__attribute__((unused)) static void *oom_malloc(size_t n)
{
        void *p = NULL;
        if(oom_fail()){
                return NULL;
        }
        p = malloc(n);
        if(p){
                atomic_fetch_add(&oom_live, 1);
        }
        return p;
}

__attribute__((unused)) static void *oom_calloc(size_t a, size_t b)
{
        void *p = NULL;
        if(oom_fail()){
                return NULL;
        }
        p = calloc(a, b);
        if(p){
                atomic_fetch_add(&oom_live, 1);
        }
        return p;
}

__attribute__((unused)) static void *oom_realloc(void *p, size_t n)
{
        void *q = NULL;
        if(oom_fail()){
                return NULL;
        }
        q = realloc(p, n);
        if(q && !p){
                atomic_fetch_add(&oom_live, 1);
        }
        return q;
}

__attribute__((unused)) static void *oom_aligned_alloc(size_t a, size_t n)
{
        void *p = NULL;
        if(oom_fail()){
                return NULL;
        }
        p = aligned_alloc(a, n);
        if(p){
                atomic_fetch_add(&oom_live, 1);
        }
        return p;
}

/* strdup allocates inside libc, out of reach of the malloc macro below: copy through
   oom_malloc so its allocations are counted and can fail like the others. */
__attribute__((unused)) static char *oom_strdup(const char *s)
{
        size_t n = strlen(s) + 1;
        char *d = oom_malloc(n);
        if(d){
                memcpy(d, s, n);
        }
        return d;
}

__attribute__((unused)) static void oom_free(void *p)
{
        if(p){
                atomic_fetch_sub(&oom_live, 1);
        }
        free(p);
}

struct oom_scenario {
        const char *name;
        int (*fn)(void);
        const char *about;
        const char *allowed;    /* see SWALLOWED above; NULL for none */
        long stride;            /* 0 or 1: every allocation fails in turn; above 1: the
                                   first and the last OOM_DENSE, and every stride-th between
                                   (for a scenario whose allocations are mostly one loop
                                   repeated, where failing each costs the square of their
                                   number and reaches no new error path) */
};

#define OOM_DENSE 512

/* Does the file at fd contain text? text must be shorter than half the buffer; longer
   text is never found. The buffer holds the tail of the previous read (shorter than
   text, for a match across reads) followed by one read of half the buffer. */
static int oom_file_has(int fd, const char *text)
{
        static char buf[1 << 16];
        const size_t chunk = sizeof buf / 2;
        size_t keep = strlen(text);
        size_t have = 0;
        ssize_t n = 0;
        if(keep == 0 || keep >= chunk){
                return keep == 0;
        }
        if(lseek(fd, 0, SEEK_SET) < 0){
                return 0;
        }
        while((n = read(fd, buf + have, chunk - 1)) > 0){
                have += (size_t) n;
                buf[have] = 0;
                if(strstr(buf, text)){
                        return 1;
                }
                if(have >= keep){
                        memmove(buf, buf + have - (keep - 1), keep - 1);
                        have = keep - 1;
                }
        }
        return 0;
}

/* Sweep scenario s over [from, to) (to < 0: to the end); returns the number of findings. */
static int oom_sweep(const struct oom_scenario *s, long from, long to)
{
        char path[256];
        long total = 0;
        long live0 = 0;
        int n_found = 0;
        int rc = 0;
        int fd = -1;
        int err = -1;

        snprintf(path, sizeof path, "oom-%s.err", s->name);
        fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        err = dup(STDERR_FILENO);
        if(fd < 0 || err < 0){
                fprintf(stderr, "oom: cannot open %s\n", path);
                return 1;
        }
        fflush(stderr);
        dup2(fd, STDERR_FILENO);

        /* warm-up (settings, caches, digests), then the counting run */
        if(s->fn() != 0){
                dprintf(err, "BROKEN %s: fails without any injected failure (see %s)\n", s->name, path);
                n_found++;
                goto DONE;
        }
        atomic_store(&oom_n, 0);
        live0 = atomic_load(&oom_live);
        rc = s->fn();
        total = atomic_load(&oom_n);
        if(rc != 0 || atomic_load(&oom_live) != live0){
                dprintf(err, "BROKEN %s: the counting run returned %d with %ld block(s) left\n", s->name, rc,
                        atomic_load(&oom_live) - live0);
                n_found++;
                goto DONE;
        }
        if(to < 0 || to > total){
                to = total;
        }
        for(long i = from; i < to; i++){
                if(s->stride > 1 && to - from > 1 && i >= from + OOM_DENSE && i < to - OOM_DENSE &&
                   (i - from - OOM_DENSE) % s->stride){
                        continue;
                }
                fflush(stderr);
                if(ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) < 0){
                        dprintf(err, "oom: cannot reset %s\n", path);
                        n_found++;
                        break;
                }
                dprintf(fd, "== %s: allocation %ld of %ld fails\n", s->name, i, total);
                live0 = atomic_load(&oom_live);
                atomic_store(&oom_n, 0);
                oom_fail_at = i;
                rc = s->fn();
                oom_fail_at = -1;
                fflush(stderr);
                if(atomic_load(&oom_live) != live0){
                        dprintf(err, "LEAK %s:%ld: %ld block(s) not freed (returned %d)\n", s->name, i,
                                atomic_load(&oom_live) - live0, rc);
                        n_found++;
                }
                if(rc == 0 && !(s->allowed && oom_file_has(fd, s->allowed))){
                        dprintf(err, "SWALLOWED %s:%ld: returned 0 although an allocation failed\n", s->name, i);
                        n_found++;
                }
        }
DONE:
        fflush(stderr);
        dup2(err, STDERR_FILENO);
        close(err);
        close(fd);
        if(n_found == 0 && to - from == 1 && from < total){
                /* one index asked for: keep its output (and OOM_TRACE stack) */
                fprintf(stderr, " OK %s:%ld: no finding; its stderr is in %s\n", s->name, from, path);
        }else if(n_found == 0){
                const char *v = getenv("TLD_VERBOSE");
                unlink(path);
                if(!v || strcmp(v, "0") != 0){  /* tld.h comes after this file: no OK_MSG */
                        if(s->stride > 1){
                                fprintf(stderr, " OK %s: %ld allocations, the first and last %d and every "
                                        "%ld-th failed\n", s->name, total, OOM_DENSE, s->stride);
                        }else{
                                fprintf(stderr, " OK %s: %ld allocations, each failed in turn\n", s->name, total);
                        }
                }
        }
        return n_found;
}

static int oom_main(int argc, char *argv[], const struct oom_scenario *s, int n, const char *what)
{
        char help[4096];
        int len = 0;
        int n_found = 0;
        len = snprintf(help, sizeof help,
                       "Allocation-failure sweep over %s: each allocation of a scenario fails in\n"
                       "turn; a leak, a crash or a failure reported as success fails the test.\n"
                       "NAME:INDEX runs one failing allocation (OOM_TRACE=1 prints its stack in an\n"
                       "ASan build). Each run's stderr goes to oom-NAME.err, kept on failure.\n"
                       "\nSCENARIO names (default: all, in this order):\n", what);
        for(int k = 0; k < n && len < (int) sizeof help; k++){
                len += snprintf(help + len, sizeof help - (size_t) len, "  %-13s %s\n", s[k].name, s[k].about);
        }
        test_init(&argc, argv, "[SCENARIO[:INDEX] ...]", help);
        oom_trace = getenv("OOM_TRACE") != NULL;
        for(int i = 1; i < argc; i++){
                const char *colon = strchr(argv[i], ':');
                size_t nl = colon ? (size_t) (colon - argv[i]) : strlen(argv[i]);
                int k = 0;
                while(k < n && !(strlen(s[k].name) == nl && strncmp(argv[i], s[k].name, nl) == 0)){
                        k++;
                }
                if(k == n){
                        fprintf(stderr, "oom: no scenario named '%.*s' (see -h)\n", (int) nl, argv[i]);
                        return 2;
                }
        }
        for(int k = 0; k < n; k++){
                int want = argc == 1;
                long from = 0;
                long to = -1;
                for(int i = 1; i < argc; i++){
                        const char *colon = strchr(argv[i], ':');
                        size_t nl = colon ? (size_t) (colon - argv[i]) : strlen(argv[i]);
                        if(strlen(s[k].name) == nl && strncmp(argv[i], s[k].name, nl) == 0){
                                want = 1;
                                if(colon){
                                        from = atol(colon + 1);
                                        to = from + 1;
                                }
                        }
                }
                if(want){
                        n_found += oom_sweep(&s[k], from, to);
                }
        }
        if(n_found){
                fprintf(stderr, "oom: %d finding(s); the stderr of the last failing run of each scenario is in oom-NAME.err\n",
                        n_found);
                return EXIT_FAILURE;
        }
        {
                const char *v = getenv("TLD_VERBOSE");
                if(!v || strcmp(v, "0") != 0){
                        fprintf(stderr, " OK no leak, crash or swallowed failure in any of them\n");
                }
        }
        return EXIT_SUCCESS;
}

#define malloc oom_malloc
#define calloc oom_calloc
#define realloc oom_realloc
#define aligned_alloc oom_aligned_alloc
#define free oom_free
#undef strdup
#define strdup oom_strdup

#endif

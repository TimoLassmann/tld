/* unit-align: every heap-allocated struct that declares an alignment larger than
 * malloc's guarantee (16 bytes on 64-bit platforms) must actually be allocated at that
 * alignment. The struct declarations promise it to the compiler
 * (__attribute__((aligned(N)))); allocating them with plain malloc broke the promise,
 * which is undefined behaviour (UBSan: "member access within misaligned address ...
 * for type 'struct tld_rng', which requires 64 byte alignment" in tld_rng_alloc).
 *
 * Each constructor is called many times: plain malloc returns 16-byte-aligned blocks,
 * so the chance that 200 of them all land on a 64/128-byte boundary by accident is
 * negligible, and the test fails reliably on a regression. */
#include "tld.h"
#include "test.h"
#include <stdint.h>
#include <stdio.h>

#define N_ALLOC 200
#define N_MAX 100000

static int n_fail = 0;

static void check(const void *p, uintptr_t align, const char *what, int i)
{
        if (((uintptr_t)p) % align != 0) {
                if (n_fail < 10) {
                        fprintf(stderr, "FAIL: %s #%d at %p is not %lu-byte aligned\n",
                                what, i, p, (unsigned long)align);
                }
                n_fail++;
        }
}

int main(int argc, char *argv[])
{
        static struct tld_rng *rng[N_MAX];
        static struct tld_rng *rng2[N_MAX];
        static struct tld_rng_dist *dist[N_MAX];
        double w[4] = { 0.1, 0.2, 0.3, 0.4 };
        int n = N_ALLOC;

        test_init(&argc, argv, "[N]",
                  "With no arguments: allocates 200 tld_rng (64-byte aligned) and tld_rng_dist\n"
                  "(32-byte aligned) structs through tld_rng_alloc, tld_rng_split and\n"
                  "tld_rng_dist_alloc and fails if any is misaligned.\n"
                  "With N: the same with N allocations per constructor (1..100000); prints the\n"
                  "number of misaligned allocations.\n"
                  "Example: unit-align 5000");
        if (argc > 1) {
                n = atoi(argv[1]);
                if (n < 1 || n > N_MAX) {
                        fprintf(stderr, "unit-align: N must be 1..%d\n", N_MAX);
                        return 2;
                }
        }
        SECTION_MSG("allocating over-aligned structs");
        KV_MSG("allocations", "%d per constructor", n);
        for (int i = 0; i < n; i++) {
                RUN(tld_rng_alloc(&rng[i], (uint64_t)(i + 1)));
                check(rng[i], 64, "tld_rng_alloc tld_rng", i);
                RUN(tld_rng_split(&rng2[i], rng[i]));
                check(rng2[i], 64, "tld_rng_split tld_rng", i);
                RUN(tld_rng_dist_alloc(&dist[i], w, 4));
                check(dist[i], 32, "tld_rng_dist_alloc tld_rng_dist", i);
                DEBUG_MSG("#%d: rng %p, rng2 %p, dist %p", i, (void *)rng[i], (void *)rng2[i], (void *)dist[i]);
                tld_progress(i + 1, n, "allocating");
        }
        for (int i = 0; i < n; i++) {
                tld_rng_dist_free(dist[i]);
                tld_rng_free(rng2[i]);
                tld_rng_free(rng[i]);
        }
        if (argc > 1) {
                fprintf(stdout, "%d of %d allocations misaligned\n", n_fail, 3 * n);
        }
        if (n_fail) {
                fprintf(stderr, "unit-align: %d misaligned allocation(s)\n", n_fail);
                return EXIT_FAILURE;
        }
        OK_MSG("all %d over-aligned structs correctly aligned", 3 * n);
        return EXIT_SUCCESS;
ERROR:
        fprintf(stderr, "unit-align: setup error\n");
        return EXIT_FAILURE;
}

/* unit-align: every heap-allocated struct that declares an alignment larger than
 * malloc's guarantee (16 bytes on 64-bit platforms) must actually be allocated at that
 * alignment. The struct declarations promise it to the compiler
 * (__attribute__((aligned(N)))); allocating them with plain malloc broke the promise,
 * which is undefined behaviour (UBSan: "member access within misaligned address ...
 * for type 'struct rng_state', which requires 64 byte alignment" in init_rng).
 *
 * Each constructor is called many times: plain malloc returns 16-byte-aligned blocks,
 * so the chance that 200 of them all land on a 64/128-byte boundary by accident is
 * negligible, and the test fails reliably on a regression. */
#include "tld.h"
#include <stdint.h>
#include <stdio.h>

#define N_ALLOC 200

static int n_fail = 0;

static void check(const void *p, uintptr_t align, const char *what, int i)
{
        if (((uintptr_t)p) % align != 0) {
                if (n_fail < 10) {
                        fprintf(stdout, "FAIL: %s #%d at %p is not %lu-byte aligned\n",
                                what, i, p, (unsigned long)align);
                }
                n_fail++;
        }
}

int main(void)
{
        struct rng_state *rng[N_ALLOC] = { NULL };
        struct rng_state *rng2[N_ALLOC] = { NULL };
        struct rng_dist *dist[N_ALLOC] = { NULL };
#ifndef BOOT
        struct alphabet *alpha[N_ALLOC] = { NULL };
        struct tl_seq_buffer *sb[N_ALLOC] = { NULL };
        struct tl_seq *seq[N_ALLOC] = { NULL };
#endif
        double w[4] = { 0.1, 0.2, 0.3, 0.4 };

        for (int i = 0; i < N_ALLOC; i++) {
                RUN(init_rng(&rng[i], (uint64_t)(i + 1)));
                check(rng[i], 64, "init_rng rng_state", i);
                RUN(init_rng_from_rng(&rng2[i], rng[i]));
                check(rng2[i], 64, "init_rng_from_rng rng_state", i);
                RUN(tl_random_sdist_init(&dist[i], w, 4, i + 1));
                check(dist[i], 32, "tl_random_sdist_init rng_dist", i);
#ifndef BOOT   /* the seq module is not built under BOOT (tld.h) */
                RUN(create_alphabet(&alpha[i], rng[i], TLALPHABET_DEFAULT_DNA));
                check(alpha[i], 128, "create_alphabet alphabet", i);
                RUN(alloc_tl_seq_buffer(&sb[i], 2));
                check(sb[i], 64, "alloc_tl_seq_buffer tl_seq_buffer", i);
                for (int j = 0; j < sb[i]->malloc_num; j++) {
                        check(sb[i]->sequences[j], 64, "alloc_tl_seq_buffer tl_seq", i);
                }
                RUN(alloc_tl_seq(&seq[i]));
                check(seq[i], 64, "alloc_tl_seq tl_seq", i);
#endif
        }
        for (int i = 0; i < N_ALLOC; i++) {
#ifndef BOOT
                free_tl_seq(seq[i]);
                free_tl_seq_buffer(sb[i]);
                free_alphabet(alpha[i]);
#endif
                tl_random_sdist_free(dist[i]);
                free_rng(rng2[i]);
                free_rng(rng[i]);
        }
        if (n_fail) {
                fprintf(stdout, "unit-align: %d misaligned allocation(s)\n", n_fail);
                return EXIT_FAILURE;
        }
        fprintf(stdout, "unit-align: all over-aligned structs correctly aligned\n");
        return EXIT_SUCCESS;
ERROR:
        fprintf(stdout, "unit-align: setup error\n");
        return EXIT_FAILURE;
}

/* rng: the xoshiro256** random number generator (seedable, and splittable for threads)
 * with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a
 * discrete distribution. A generator is not locked: use one per thread. */
#ifndef TLD_RNG_H
#define TLD_RNG_H

#include "../core/tld-core.h"

/* Opaque generator state. Create with init_rng or init_rng_from_rng, free with
 * free_rng. */
typedef struct rng_state rng_state;

/* Opaque alias table for sampling from a discrete distribution (tl_random_sdist_*). */
struct rng_dist;

/* Allocate a generator in *rng seeded with seed; the same seed gives the same sequence.
 * seed 0 picks a seed from /dev/urandom, mixed with the time, the process id and a
 * counter, so every generator made with seed 0 has its own stream, also across jobs
 * started at the same moment. Free with free_rng. */
tld_external int init_rng(struct rng_state **rng, uint64_t seed);

/* Allocate a generator for another thread in *target: it continues rng's current
 * stream, and rng itself jumps 2^128 draws ahead, so the two streams never overlap.
 * Free with free_rng. */
tld_external int init_rng_from_rng(struct rng_state** target, struct rng_state* rng);

/* Reseed an existing generator (seed is used as given, 0 included) and drop any cached
 * Gaussian deviate. */
tld_external int tld_rng_set_seed(struct rng_state *s, uint64_t seed);

/* Free a generator; NULL is ignored. */
tld_external void free_rng(struct rng_state* rng);

/* Uniform double on the open interval (0, 1): never exactly 0 or 1. */
tld_external double tl_random_double(struct rng_state* rng);

/* Normal deviate with mean mu and standard deviation sigma (Box-Muller; values are made
 * in pairs and every other call returns the cached second one). */
tld_external double tl_random_gaussian(struct rng_state* rng, double mu, double sigma);

/* Gamma deviate with the given shape (> 0, not checked) and scale; mean shape * scale. */
tld_external double tl_random_gamma(struct rng_state* rng, double shape, double scale);

/* Uniform integer in [0, a), without modulo bias; 0 when a <= 1. */
tld_external int tl_random_int(struct rng_state* rng,int a);

/* Build in *rng_dist an alias table that samples index i in [0, n) with probability
 * w[i] / sum(w). Weights must be >= 0 with a finite, positive sum. The table owns its
 * own generator seeded with seed (0 = arbitrary). Free with tl_random_sdist_free. */
tld_external int tl_random_sdist_init(struct rng_dist** rng_dist,double* w, int n,int seed);

/* One sample from the table: an index in [0, n). */
tld_external int tl_random_sdist_smpl(struct rng_dist* d);

/* Free an alias table and its generator; NULL is ignored. */
tld_external void tl_random_sdist_free(struct rng_dist* d);

#endif

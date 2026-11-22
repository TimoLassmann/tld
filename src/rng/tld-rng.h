/* rng: the xoshiro256** random number generator (seedable, and splittable for threads)
 * with uniform, integer, Gaussian and gamma draws, plus alias-table sampling from a
 * discrete distribution. A generator is not locked: use one per thread. */
#ifndef TLD_RNG_H
#define TLD_RNG_H

#include "../core/tld-core.h"

/* Opaque generator state. Create with tld_rng_alloc or tld_rng_split, free with
 * tld_rng_free. */
typedef struct tld_rng tld_rng;

/* Opaque alias table for sampling from a discrete distribution (tl_random_sdist_*). */
struct tld_rng_dist;

/* Allocate a generator in *rng seeded with seed; the same seed gives the same sequence.
 * seed 0 picks a seed from /dev/urandom, mixed with the time, the process id and a
 * counter, so every generator made with seed 0 has its own stream, also across jobs
 * started at the same moment. Free with tld_rng_free. */
TLD_API int tld_rng_alloc(struct tld_rng **rng, uint64_t seed);

/* Allocate a generator for another thread in *target: it continues rng's current
 * stream, and rng itself jumps 2^128 draws ahead, so the two streams never overlap.
 * Free with tld_rng_free. */
TLD_API int tld_rng_split(struct tld_rng** target, struct tld_rng* rng);

/* Reseed an existing generator (seed is used as given, 0 included) and drop any cached
 * Gaussian deviate. */
TLD_API int tld_rng_set_seed(struct tld_rng *s, uint64_t seed);

/* Free a generator; NULL is ignored. */
TLD_API void tld_rng_free(struct tld_rng* rng);

/* Uniform double on the open interval (0, 1): never exactly 0 or 1. The double results
 * below are NaN, and tld_rng_int / tld_rng_dist_sample give -1, for a NULL generator or
 * table (with an error). */
TLD_API double tld_rng_double(struct tld_rng* rng);

/* Normal deviate with mean mu and standard deviation sigma (Box-Muller; values are made
 * in pairs and every other call returns the cached second one). */
TLD_API double tld_rng_gaussian(struct tld_rng* rng, double mu, double sigma);

/* Gamma deviate with the given shape (> 0, not checked) and scale; mean shape * scale. */
TLD_API double tld_rng_gamma(struct tld_rng* rng, double shape, double scale);

/* Uniform integer in [0, a), without modulo bias; 0 when a <= 1. */
TLD_API int tld_rng_int(struct tld_rng* rng,int a);

/* Build in *tld_rng_dist an alias table that samples index i in [0, n) with probability
 * w[i] / sum(w). Weights must be >= 0 with a finite, positive sum. The table owns its
 * own generator seeded with seed (0 = arbitrary). Free with tld_rng_dist_free. */
TLD_API int tld_rng_dist_alloc(struct tld_rng_dist** dist,double* w, int n,int seed);

/* One sample from the table: an index in [0, n). */
TLD_API int tld_rng_dist_sample(struct tld_rng_dist* d);

/* Free an alias table and its generator; NULL is ignored. */
TLD_API void tld_rng_dist_free(struct tld_rng_dist* d);

#endif

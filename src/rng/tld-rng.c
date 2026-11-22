#include "tld-rng.h"
#include "../alloc/tld-alloc.h"

#include <math.h>
#include <float.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TLD__2PI ((2.0)*(M_PI))

/* code here was adopted from:  */

/*  Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

/* http://vigna.di.unimi.it/xorshift/xoshiro256starstar.c */
/* http://xoshiro.di.unimi.it/splitmix64.c */

/* written by: */
/* Sebastiano Vigna (vigna@acm.org) */
/* David Blackman */

/* Code for sampling from various distributions was taken from: */

/* Copyright 2005 Robert Kern (robert.kern@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* And code bits from  */
/* the easel library (by Sean Eddy) */

#include <stdint.h>

#define ALIGN64 64
struct tld_rng{
        uint64_t s[4];
        int has_gauss; /* !=0: gauss contains a gaussian deviate */
        double gauss;
} __attribute__((aligned(ALIGN64)));
#undef ALIGN64

#define ALIGN32 32
struct tld_rng_dist{
        double* p;
        int* a;
        int n;
} __attribute__((aligned(ALIGN32)));

#undef ALIGN32

static inline uint64_t tld__rng_rotl(uint64_t x,  uint64_t k);
static uint64_t tld__rng_next(struct tld_rng* s);
static void tld__rng_jump(struct tld_rng* s);

static uint64_t tld__rng_entropy_seed(void);

static double tld__rng_standard_gamma(struct tld_rng* rng, double shape);
static double tld__rng_standard_exponential(struct tld_rng* rng);
static double tld__rng_gauss(struct tld_rng* rng);

int tld_rng_alloc(struct tld_rng** rng,uint64_t seed)
{
        struct tld_rng* s = NULL;
        ASSERT(rng != NULL, "No output pointer");

        MMALLOC_ALIGNED(s, sizeof(struct tld_rng), _Alignof(struct tld_rng));
        s->gauss = 0.0;
        s->has_gauss = 0;

        if(!seed){
                seed = tld__rng_entropy_seed();
        }

        tld_rng_set_seed(s, seed);

        *rng = s;
        return OK;
ERROR:
        return FAIL;
}

static inline uint64_t tld__rng_rotl(const uint64_t x,  uint64_t k) {
        return (x << k) | (x >> (64U - k));
}

/* This is xoshiro256** 1.0, one of our all-purpose, rock-solid
   generators. It has excellent (sub-ns) speed, a state (256 bits) that is
   large enough for any parallel application, and it passes all tests we
   are aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

uint64_t tld__rng_next(struct tld_rng* s)
{
        const uint64_t result_starstar = tld__rng_rotl(s->s[1] * 5, 7) * 9;
        const uint64_t t = s->s[1] << 17U;

        s->s[2] ^= s->s[0];
        s->s[3] ^= s->s[1];
        s->s[1] ^= s->s[2];
        s->s[0] ^= s->s[3];

        s->s[2] ^= t;

        s->s[3] = tld__rng_rotl(s->s[3], 45);

        return result_starstar;
}

/* This is the jump function for the generator. It is equivalent
   to 2^128 calls to tld__rng_next(); it can be used to generate 2^128
   non-overlapping subsequences for parallel computations. */

void tld__rng_jump(struct tld_rng* s)
{
        static const uint64_t JUMP[] = { 0x180ec6d33cfd0aba,
                                         0xd5a61266f0c9392c,
                                         0xa9582618e03fc9aa,
                                         0x39abdc4529b1661c };

        uint64_t s0 = 0U;
        uint64_t s1 = 0U;
        uint64_t s2 = 0U;
        uint64_t s3 = 0U;
        for(uint64_t i = 0; i < sizeof JUMP / sizeof *JUMP; i++){
                for(uint64_t b = 0; b < 64; b++) {
                        if (JUMP[i] & UINT64_C(1) << b) {
                                s0 ^= s->s[0];
                                s1 ^= s->s[1];
                                s2 ^= s->s[2];
                                s3 ^= s->s[3];
                        }
                        tld__rng_next(s);
                }
        }

        s->s[0] = s0;
        s->s[1] = s1;
        s->s[2] = s2;
        s->s[3] = s3;
}

int tld_rng_split(struct tld_rng** target, struct tld_rng* rng)
{
        struct tld_rng* s = NULL;
        int i = 0;
        ASSERT(rng != NULL, "No generator");
        ASSERT(target != NULL, "No output pointer");
        MMALLOC_ALIGNED(s, sizeof(struct tld_rng), _Alignof(struct tld_rng));

        for(i = 0; i < 4;i++){
                s->s[i] = rng->s[i];
        }
        /* Do not inherit cached deviates: the child must not repeat the
           parent's next gaussian. */
        s->gauss = 0.0;
        s->has_gauss = 0;
        tld__rng_jump(rng);
        *target = s;
        return OK;
ERROR:
        return FAIL;
}

int tld_rng_set_seed(struct tld_rng *s, uint64_t seed)
{
        uint64_t sanity = 0U;
        uint64_t z = 0U;

        ASSERT(s != NULL, "No rng allocated in set seed");
        s->gauss = 0.0;
        s->has_gauss = 0;

        while(!sanity){
                sanity = 0;
                z = (seed += 0x9e3779b97f4a7c15);
                z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9;
                z = (z ^ (z >> 27U)) * 0x94d049bb133111eb;
                s->s[0] = z ^ (z >> 31U);
                if(s->s[0]){
                        sanity++;
                }
                z = (seed += 0x9e3779b97f4a7c15);
                z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9;
                z = (z ^ (z >> 27U)) * 0x94d049bb133111eb;
                s->s[1] = z ^ (z >> 31U);
                if(s->s[1]){
                        sanity++;
                }

                z = (seed += 0x9e3779b97f4a7c15);
                z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9;
                z = (z ^ (z >> 27U)) * 0x94d049bb133111eb;
                s->s[2] = z ^ (z >> 31U);
                if(s->s[2]){
                        sanity++;
                }

                z = (seed += 0x9e3779b97f4a7c15);
                z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9;
                z = (z ^ (z >> 27U)) * 0x94d049bb133111eb;
                s->s[3] = z ^ (z >> 31U);
                if(s->s[3]){
                        sanity++;
                }
        }

        return OK;
ERROR:
        return FAIL;
}

void tld_rng_free(struct tld_rng* rng)
{
        if(rng){
                MFREE(rng);
        }
}
/* The splitmix64 output function: spreads the bits of z over all 64 bits. */
static uint64_t tld__rng_mix64(uint64_t z)
{
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
}

/* The seed tld_rng_alloc uses for seed 0: 64 bits from /dev/urandom, mixed with the time in
 * nanoseconds, the process id and a count of the seeds made so far. Generators made
 * in one loop, or by jobs started at the same moment, therefore get different seeds,
 * even if /dev/urandom cannot be read. */
static uint64_t tld__rng_entropy_seed(void)
{
        static _Atomic uint64_t n_made = 0;
        struct timespec now = {0};
        uint64_t seed = 0U;
        FILE *f = fopen("/dev/urandom", "rb");

        if(f){
                if(fread(&seed, sizeof seed, 1, f) != 1){
                        seed = 0U;
                }
                fclose(f);
        }
        if(timespec_get(&now, TIME_UTC) != TIME_UTC){
                now.tv_sec = time(NULL);
        }
        seed ^= tld__rng_mix64((uint64_t) now.tv_sec * 1000000000ULL + (uint64_t) now.tv_nsec);
        seed ^= tld__rng_mix64(((uint64_t) getpid() << 32U) ^ atomic_fetch_add(&n_made, 1));
        return tld__rng_mix64(seed);
}

/* Uniform on the open interval (0,1): the top 53 bits of a draw give a
   multiple of 2^-53 in [0, 1 - 2^-53]; zero is rejected. */
double tld_rng_double(struct tld_rng* rng)
{
        uint64_t x = 0U;
        ASSERT(rng != NULL, "No generator");
        do{
                x = tld__rng_next(rng) >> 11U;
        }while (x == 0U);
        return (double) x * 0x1.0p-53;
ERROR:
        return NAN;
}

double tld_rng_gaussian(struct tld_rng* rng, double mu, double sigma)
{
        ASSERT(rng != NULL, "No generator");
        return mu + sigma * tld__rng_gauss(rng);
ERROR:
        return NAN;
}

double tld__rng_standard_gamma(struct tld_rng* rng, double shape)
{
        double b = 0.0;
        double c = 0.0;
        double U = 0.0;
        double V = 0.0;
        double X = 0.0;
        double Y = 0.0;
        double r = 0.0;
        if (shape == 1.0){
                r = tld__rng_standard_exponential(rng);
        }else if (shape < 1.0){
                for (;;){
                        U = tld_rng_double(rng);
                        V = tld__rng_standard_exponential(rng);
                        if (U <= 1.0 - shape){
                                X = pow(U, 1./shape);
                                if (X <= V){
                                        r = X;
                                        break;
                                }
                        }else{
                                Y = -log((1-U)/shape);
                                X = pow(1.0 - shape + shape*Y, 1./shape);
                                if (X <= (V + Y)){
                                        r = X;
                                        break;
                                }
                        }
                }
        }else{
                b = shape - 1./3.;
                c = 1./sqrt(9*b);
                for (;;){
                        do
                        {
                                X = tld__rng_gauss(rng);
                                V = 1.0 + c*X;
                        } while (V <= 0.0);

                        V = V*V*V;
                        U = tld_rng_double(rng);
                        if (U < 1.0 - 0.0331*(X*X)*(X*X)){
                                r = b*V;
                                break;
                        }
                        if (log(U) < 0.5*X*X + b*(1. - V + log(V))){
                                r = b*V;
                                break;
                        }
                }
        }
        return r;
}

double tld__rng_standard_exponential(struct tld_rng* rng)
{
    /* We use -log(1-U) since U is [0, 1) */
        return -log(1.0 - tld_rng_double(rng));
}

double tld__rng_gauss(struct tld_rng* rng)
{
        double r = 0.0;
        if (rng->has_gauss) {
                const double tmp = rng->gauss;
                rng->gauss = 0;
                rng->has_gauss = 0;
                r = tmp;
        } else {
                double f = 0.0;
                double x1 = 0.0;
                double x2 = 0.0;
                double r2 = 0.0;

                do {
                        x1 = 2.0*tld_rng_double(rng) - 1.0;
                        x2 = 2.0*tld_rng_double(rng) - 1.0;
                        r2 = x1*x1 + x2*x2;
                }
                while (r2 >= 1.0 || r2 == 0.0);

                /* Box-Muller transform */
                f = sqrt(-2.0*log(r2)/r2);
                /* Keep for next call */
                rng->gauss = f*x1;
                rng->has_gauss = 1;
                r = f*x2;
        }
        return r;
}

double tld_rng_gamma(struct tld_rng* rng, double shape, double scale)
{
        ASSERT(rng != NULL, "No generator");
        return scale * tld__rng_standard_gamma(rng, shape);
ERROR:
        return NAN;
}

/* Uniform integer in [0, a). Unbiased: draws below 2^64 mod a are
   rejected so that every residue is equally likely. Returns 0 if a <= 1. */
int tld_rng_int(struct tld_rng* rng,int a)
{
        uint64_t n = 0U;
        uint64_t threshold = 0U;
        uint64_t x = 0U;
        ASSERT(rng != NULL, "No generator");
        if(a <= 1){
                return 0;
        }
        n = (uint64_t) a;
        threshold = ((uint64_t) 0 - n) % n;
        do{
                x = tld__rng_next(rng);
        }while(x < threshold);
        return (int) (x % n);
ERROR:
        return -1;
}

int tld_rng_dist_alloc(struct tld_rng_dist** dist, double* w, int n)
{
        struct tld_rng_dist* d = NULL;

        double* p = NULL;
        int* s = NULL;
        int* l = NULL;
        int n_s = 0;
        int n_l = 0;
        double sum = 0.0;
        int a = 0;
        int g = 0;
        ASSERT(dist != NULL, "No dist pointer");
        ASSERT(w != NULL, "No weights");
        ASSERT(n > 0, "Number of weights must be positive (got %d)", n);
        MMALLOC_ALIGNED(d, sizeof(struct tld_rng_dist), _Alignof(struct tld_rng_dist));
        d->p = NULL;
        d->a = NULL;
        d->n = n;

        galloc(&d->p, d->n);
        galloc(&d->a, d->n);

        galloc(&p,n);
        galloc(&s,n);
        galloc(&l,n);

        sum = 0.0;

        for(int i = 0; i < n;i++){
                if(!(w[i] >= 0.0)){
                        ERROR_MSG("invalid weight: w[%d] = %f", i, w[i]);
                }
                sum += w[i];
        }
        ASSERT(sum > 0.0 && isfinite(sum), "Sum of weights is zero or not finite.");

        for(int i = 0; i < n;i++){
                p[i] = w[i] * (double) n / sum;
        }
        n_s = 0;
        n_l = 0;

        for (int i= n-1; i >= 0; --i ){
                if(p[i] < 1.0){
                        s[n_s] = i;
                        n_s++;
                }else{
                        l[n_l] = i;
                        n_l++;
                }
        }

        while(n_s && n_l){
                n_s--;
                a = s[n_s];
                n_l--;
                g = l[n_l];

                d->p[a] = p[a];
                d->a[a] = g;
                p[g] = p[g] + p[a] - 1.0;
                if(p[g] < 1.0){
                        s[n_s] = g;
                        n_s++;
                }else{
                        l[n_l] = g;
                        n_l++;
                }
        }
        while(n_l){
                d->p[l[--n_l]] = 1.0;
        }
        while(n_s){
                d->p[s[--n_s]] = 1.0;
        }
        gfree(p);
        gfree(s);
        gfree(l);

        *dist = d;
        return OK;
ERROR:
        if(p){
                gfree(p);
        }
        if(s){
                gfree(s);
        }
        if(l){
                gfree(l);
        }
        tld_rng_dist_free(d);
        return FAIL;
}

int tld_rng_dist_sample(struct tld_rng_dist* d, struct tld_rng* rng)
{
        double r1 = 0.0;
        double r2 = 0.0;
        int i = 0;
        ASSERT(d != NULL, "No table");
        ASSERT(rng != NULL, "No generator");
        r1 = tld_rng_double(rng);
        r2 = tld_rng_double(rng);
        i = (int) (d->n * r1);
        if(i >= d->n){
                i = d->n - 1;
        }
        return r2 < d->p[i] ? i : d->a[i];
ERROR:
        return -1;
}

void tld_rng_dist_free(struct tld_rng_dist* d)
{
        if(d){
                if(d->p){
                        gfree(d->p);
                }
                if(d->a){
                        gfree(d->a);
                }
                MFREE(d);
        }
}

#undef TLD__2PI

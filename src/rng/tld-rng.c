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

#define M_2PI ((2.0)*(M_PI))

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
struct rng_state{
        uint64_t s[4];
        uint8_t gen;
        double z1;
        int has_gauss; /* !=0: gauss contains a gaussian deviate */
        double gauss;
} __attribute__((aligned(ALIGN64)));
#undef ALIGN64

#define ALIGN32 32
struct rng_dist{
        struct rng_state* rng;
        double* p;
        int* a;
        int n;
} __attribute__((aligned(ALIGN32)));

#undef ALIGN32

static inline uint64_t rotl(uint64_t x,  uint64_t k);
static uint64_t next(struct rng_state* s);
void jump(struct rng_state* s);

static uint64_t choose_arbitrary_seed(void);

static double tl_standard_gamma(struct rng_state* rng, double shape);
static double tl_standard_exponential(struct rng_state* rng);
static double tl_gauss(struct rng_state* rng);

int init_rng(struct rng_state** rng,uint64_t seed)
{
        struct rng_state* s = NULL;

        MMALLOC_ALIGNED(s, sizeof(struct rng_state), _Alignof(struct rng_state));
        s->gen = 0;
        s->z1 = 0.0F;
        s->gauss = 0.0;
        s->has_gauss = 0;

        if(!seed){
                seed = choose_arbitrary_seed();
        }

        tld_rng_set_seed(s, seed);

        *rng = s;
        return OK;
ERROR:
        return FAIL;
}

static inline uint64_t rotl(const uint64_t x,  uint64_t k) {
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

uint64_t next(struct rng_state* s)
{
        const uint64_t result_starstar = rotl(s->s[1] * 5, 7) * 9;
        const uint64_t t = s->s[1] << 17U;

        s->s[2] ^= s->s[0];
        s->s[3] ^= s->s[1];
        s->s[1] ^= s->s[2];
        s->s[0] ^= s->s[3];

        s->s[2] ^= t;

        s->s[3] = rotl(s->s[3], 45);

        return result_starstar;
}

/* This is the jump function for the generator. It is equivalent
   to 2^128 calls to next(); it can be used to generate 2^128
   non-overlapping subsequences for parallel computations. */

void jump(struct rng_state* s)
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
                        next(s);
                }
        }

        s->s[0] = s0;
        s->s[1] = s1;
        s->s[2] = s2;
        s->s[3] = s3;
}

int init_rng_from_rng(struct rng_state** target, struct rng_state* rng)
{
        struct rng_state* s = NULL;
        int i = 0;
        MMALLOC_ALIGNED(s, sizeof(struct rng_state), _Alignof(struct rng_state));

        for(i = 0; i < 4;i++){
                s->s[i] = rng->s[i];
        }
        /* Do not inherit cached deviates: the child must not repeat the
           parent's next gaussian. */
        s->gen = 0;
        s->z1 = 0.0;
        s->gauss = 0.0;
        s->has_gauss = 0;
        jump(rng);
        *target = s;
        return OK;
ERROR:
        return FAIL;
}

int tld_rng_set_seed(struct rng_state *s, uint64_t seed)
{
        uint64_t sanity = 0U;
        uint64_t z = 0U;

        ASSERT(s != NULL, "No rng allocated in set seed");
        s->gen = 0;
        s->z1 = 0.0F;
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

void free_rng(struct rng_state* rng)
{
        if(rng){
                MFREE(rng);
        }
}
/* The splitmix64 output function: spreads the bits of z over all 64 bits. */
static uint64_t mix64(uint64_t z)
{
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
}

/* The seed init_rng uses for seed 0: 64 bits from /dev/urandom, mixed with the time in
 * nanoseconds, the process id and a count of the seeds made so far. Generators made
 * in one loop, or by jobs started at the same moment, therefore get different seeds,
 * even if /dev/urandom cannot be read. */
static uint64_t choose_arbitrary_seed(void)
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
        seed ^= mix64((uint64_t) now.tv_sec * 1000000000ULL + (uint64_t) now.tv_nsec);
        seed ^= mix64(((uint64_t) getpid() << 32U) ^ atomic_fetch_add(&n_made, 1));
        return mix64(seed);
}

/* Uniform on the open interval (0,1): the top 53 bits of a draw give a
   multiple of 2^-53 in [0, 1 - 2^-53]; zero is rejected. */
double tl_random_double(struct rng_state* rng)
{
        uint64_t x = 0U;
        do{
                x = next(rng) >> 11U;
        }while (x == 0U);
        return (double) x * 0x1.0p-53;
}

/* from:  */
/* https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform */
double tl_random_gaussian(struct rng_state* rng, double mu, double sigma)
{
        double u1 = 0.0;
        double u2 = 0.0;
        double z0 = 0.0;
        const double n2 = -2.0;
        rng->gen = !rng->gen;

        if (!rng->gen){
                return rng->z1 * sigma + mu;
        }

        do{
                u1 = tl_random_double(rng);
                u2 = tl_random_double(rng);
        } while (u1 <= DBL_EPSILON);

        z0 = sqrt(n2 * log(u1)) * cos(M_2PI * u2);
        rng->z1 = sqrt(n2 * log(u1)) * sin(M_2PI * u2);

        return z0 * sigma + mu;
}

double tl_standard_gamma(struct rng_state* rng, double shape)
{
        double b = 0.0;
        double c = 0.0;
        double U = 0.0;
        double V = 0.0;
        double X = 0.0;
        double Y = 0.0;
        double r = 0.0;
        if (shape == 1.0){
                r = tl_standard_exponential(rng);
        }else if (shape < 1.0){
                for (;;){
                        U = tl_random_double(rng);
                        V = tl_standard_exponential(rng);
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
                                X = tl_gauss(rng);
                                V = 1.0 + c*X;
                        } while (V <= 0.0);

                        V = V*V*V;
                        U = tl_random_double(rng);
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

double tl_standard_exponential(struct rng_state* rng)
{
    /* We use -log(1-U) since U is [0, 1) */
        return -log(1.0 - tl_random_double(rng));
}

double tl_gauss(struct rng_state* rng)
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
                        x1 = 2.0*tl_random_double(rng) - 1.0;
                        x2 = 2.0*tl_random_double(rng) - 1.0;
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

double tl_random_gamma(struct rng_state* rng, double shape, double scale)
{
        return scale * tl_standard_gamma(rng, shape);
}

/* Uniform integer in [0, a). Unbiased: draws below 2^64 mod a are
   rejected so that every residue is equally likely. Returns 0 if a <= 1. */
int tl_random_int(struct rng_state* rng,int a)
{
        uint64_t n = 0U;
        uint64_t threshold = 0U;
        uint64_t x = 0U;
        if(a <= 1){
                return 0;
        }
        n = (uint64_t) a;
        threshold = ((uint64_t) 0 - n) % n;
        do{
                x = next(rng);
        }while(x < threshold);
        return (int) (x % n);
}

int tl_random_sdist_init(struct rng_dist** rng_dist,double* w, int n,int seed)
{
        struct rng_dist* d = NULL;

        double* p = NULL;
        int* s = NULL;
        int* l = NULL;
        int n_s = 0;
        int n_l = 0;
        double sum = 0.0;
        int a = 0;
        int g = 0;
        ASSERT(rng_dist != NULL, "No rng_dist pointer");
        ASSERT(w != NULL, "No weights");
        ASSERT(n > 0, "Number of weights must be positive (got %d)", n);
        MMALLOC_ALIGNED(d, sizeof(struct rng_dist), _Alignof(struct rng_dist));
        d->p = NULL;
        d->a = NULL;
        d->rng = NULL;
        d->n = n;

        galloc(&d->p, d->n);
        galloc(&d->a, d->n);
        RUN( init_rng(&d->rng, seed));

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

        *rng_dist = d;
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
        tl_random_sdist_free(d);
        return FAIL;
}

int tl_random_sdist_smpl(struct rng_dist* d)
{
        const double r1 = tl_random_double(d->rng);
        const double r2 = tl_random_double(d->rng);
        int i = (int) (d->n * r1);
        if(i >= d->n){
                i = d->n - 1;
        }
        return r2 < d->p[i] ? i : d->a[i];
}

void tl_random_sdist_free(struct rng_dist* d)
{
        if(d){
                if(d->p){
                        gfree(d->p);
                }
                if(d->a){
                        gfree(d->a);
                }
                free_rng(d->rng);
                MFREE(d);
        }
}

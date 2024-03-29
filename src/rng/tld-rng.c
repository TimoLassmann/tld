#include "tld-rng.h"
#include "../alloc/tld-alloc.h"

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <time.h>

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

/* code for sampling from varius distributions was taken from  */

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
void long_jump(struct rng_state* s);

static uint64_t choose_arbitrary_seed(void);
static uint32_t jenkins_mix3(uint32_t a, uint32_t b, uint32_t c);

static double tl_standard_gamma(struct rng_state* rng, double shape);
static double tl_standard_exponential(struct rng_state* rng);
static double tl_gauss(struct rng_state* rng);

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
        MMALLOC(d, sizeof(struct rng_dist));
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
                if(w[i] < 0){
                        ERROR_MSG("invalid p : w[i] = %f ", w[i]);
                }
                sum += w[i];
        }
        ASSERT(sum > 0.0, "Sum is zero.");

        for(int i = 0; i < n;i++){
                p[i] = w[i] * (double) n / sum;
                fprintf(stdout,"%f\n", p[i]);
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
        for(int i = 0; i < n;i++){
                fprintf(stdout,"%f %d\n",d->p[i],d->a[i]);
        }
        gfree(p);
        gfree(s);
        gfree(l);

        *rng_dist = d;
        return OK;
ERROR:
        gfree(p);
        gfree(s);
        gfree(l);
        return FAIL;
}

int tl_random_sdist_smpl(struct rng_dist* d)
{
        const double r1 = tl_random_double(d->rng);
        const double r2 = tl_random_double(d->rng);
        const int i = (int) (d->n * r1);
        return r2 < d->p[i] ? i : d->a[i];
}


void tl_random_sdist_free(struct rng_dist* d)
{
        if(d){
                gfree(d->p);
                gfree(d->a);
                free_rng(d->rng);
                MFREE(d);
        }
}

double tl_random_double(struct rng_state* rng)
{
        const double m = 18446744073709551616.0;
        uint64_t x = 0U;
        double y = 0.0;
        do{
                x = next(rng);
                y = ((double) x / m);
        }while (y == 0.0);
        return y;
}

int tl_random_int(struct rng_state* rng,int a)
{
        return (int) (tl_random_double(rng) * a);
}

/* from:  */
//https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
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
                u1 = tl_random_double(rng);//  rand() * (1.0 / RAND_MAX);
                u2 = tl_random_double(rng);//rand() * (1.0 / RAND_MAX);
        } while (u1 <= DBL_EPSILON);


        z0 = sqrt(n2 * log(u1)) * cos(M_2PI * u2);
        rng->z1 = sqrt(n2 * log(u1)) * sin(M_2PI * u2);

        return z0 * sigma + mu;
}

double tl_random_gamma(struct rng_state* rng, double shape, double scale)
{
        return scale * tl_standard_gamma(rng, shape);
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
                /* return tmp; */
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


int init_rng(struct rng_state** rng,uint64_t seed)
{
        struct rng_state* s = NULL;
        /* uint64_t z = 0U; */
        /* uint64_t sanity = 0U; */

        MMALLOC(s, sizeof(struct rng_state));
        s->gen = 0;
        s->z1 = 0.0F;
        s->gauss = 0.0;
        s->has_gauss = 0;

        if(!seed){
                seed = choose_arbitrary_seed();
        }

        tld_rng_set_seed(s, seed);

        /* sanity = 0; */

        /* while(!sanity){ */
        /*         sanity = 0; */
        /*         z = (seed += 0x9e3779b97f4a7c15); */
        /*         z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9; */
        /*         z = (z ^ (z >> 27U)) * 0x94d049bb133111eb; */
        /*         s->s[0] = z ^ (z >> 31U); */
        /*         if(s->s[0]){ */
        /*                 sanity++; */
        /*         } */
        /*         z = (seed += 0x9e3779b97f4a7c15); */
        /*         z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9; */
        /*         z = (z ^ (z >> 27U)) * 0x94d049bb133111eb; */
        /*         s->s[1] = z ^ (z >> 31U); */
        /*         if(s->s[1]){ */
        /*                 sanity++; */
        /*         } */

        /*         z = (seed += 0x9e3779b97f4a7c15); */
        /*         z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9; */
        /*         z = (z ^ (z >> 27U)) * 0x94d049bb133111eb; */
        /*         s->s[2] = z ^ (z >> 31U); */
        /*         if(s->s[2]){ */
        /*                 sanity++; */
        /*         } */

        /*         z = (seed += 0x9e3779b97f4a7c15); */
        /*         z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9; */
        /*         z = (z ^ (z >> 27U)) * 0x94d049bb133111eb; */
        /*         s->s[3] = z ^ (z >> 31U); */
        /*         if(s->s[3]){ */
        /*                 sanity++; */
        /*         } */
        /* } */
        *rng = s;
        return OK;
ERROR:
        return FAIL;
}

int tld_rng_set_seed(struct rng_state *s, uint64_t seed)
{
        uint64_t sanity = 0U;
        uint64_t z = 0U;

        s->gen = 0;
        s->z1 = 0.0F;
        s->gauss = 0.0;
        s->has_gauss = 0;

        ASSERT(s != NULL, "No rng allocated in set seed");
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

int init_rng_from_rng(struct rng_state** target, struct rng_state* rng)
{
        struct rng_state* s = NULL;
        int i = 0;
        MMALLOC(s, sizeof(struct rng_state));

        for(i = 0; i < 4;i++){
                s->s[i] = rng->s[i];
                s->gen = 0;
                s->z1 = 0.0;
                s->gauss = rng->gauss;
                s->has_gauss = rng->has_gauss;

        }
        jump(rng);
        *target = s;
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
/* Taken from easel library (by Sean Eddy) */
static uint64_t choose_arbitrary_seed(void)
{
        uint32_t a = (uint32_t) time ((time_t *) NULL);
        uint32_t b = 87654321;	 // we'll use getpid() below, if we can
        uint32_t c = (uint32_t) clock();  // clock() gives time since process invocation, in msec at least, if not usec
        uint64_t seed = 0U;
#ifdef HAVE_GETPID
        b  = (uint32_t) getpid();	 // preferable b choice, if we have POSIX getpid()
#endif
        seed = jenkins_mix3(a,b,c);    // try to decorrelate closely spaced choices of pid/times
        return (seed == 0) ? 42 : seed; /* 42 is entirely arbitrary, just to avoid seed==0. */
}

/* jenkins_mix3()
 *
 * from Bob Jenkins: given a,b,c, generate a number that's distributed
 * reasonably uniformly on the interval 0..2^32-1 even for closely
 * spaced choices of a,b,c.
 */
static uint32_t jenkins_mix3(uint32_t a, uint32_t b, uint32_t c)
{
        a -= b; a -= c; a ^= (c>>13U);
        b -= c; b -= a; b ^= (a<<8U);
        c -= a; c -= b; c ^= (b>>13U);
        a -= b; a -= c; a ^= (c>>12U);
        b -= c; b -= a; b ^= (a<<16U);
        c -= a; c -= b; c ^= (b>>5U);
        a -= b; a -= c; a ^= (c>>3U);
        b -= c; b -= a; b ^= (a<<10U);
        c -= a; c -= b; c ^= (b>>15U);
        return c;
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



/* This is the long-jump function for the generator. It is equivalent to
   2^192 calls to next(); it can be used to generate 2^64 starting points,
   from each of which jump() will generate 2^64 non-overlapping
   subsequences for parallel distributed computations. */

void long_jump(struct rng_state* s)
{
        static const uint64_t LONG_JUMP[] = { 0x76e15d3efefdcbbf,
                                              0xc5004e441c522fb3,
                                              0x77710069854ee241,
                                              0x39109bb02acbe635 };

        uint64_t s0 = 0U;
        uint64_t s1 = 0U;
        uint64_t s2 = 0U;
        uint64_t s3 = 0U;
        for(uint64_t i = 0; i < sizeof LONG_JUMP / sizeof *LONG_JUMP; i++){
                for(uint64_t b = 0; b < 64; b++) {
                        if (LONG_JUMP[i] & UINT64_C(1) << b) {
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

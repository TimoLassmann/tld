/* unit-bpm: tld_bpm_best and tld_bpm_find against a plain dynamic-programming reference.
 *
 *   - every kernel (word, block, and avx2/neon where the CPU has them) gives the same
 *     matches and the same tld_bpm_distance as the reference, for patterns of 1 to 1000
 *     bytes (each side of the word boundaries up to 4 words, 5 and 6 words, and 16
 *     words), texts from empty to longer than the pattern, alphabets of 2, 4, 20 and 256
 *     byte values (20 and 256 for patterns over 4 words), k from 0 to above the pattern
 *     length, with and without TLD_BPM_NOCASE;
 *   - checks that do not depend on how matches are chosen: matches are in order, do not
 *     overlap and are within k, and tld_bpm_best's distance is the smallest of any
 *     stretch of text; when a match differs from the reference, its distance is
 *     recomputed (the reference's are exact), to say which side is wrong;
 *   - the reference computes each match start once per case and shares it across k, so
 *     the test stays a correctness check under the sanitizers, not a benchmark;
 *   - hand-worked cases (Levenshtein distances, repeats, overlaps, case, bytes 0 and 255, empty text, k above
 *     the pattern length) and argument errors;
 *   - matches ending on and around the boundaries of the kernels' 1024-position chunks,
 *     in texts where most chunks are passed over;
 *   - a 1 MB text with mutated copies of a pattern planted in it: every copy is found,
 *     and both kernels give the same matches.
 *
 *   unit-bpm PATTERN TEXT [K]    the kernel, the best match and the matches (K default 1)
 */
#include "tld.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int n_fail = 0;
static int n_check = 0;

#define CHECK(cond, ...)                                                        \
        do {                                                                    \
                n_check++;                                                      \
                if(!(cond)){                                                    \
                        if(n_fail < 25){                                        \
                                fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
                                fprintf(stderr, __VA_ARGS__);                   \
                                fputc('\n', stderr);                            \
                        }                                                       \
                        n_fail++;                                               \
                }                                                               \
        } while(0)

/* ------------------------------------------------------------------------ */
/* reference                                                                */
/* ------------------------------------------------------------------------ */

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static uint64_t rnd(void)
{
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 7;
        rng_state ^= rng_state << 17;
        return rng_state;
}
static int rnd_int(int n) { return (int)(rnd() % (uint64_t)n); }

static int fold(int c, int nocase)
{
        return (nocase && c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static int min3(int a, int b, int c)
{
        int m = a < b ? a : b;
        return m < c ? m : c;
}

/* d[e] for e = 0 .. n: the smallest distance of p to a stretch of t ending before e */
static int *ref_ends(const uint8_t *p, int m, const uint8_t *t, int64_t n, int nocase)
{
        int *d = malloc(sizeof(int) * (size_t)(n + 1));
        int *col = malloc(sizeof(int) * (size_t)(m + 1));
        for(int j = 0; j <= m; j++) col[j] = j;
        d[0] = m;
        for(int64_t i = 0; i < n; i++){
                int diag = col[0];
                col[0] = 0;
                for(int j = 1; j <= m; j++){
                        int up = col[j];
                        int c = fold(p[j - 1], nocase) == fold(t[i], nocase) ? 0 : 1;
                        col[j] = min3(diag + c, up + 1, col[j - 1] + 1);
                        diag = up;
                }
                d[i + 1] = col[m];
        }
        free(col);
        return d;
}

/* Levenshtein distance of p and t[0, n) */
static int ref_ed(const uint8_t *p, int m, const uint8_t *t, int64_t n, int nocase)
{
        int *col = malloc(sizeof(int) * (size_t)(m + 1));
        int r;
        for(int j = 0; j <= m; j++) col[j] = j;
        for(int64_t i = 0; i < n; i++){
                int diag = col[0];
                col[0] = (int)(i + 1);
                for(int j = 1; j <= m; j++){
                        int up = col[j];
                        int c = fold(p[j - 1], nocase) == fold(t[i], nocase) ? 0 : 1;
                        col[j] = min3(diag + c, up + 1, col[j - 1] + 1);
                        diag = up;
                }
        }
        r = col[m];
        free(col);
        return r;
}

/* the largest j <= min(e, m + d) with distance(p, t[e - j, e)) == d, by one DP over the
 * reversed text before e (row m after j bytes is that distance) */
static int64_t ref_start(const uint8_t *p, int m, const uint8_t *t, int64_t e, int d, int nocase)
{
        int64_t len = (int64_t)m + d;
        int64_t best = d == m ? 0 : -1;
        int *col = malloc(sizeof(int) * (size_t)(m + 1));
        if(len > e) len = e;
        for(int j = 0; j <= m; j++) col[j] = j;
        for(int64_t i = 0; i < len; i++){
                const uint8_t c_t = t[e - 1 - i];
                int diag = col[0];
                col[0] = (int)(i + 1);
                for(int j = 1; j <= m; j++){
                        int up = col[j];
                        int c = fold(p[m - j], nocase) == fold(c_t, nocase) ? 0 : 1;
                        col[j] = min3(diag + c, up + 1, col[j - 1] + 1);
                        diag = up;
                }
                if(col[m] == d) best = i + 1;
        }
        free(col);
        return e - best;
}

static int better(const tld_bpm_hit *a, const tld_bpm_hit *b)
{
        if(a->dist != b->dist) return a->dist < b->dist;
        if(a->start != b->start) return a->start < b->start;
        return a->end > b->end;
}

/* the matches as tld_bpm_find documents them, from the reference distances d; start[e]
 * caches ref_start for end e (-1: not computed yet), which depends on e and d[e] only, so
 * the calls for several k share it */
static int64_t ref_find(const uint8_t *p, int m, const uint8_t *t, int64_t n, const int *d, int k,
                        int nocase, int64_t *start, tld_bpm_hit *out)
{
        int64_t nh = 0;
        if(k > m) k = m;
        for(int64_t e = 1; e <= n; e++){
                int right = e < n ? d[e + 1] : 1 << 30;
                tld_bpm_hit h;
                int64_t j;
                int keep = 1;
                if(!(d[e] <= k && d[e] <= d[e - 1] && d[e] <= right)) continue;
                h.end = e;
                h.dist = d[e];
                if(start[e] < 0) start[e] = ref_start(p, m, t, e, d[e], nocase);
                h.start = start[e];
                j = nh;
                while(j > 0 && out[j - 1].end > h.start){
                        if(!better(&h, &out[j - 1])){ keep = 0; break; }
                        j--;
                }
                if(!keep) continue;
                nh = j;
                out[nh++] = h;
        }
        return nh;
}

/* ------------------------------------------------------------------------ */
/* one case: every kernel against the reference                             */
/* ------------------------------------------------------------------------ */

static void check_case(const uint8_t *p, int m, const uint8_t *t, int64_t n, const int *ks, int nk,
                       int flags, const char *what)
{
        const int nocase = flags & TLD_BPM_NOCASE;
        int *d = ref_ends(p, m, t, n, nocase);
        int64_t *start = malloc(sizeof(int64_t) * (size_t)(n + 1));
        const int whole = ref_ed(p, m, t, n, nocase);   /* the distance of the whole text */
        tld_bpm_hit *ref[8];
        int64_t n_ref[8];
        int dmin = m;
        for(int64_t e = 1; e <= n; e++) if(d[e] < dmin) dmin = d[e];
        for(int64_t e = 0; e <= n; e++) start[e] = -1;
        /* the reference matches for each k, once for both kernels */
        for(int ik = 0; ik < nk; ik++){
                ref[ik] = malloc(sizeof(tld_bpm_hit) * (size_t)(n + 1));
                n_ref[ik] = n ? ref_find(p, m, t, n, d, ks[ik], nocase, start, ref[ik]) : 0;
        }

        for(int plain = 0; plain < 2; plain++){
                tld_bpm *b = NULL;
                tld_bpm_hit best;
                const char *kern;
                if(tld_bpm_alloc(&b, (const char *)p, m, flags | (plain ? TLD_BPM_PLAIN : 0)) != OK){
                        CHECK(0, "%s: alloc m=%d", what, m);
                        continue;
                }
                kern = tld_bpm_kernel(b);
                if(m <= 64) CHECK(!strcmp(kern, "word"), "%s: m=%d kernel %s", what, m, kern);
                else if(plain) CHECK(!strcmp(kern, "block"), "%s: m=%d plain kernel %s", what, m, kern);

                /* best */
                CHECK(tld_bpm_best(b, (const char *)t, n, &best) == OK, "%s: best failed", what);
                if(n == 0){
                        CHECK(best.start == 0 && best.end == 0 && best.dist == m, "%s: best on empty text", what);
                }else{
                        tld_bpm_hit r1[1];
                        int64_t nr;
                        tld_bpm_hit *all = malloc(sizeof(tld_bpm_hit) * (size_t)(n + 1));
                        CHECK(best.dist == dmin, "%s [%s] m=%d n=%lld: best dist %d, smallest %d",
                              what, kern, m, (long long)n, best.dist, dmin);
                        CHECK(best.start >= 0 && best.start <= best.end && best.end <= n, "%s: best bounds", what);
                        nr = ref_find(p, m, t, n, d, dmin, nocase, start, all);
                        CHECK(nr > 0, "%s: reference has no best", what);
                        if(nr > 0){
                                int same_best;
                                r1[0] = all[0];
                                same_best = best.start == r1[0].start && best.end == r1[0].end &&
                                        best.dist == r1[0].dist;
                                CHECK(same_best, "%s [%s] m=%d n=%lld: best [%lld,%lld) %d, reference [%lld,%lld) %d",
                                      what, kern, m, (long long)n, (long long)best.start, (long long)best.end,
                                      best.dist, (long long)r1[0].start, (long long)r1[0].end, r1[0].dist);
                                if(!same_best){
                                        /* the reference's distance is exact; say whether best's is */
                                        CHECK(ref_ed(p, m, t + best.start, best.end - best.start, nocase) == best.dist,
                                              "%s [%s]: best [%lld, %lld) is not at distance %d", what, kern,
                                              (long long)best.start, (long long)best.end, best.dist);
                                }
                        }
                        free(all);
                }

                /* the distance of the whole text */
                {
                        int dist = -1;
                        CHECK(tld_bpm_distance(b, (const char *)t, n, &dist) == OK && dist == whole,
                              "%s [%s] m=%d n=%lld: distance %d, reference %d", what, kern, m, (long long)n, dist,
                              whole);
                }

                /* find, for each k */
                for(int ik = 0; ik < nk; ik++){
                        int k = ks[ik];
                        tld_bpm_hit *hits = NULL;
                        int64_t nh = -1;
                        const int64_t nr = n_ref[ik];
                        const tld_bpm_hit *r = ref[ik];
                        int same = 1;
                        if(tld_bpm_find(b, (const char *)t, n, k, &hits, &nh) != OK){
                                CHECK(0, "%s: find failed (m=%d k=%d)", what, m, k);
                                continue;
                        }
                        if(nh != nr) same = 0;
                        for(int64_t i = 0; same && i < nh; i++){
                                if(hits[i].start != r[i].start || hits[i].end != r[i].end || hits[i].dist != r[i].dist){
                                        same = 0;
                                }
                        }
                        CHECK(same, "%s [%s] m=%d n=%lld k=%d: %lld matches, reference %lld", what, kern, m,
                              (long long)n, k, (long long)nh, (long long)nr);
                        /* when the matches equal the reference their distances are exact (the
                           reference computes them); recompute them only when they differ, to
                           report which one is wrong */
                        for(int64_t i = 0; i < nh; i++){
                                const int kk = k > m ? m : k;
                                CHECK(hits[i].dist <= kk, "%s: match over k", what);
                                CHECK(i == 0 || hits[i].start >= hits[i - 1].end, "%s: matches overlap", what);
                                if(!same){
                                        CHECK(ref_ed(p, m, t + hits[i].start, hits[i].end - hits[i].start, nocase) ==
                                              hits[i].dist, "%s [%s]: match [%lld, %lld) is not at distance %d", what,
                                              kern, (long long)hits[i].start, (long long)hits[i].end, hits[i].dist);
                                }
                        }
                        MFREE(hits);
                }
                tld_bpm_free(b);
        }
        for(int ik = 0; ik < nk; ik++){
                free(ref[ik]);
        }
        free(start);
        free(d);
}

/* random bytes from an alphabet of size a (256: every byte value; 52: both cases) */
static void fill(uint8_t *x, int64_t n, int a)
{
        static const char *dna = "ACGT";
        for(int64_t i = 0; i < n; i++){
                switch(a){
                case 2: x[i] = (uint8_t)("ab"[rnd_int(2)]); break;
                case 4: x[i] = (uint8_t)dna[rnd_int(4)]; break;
                case 20: x[i] = (uint8_t)('a' + rnd_int(20)); break;
                case 52: x[i] = (uint8_t)(rnd_int(2) ? 'a' + rnd_int(4) : 'A' + rnd_int(4)); break;
                default: x[i] = (uint8_t)rnd_int(256); break;
                }
        }
}

/* copy p into t with at most e random edits (at e of its positions, chosen with
 * repeats); returns the bytes written */
static int plant(uint8_t *t, int64_t room, const uint8_t *p, int m, int e, int a)
{
        uint8_t *edit = calloc((size_t)m, 1);
        int w = 0;
        for(int i = 0; i < e; i++) edit[rnd_int(m)] = 1;
        for(int i = 0; i < m && w < room; i++){
                if(edit[i]){
                        int kind = rnd_int(3);
                        uint8_t c;
                        fill(&c, 1, a);
                        if(kind == 0){ t[w++] = c; }                    /* substitution */
                        else if(kind == 1){ /* deletion */ }
                        else { t[w++] = c; if(w < room) t[w++] = p[i]; }        /* insertion */
                }else{
                        t[w++] = p[i];
                }
        }
        free(edit);
        return w;
}

/* Matches that end on, just before and just after the boundaries of the 1024-position
 * chunks the kernels hand out, in texts of six chunks where most chunks hold no match
 * (so they are passed over). Copies are exact or edited; every kernel is checked against
 * the reference. */
static void chunk_edges(void)
{
        static const int ms[] = { 5, 20, 64, 65, 130, 400 };
        static const int deltas[] = { -2, -1, 0, 1, 2 };
        const int64_t n = 6 * 1024 + 37;
        uint8_t *t = malloc((size_t)n);
        uint8_t *p = malloc(400);
        for(int im = 0; im < (int)(sizeof ms / sizeof ms[0]); im++){
                const int m = ms[im];
                for(int id = 0; id < 5; id++){
                        for(int edited = 0; edited < 2; edited++){
                                int ks[3] = { 0, 1, 2 + m / 20 };
                                fill(p, m, 20);
                                fill(t, n, 20);
                                for(int j = 1; j <= 5; j++){
                                        const int64_t e = 1024 * (int64_t)j + deltas[(id + j) % 5];
                                        if(edited && j % 2){
                                                plant(t + e - m, n - (e - m), p, m, 1 + m / 40, 20);
                                        }else{
                                                memcpy(t + e - m, p, (size_t)m);        /* ends exactly at e */
                                        }
                                }
                                check_case(p, m, t, n, ks, 3, 0, "chunk edges");
                        }
                }
        }
        free(p);
        free(t);
}

static void random_cases(void)
{
        /* pattern lengths: each side of the 64-byte word boundaries up to 4 words, the
           5 -> 6 word step (where NEON's wavefront starts), and one long pattern of 16
           words; the patterns over 4 words run with 2 of the 4 alphabets, so the test
           stays a correctness check (also under the sanitizers), not a benchmark */
        static const int ms[] = { 1, 2, 3, 5, 8, 13, 31, 32, 33, 63, 64, 65, 100, 127, 128, 129,
                                  191, 192, 193, 255, 256, 257, 320, 321, 1000 };
        static const int alpha[] = { 2, 4, 20, 256 };
        const int nm = (int)(sizeof ms / sizeof ms[0]);
        uint8_t *p = malloc(1000);
        uint8_t *t = malloc(4000);
        char what[64];
        for(int im = 0; im < nm; im++){
                const int m = ms[im];
                const int64_t ns[] = { 0, 1, 2, 3, 7, m > 1 ? m - 1 : 1, m, m + 1, 2 * (int64_t)m + 5, 600 };
                for(int ia = m > 257 ? 2 : 0; ia < 4; ia++){
                        const int a = alpha[ia];
                        fill(p, m, a);
                        for(int in = 0; in < (int)(sizeof ns / sizeof ns[0]); in++){
                                const int64_t n = ns[in];
                                int ks[7];
                                int nk = 0;
                                fill(t, n, a);
                                /* plant one or two copies with a few edits, so long patterns match */
                                if(n >= m && m > 4){
                                        int64_t at = rnd_int((int)(n - m + 1));
                                        plant(t + at, n - at, p, m, rnd_int(m / 8 + 2), a);
                                        if(n >= 2 * (int64_t)m && rnd_int(2)){
                                                at = rnd_int((int)(n - m + 1));
                                                plant(t + at, n - at, p, m, rnd_int(m / 6 + 2), a);
                                        }
                                }
                                ks[nk++] = 0;
                                ks[nk++] = 1;
                                ks[nk++] = 2;
                                ks[nk++] = m / 10;
                                if((int64_t)m * n <= 40000){    /* many matches: keep the reference quick */
                                        ks[nk++] = m / 3;
                                        ks[nk++] = m;
                                        ks[nk++] = m + 5;
                                }
                                snprintf(what, sizeof what, "random a=%d", a);
                                check_case(p, m, t, n, ks, nk, 0, what);
                        }
                }
        }
        /* NOCASE: letters of both cases */
        for(int im = 0; im < nm; im += 2){
                const int m = ms[im];
                const int64_t n = 3 * (int64_t)m + 10;
                int ks[3] = { 0, 1, m / 5 };
                fill(p, m, 52);
                fill(t, n, 52);
                if(m > 4){
                        int64_t at = rnd_int((int)(n - m + 1));
                        int w = plant(t + at, n - at, p, m, rnd_int(m / 8 + 1), 52);
                        for(int i = 0; i < w; i++){                     /* flip the case of the copy */
                                uint8_t c = t[at + i];
                                if(c >= 'a' && c <= 'z') t[at + i] = (uint8_t)(c - 'a' + 'A');
                                else if(c >= 'A' && c <= 'Z') t[at + i] = (uint8_t)(c - 'A' + 'a');
                        }
                }
                check_case(p, m, t, n, ks, 3, TLD_BPM_NOCASE, "nocase");
                check_case(p, m, t, n, ks, 3, 0, "case");
        }
        free(p);
        free(t);
}

/* ------------------------------------------------------------------------ */
/* hand-worked cases                                                        */
/* ------------------------------------------------------------------------ */

static int find_str(const char *pat, const char *text, int k, int flags, tld_bpm_hit **hits, int64_t *n)
{
        tld_bpm *b = NULL;
        int r;
        if(tld_bpm_alloc(&b, pat, (int)strlen(pat), flags) != OK) return FAIL;
        r = tld_bpm_find(b, text, (int64_t)strlen(text), k, hits, n);
        tld_bpm_free(b);
        return r;
}

static int hit_is(const tld_bpm_hit *h, int64_t s, int64_t e, int d)
{
        return h->start == s && h->end == e && h->dist == d;
}

static void hand_cases(void)
{
        tld_bpm *b = NULL;
        tld_bpm_hit *h = NULL;
        tld_bpm_hit best;
        int64_t n = 0;

        /* the manual's examples */
        CHECK(find_str("color", "colour, color or colr", 1, 0, &h, &n) == OK && n == 3, "colour: %lld", (long long)n);
        if(n == 3){
                CHECK(hit_is(&h[0], 0, 6, 1), "colour [%lld, %lld) %d",
                      (long long)h[0].start, (long long)h[0].end, h[0].dist);
                CHECK(hit_is(&h[1], 8, 13, 0), "color");
                CHECK(hit_is(&h[2], 17, 21, 1), "colr");
        }
        MFREE(h);
        if(tld_bpm_alloc(&b, "tld_jsn_get_str", 15, 0) == OK){
                const char text[] = "a call to tld_json_get_string";
                CHECK(tld_bpm_best(b, text, (int64_t)(sizeof text - 1), &best) == OK && hit_is(&best, 10, 26, 1),
                      "best tld_json_get_str: [%lld, %lld) %d", (long long)best.start, (long long)best.end, best.dist);
        }
        tld_bpm_free(b);
        b = NULL;

        /* every exact occurrence, back to back and apart */
        CHECK(find_str("abc", "abcabcxxabc", 0, 0, &h, &n) == OK && n == 3, "abc x3: %lld", (long long)n);
        if(n == 3) CHECK(hit_is(&h[0], 0, 3, 0) && hit_is(&h[1], 3, 6, 0) && hit_is(&h[2], 8, 11, 0), "abc positions");
        MFREE(h);
        /* overlapping exact occurrences: the non-overlapping ones from the left */
        CHECK(find_str("aa", "aaaaa", 0, 0, &h, &n) == OK && n == 2, "aa in aaaaa: %lld", (long long)n);
        if(n == 2) CHECK(hit_is(&h[0], 0, 2, 0) && hit_is(&h[1], 2, 4, 0), "aa positions");
        MFREE(h);
        /* one insertion: one match covering it, not three */
        CHECK(find_str("abc", "abXc", 1, 0, &h, &n) == OK && n == 1, "abXc: %lld", (long long)n);
        if(n == 1) CHECK(hit_is(&h[0], 0, 4, 1), "abXc [%lld, %lld) %d", (long long)h[0].start, (long long)h[0].end, h[0].dist);
        MFREE(h);
        /* a substitution is preferred to a deletion at the start (the longer stretch) */
        CHECK(find_str("abcd", "xxbcd", 1, 0, &h, &n) == OK && n == 1 && hit_is(&h[0], 1, 5, 1), "xbcd");
        MFREE(h);
        /* nothing within k */
        CHECK(find_str("hello", "abcdefgh", 1, 0, &h, &n) == OK && n == 0 && h == NULL, "no match");
        MFREE(h);
        /* pattern longer than the text */
        if(tld_bpm_alloc(&b, "abcdef", 6, 0) == OK){
                CHECK(tld_bpm_best(b, "abc", 3, &best) == OK && hit_is(&best, 0, 3, 3), "long pattern best");
                CHECK(tld_bpm_best(b, "", 0, &best) == OK && hit_is(&best, 0, 0, 6), "empty text best");
                CHECK(tld_bpm_find(b, "", 0, 6, &h, &n) == OK && n == 0 && h == NULL, "empty text find");
                MFREE(h);
                /* k above the pattern length is the pattern length */
                CHECK(tld_bpm_find(b, "zzzz", 4, 1000, &h, &n) == OK && n > 0, "k clamp");
                for(int64_t i = 0; i < n; i++) CHECK(h[i].dist <= 6, "k clamp dist");
                MFREE(h);
        }
        tld_bpm_free(b);
        b = NULL;
        /* case */
        CHECK(find_str("CoLoR", "the COLOR red", 0, TLD_BPM_NOCASE, &h, &n) == OK && n == 1 && hit_is(&h[0], 4, 9, 0), "nocase");
        MFREE(h);
        CHECK(find_str("CoLoR", "the COLOR red", 1, 0, &h, &n) == OK && n == 0, "case matters without NOCASE");
        MFREE(h);
        CHECK(find_str("a-1", "A-1 a_1", 0, TLD_BPM_NOCASE, &h, &n) == OK && n == 1 && hit_is(&h[0], 0, 3, 0), "nocase non-letters");
        MFREE(h);
        /* bytes 0 and 255 */
        {
                const char pat[] = { 'x', 0, (char)255, 'y' };
                const char text[] = { 'a', 'x', 0, (char)255, 'y', 'b', 'x', 0, 0, 'y' };
                if(tld_bpm_alloc(&b, pat, 4, 0) == OK){
                        CHECK(tld_bpm_find(b, text, 10, 1, &h, &n) == OK && n == 2, "bytes: %lld", (long long)n);
                        if(n == 2) CHECK(hit_is(&h[0], 1, 5, 0) && hit_is(&h[1], 6, 10, 1), "bytes positions");
                        MFREE(h);
                }
                tld_bpm_free(b);
                b = NULL;
        }
        /* tld_bpm_distance */
        {
                static const struct { const char *a, *b; int flags, d; } dc[] = {
                        { "kitten", "sitting", 0, 3 }, { "sitting", "kitten", 0, 3 }, { "abc", "abc", 0, 0 },
                        { "abc", "", 0, 3 }, { "a", "bbbb", 0, 4 }, { "flaw", "lawn", 0, 2 },
                        { "ABC", "abc", 0, 3 }, { "ABC", "abc", TLD_BPM_NOCASE, 0 }, { "abc", "xabcx", 0, 2 },
                };
                for(int i = 0; i < (int)(sizeof dc / sizeof dc[0]); i++){
                        int dist = -1;
                        if(tld_bpm_alloc(&b, dc[i].a, (int)strlen(dc[i].a), dc[i].flags) == OK){
                                CHECK(tld_bpm_distance(b, dc[i].b, (int64_t)strlen(dc[i].b), &dist) == OK && dist == dc[i].d,
                                      "distance(%s, %s) = %d, want %d", dc[i].a, dc[i].b, dist, dc[i].d);
                        }
                        tld_bpm_free(b);
                        b = NULL;
                }
        }
        /* a 1-byte pattern */
        CHECK(find_str("q", "aqbqq", 0, 0, &h, &n) == OK && n == 3, "1-byte: %lld", (long long)n);
        MFREE(h);
        /* the best match far into a long text (the second pass starts late) */
        {
                const int m = 150;
                const int64_t len = 20000;
                uint8_t *p = malloc((size_t)m), *t = malloc((size_t)len);
                fill(p, m, 4);
                fill(t, len, 4);
                plant(t + 19000, len - 19000, p, m, 3, 4);
                for(int plain = 0; plain < 2; plain++){
                        if(tld_bpm_alloc(&b, (const char *)p, m, plain ? TLD_BPM_PLAIN : 0) == OK){
                                CHECK(tld_bpm_best(b, (const char *)t, len, &best) == OK && best.start >= 18990 &&
                                      best.end <= 19000 + m + 10 && best.dist <= 3, "late best [%lld, %lld) %d",
                                      (long long)best.start, (long long)best.end, best.dist);
                                CHECK(ref_ed(p, m, t + best.start, best.end - best.start, 0) == best.dist, "late best dist");
                        }
                        tld_bpm_free(b);
                        b = NULL;
                }
                free(p);
                free(t);
        }
}

static void error_cases(void)
{
        tld_bpm *b = NULL;
        tld_bpm_hit *h = NULL;
        tld_bpm_hit one;
        tld_bpm_hit dummy;
        int64_t n = 0;
        CHECK(tld_bpm_alloc(&b, "x", 0, 0) == FAIL && b == NULL, "len 0");
        CHECK(tld_bpm_alloc(&b, "x", -1, 0) == FAIL && b == NULL, "len -1");
        CHECK(tld_bpm_alloc(&b, "x", 1, 8) == FAIL && b == NULL, "unknown flag");
        if(tld_bpm_alloc(&b, "abc", 3, 0) != OK){
                CHECK(0, "alloc");
                return;
        }
        CHECK(tld_bpm_find(b, "abc", 3, -1, &h, &n) == FAIL, "k < 0");
        CHECK(tld_bpm_find(b, "abc", -1, 0, &h, &n) == FAIL, "len < 0");
        CHECK(tld_bpm_best(b, "abc", -1, &one) == FAIL, "best len < 0");
        {
                int dist = -1;
                CHECK(tld_bpm_distance(b, "abc", -1, &dist) == FAIL, "distance len < 0");
                CHECK(tld_bpm_distance(b, "abc", (int64_t)INT32_MAX, &dist) == FAIL, "distance len too large");
        }
        h = &dummy;
        CHECK(tld_bpm_find(b, "abc", 3, 0, &h, &n) == FAIL, "*hits not NULL");
        h = NULL;
        tld_bpm_free(b);
        tld_bpm_free(NULL);
}

/* ------------------------------------------------------------------------ */
/* a long text                                                              */
/* ------------------------------------------------------------------------ */

static void long_text(void)
{
        const int64_t len = 1 << 20;    /* 16 of the 64 KB stretches of the four-at-once scan */
        const int ms[2] = { 40, 600 };
        const int ks[2] = { 2, 20 };
        uint8_t *t = malloc((size_t)len);
        for(int c = 0; c < 2; c++){
                const int m = ms[c], k = ks[c];
                const int n_plant = 50;
                uint8_t *p = malloc((size_t)m);
                int64_t at[50];
                tld_bpm_hit *hits[2] = { NULL, NULL };
                int64_t nh[2] = { 0, 0 };
                const char *kern[2] = { "", "" };
                fill(t, len, 4);
                fill(p, m, 4);
                for(int i = 0; i < n_plant; i++){
                        at[i] = (len / n_plant) * i + rnd_int((int)(len / n_plant - 2 * m));
                        plant(t + at[i], len - at[i], p, m, k / 2, 4);
                }
                for(int plain = 0; plain < 2; plain++){
                        tld_bpm *b = NULL;
                        if(tld_bpm_alloc(&b, (const char *)p, m, plain ? TLD_BPM_PLAIN : 0) != OK){
                                CHECK(0, "long alloc");
                                continue;
                        }
                        kern[plain] = tld_bpm_kernel(b);
                        CHECK(tld_bpm_find(b, (const char *)t, len, k, &hits[plain], &nh[plain]) == OK, "long find");
                        tld_bpm_free(b);
                }
                CHECK(nh[0] == nh[1], "long m=%d: %s %lld matches, %s %lld", m, kern[0], (long long)nh[0],
                      kern[1], (long long)nh[1]);
                for(int64_t i = 0; i < nh[0] && i < nh[1]; i++){
                        CHECK(hits[0][i].start == hits[1][i].start && hits[0][i].end == hits[1][i].end &&
                              hits[0][i].dist == hits[1][i].dist, "long m=%d: match %lld differs", m, (long long)i);
                }
                /* every planted copy lies under a match */
                for(int i = 0; i < n_plant; i++){
                        int found = 0;
                        for(int64_t j = 0; j < nh[0] && !found; j++){
                                if(hits[0][j].start < at[i] + m && hits[0][j].end > at[i]) found = 1;
                        }
                        CHECK(found, "long m=%d: copy %d at %lld not found", m, i, (long long)at[i]);
                }
                LOG_MSG("1 MB, m = %d, k = %d: %lld matches; %s and %s agree", m, k, (long long)nh[0], kern[0],
                        kern[1]);
                MFREE(hits[0]);
                MFREE(hits[1]);
                free(p);
        }
        free(t);
}

static int run_args(int argc, char *argv[])
{
        tld_bpm *b = NULL;
        tld_bpm_hit *hits = NULL;
        tld_bpm_hit best;
        int64_t n = 0;
        int k = argc > 3 ? atoi(argv[3]) : 1;
        RUN(tld_bpm_alloc(&b, argv[1], (int)strlen(argv[1]), 0));
        RUN(tld_bpm_best(b, argv[2], (int64_t)strlen(argv[2]), &best));
        RUN(tld_bpm_find(b, argv[2], (int64_t)strlen(argv[2]), k, &hits, &n));
        printf("kernel %s\nbest   [%lld, %lld) distance %d: %.*s\n", tld_bpm_kernel(b), (long long)best.start,
               (long long)best.end, best.dist, (int)(best.end - best.start), argv[2] + best.start);
        for(int64_t i = 0; i < n; i++){
                printf("match  [%lld, %lld) distance %d: %.*s\n", (long long)hits[i].start, (long long)hits[i].end,
                       hits[i].dist, (int)(hits[i].end - hits[i].start), argv[2] + hits[i].start);
        }
        MFREE(hits);
        tld_bpm_free(b);
        return OK;
ERROR:
        MFREE(hits);
        tld_bpm_free(b);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[PATTERN TEXT [K]]",
                  "With no arguments checks tld_bpm against a dynamic-programming reference.\n"
                  "With PATTERN TEXT prints the kernel, the best match and the matches with at most K\n"
                  "edits (default 1).");
        if(argc > 2){
                return run_args(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        SECTION_MSG("hand-worked cases");
        hand_cases();
        SECTION_MSG("argument errors");
        error_cases();
        SECTION_MSG("random cases against the reference");
        random_cases();
        SECTION_MSG("matches at the chunk boundaries");
        chunk_edges();
        SECTION_MSG("a long text");
        long_text();
        if(n_fail){
                fprintf(stderr, "unit-bpm: %d of %d checks failed\n", n_fail, n_check);
                return EXIT_FAILURE;
        }
        LOG_MSG("%d checks passed", n_check);
        return EXIT_SUCCESS;
}

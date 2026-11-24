#include "tld-bpm.h"

#include "../alloc/tld-alloc.h"
#include <stdint.h>
#include <string.h>

/* Myers' bit-parallel edit distance (Myers 1999, J. ACM 46:395), in the block form of
 * Hyyrö (2003) for patterns longer than one 64-bit word.
 *
 * The dynamic-programming matrix has a row per pattern byte and a column per text byte;
 * D[j][i] is the distance between the first j pattern bytes and the best stretch of
 * text ending at text byte i. Each column is kept as its vertical differences, one bit
 * per row in Pv (+1) and Mv (-1); a step over one text byte updates a word of 64 rows in
 * a few operations. Row 0 is 0 in every column for a search (a match may start anywhere)
 * and i for an anchored scan (it starts at the first text byte); column 0 is j. The
 * distance of the whole pattern, row m, is followed in d and handed out for every text
 * position: d_e is the smallest distance of the pattern to a stretch of text ending just
 * before e.
 *
 * Kernels (all give the same d for every position):
 *   word   one word, m <= 64
 *   block  words one after another for each text byte, the carry passed down
 *   avx2 / neon   the blocks as a wavefront: lane b of a 4-word vector holds block b and
 *          works on text byte s - b at step s, so the carry from block b - 1 is the
 *          previous step's output, shifted one lane. Rows past m (the padding of the last
 *          word) match nothing; they cannot change rows <= m, which only feed on rows
 *          above them.
 *
 * Bytes are mapped to classes (the distinct bytes of the pattern, or of its letters
 * folded with TLD_BPM_NOCASE; class 0 is every other byte), so a pattern stores one
 * bit row per class rather than per byte value.
 *
 * Where matches start: the best stretch ending at e with distance d starts at most
 * m + d bytes before e. The reversed pattern is scanned anchored over the reversed text
 * before e; after j bytes its row m is the distance of text[e - j, e) to the pattern, and
 * the largest j with distance d is the start. */

enum { TLD__BPM_WORD, TLD__BPM_BLOCK, TLD__BPM_AVX2, TLD__BPM_NEON };

#if (defined(__x86_64__) || defined(__amd64__)) && (defined(__GNUC__) || defined(__clang__))
#define TLD__BPM_HAVE_AVX2 1
#include <immintrin.h>
#define TLD__BPM_TARGET __attribute__((target("avx2")))
#elif defined(__aarch64__) && defined(__ARM_NEON)
#define TLD__BPM_HAVE_NEON 1
#include <arm_neon.h>
#define TLD__BPM_TARGET
#endif

/* Fewest words for the wavefront kernels; below this the block kernel is faster. */
#ifndef TLD__BPM_AVX2_MIN             /* tests set these lower to run the wavefront on short patterns */
#define TLD__BPM_AVX2_MIN 2
#endif
#ifndef TLD__BPM_NEON_MIN
#define TLD__BPM_NEON_MIN 6
#endif

#define TLD__BPM_CHUNK 1024     /* distances handed to a sink at a time */

/* Patterns of at most 64 bytes in long texts: four stretches of text are scanned at once
 * (they are independent, so the CPU overlaps them), each stretch of QMIN to QMAX end
 * positions. Tests set these small to put many stretch boundaries into short texts. */
#ifndef TLD__BPM_IL_QMAX
#define TLD__BPM_IL_QMAX 65536
#endif
#ifndef TLD__BPM_IL_QMIN
#define TLD__BPM_IL_QMIN 1024
#endif
/* tld_bpm_find with patterns over 64 bytes uses the cut-off (the band kernel) when
 * (k + 1) * RATIO <= m: measured on random DNA, the cut-off is faster up to about k = m/8
 * for 128-byte patterns and k = m/5 for longer ones. Both give the same matches. Tests
 * set 0 (always) or a large number (never). */
#ifndef TLD__BPM_BAND_RATIO
#define TLD__BPM_BAND_RATIO 8
#endif

struct tld_bpm {
        uint64_t *peq;          /* [n_class][nwp]: bit j of word w: pattern byte 64w + j is in the class */
        uint64_t *rpeq;         /* the same for the reversed pattern */
        int m;                  /* pattern length */
        int nw;                 /* words: (m + 63) / 64 */
        int nwp;                /* nw rounded up to a multiple of 4; the padding words are 0 */
        int n_class;
        int kernel;
        uint16_t map[256];      /* byte -> class */
};

/* A sink takes the distances d[0 .. cnt) for the end positions e0 .. e0 + cnt - 1, and
 * dmin, the smallest of them (so a sink can pass over a chunk without reading it), and
 * returns 0 to go on, 1 to stop the scan, -1 on failure. */
typedef int (*tld__bpm_sink)(void *ctx, const int32_t *d, int64_t e0, int cnt, int32_t dmin);

static int tld__bpm_scan(const tld_bpm *b, int rev, const uint8_t *t, int64_t n, int anchored,
                         int64_t e_base, uint64_t *ws, tld__bpm_sink sink, void *ctx);

int tld_bpm_alloc(tld_bpm **bpm, const char *pattern, int len, int flags)
{
        tld_bpm *b = NULL;
        const uint8_t *p = (const uint8_t *)pattern;
        size_t words = 0;
        ASSERT(bpm != NULL, "No output pointer");
        ASSERT(pattern != NULL, "No pattern");
        ASSERT(len > 0, "Pattern length must be positive (got %d)", len);
        ASSERT((flags & ~(TLD_BPM_NOCASE | TLD_BPM_PLAIN)) == 0, "Unknown flags 0x%x", (unsigned)flags);
        MMALLOC(b, sizeof(tld_bpm));
        b->m = len;
        b->nw = (int)(((int64_t)len + 63) / 64);
        b->nwp = (b->nw + 3) & ~3;
        b->n_class = 1;
        for(int i = 0; i < len; i++){
                int c = p[i];
                if(b->map[c] != 0){
                        continue;
                }
                b->map[c] = (uint16_t)b->n_class;
                if(flags & TLD_BPM_NOCASE){
                        if(c >= 'a' && c <= 'z'){
                                b->map[c - 'a' + 'A'] = (uint16_t)b->n_class;
                        }else if(c >= 'A' && c <= 'Z'){
                                b->map[c - 'A' + 'a'] = (uint16_t)b->n_class;
                        }
                }
                b->n_class++;
        }
        words = (size_t)b->n_class * (size_t)b->nwp;
        MMALLOC(b->peq, words * sizeof(uint64_t));
        MMALLOC(b->rpeq, words * sizeof(uint64_t));
        for(int i = 0; i < len; i++){
                const size_t row = (size_t)b->map[p[i]] * (size_t)b->nwp;
                const int ri = len - 1 - i;
                b->peq[row + (size_t)(i / 64)] |= (uint64_t)1 << (i % 64);
                b->rpeq[row + (size_t)(ri / 64)] |= (uint64_t)1 << (ri % 64);
        }
        b->kernel = b->nw == 1 ? TLD__BPM_WORD : TLD__BPM_BLOCK;
        if(!(flags & TLD_BPM_PLAIN)){
#if defined(TLD__BPM_HAVE_AVX2)
                if(b->nw >= TLD__BPM_AVX2_MIN && __builtin_cpu_supports("avx2")){
                        b->kernel = TLD__BPM_AVX2;
                }
#elif defined(TLD__BPM_HAVE_NEON)
                if(b->nw >= TLD__BPM_NEON_MIN){
                        b->kernel = TLD__BPM_NEON;
                }
#endif
        }
        *bpm = b;
        return OK;
ERROR:
        tld_bpm_free(b);
        return FAIL;
}

void tld_bpm_free(tld_bpm *b)
{
        if(b){
                MFREE(b->peq);
                MFREE(b->rpeq);
                MFREE(b);
        }
}

const char *tld_bpm_kernel(const tld_bpm *b)
{
        ASSERT(b != NULL, "No pattern");
        switch(b->kernel){
        case TLD__BPM_WORD: return "word";
        case TLD__BPM_BLOCK: return "block";
        case TLD__BPM_AVX2: return "avx2";
        default: return "neon";
        }
ERROR:
        return NULL;
}

/* ------------------------------------------------------------------------ */
/* kernels                                                                  */
/* ------------------------------------------------------------------------ */

/* m <= 64: one word. */
static int tld__bpm_scan_word(const tld_bpm *b, const uint64_t *peq, const uint8_t *t, int64_t n,
                              int anchored, int64_t e_base, tld__bpm_sink sink, void *ctx)
{
        int32_t buf[TLD__BPM_CHUNK];
        const uint16_t *map = b->map;
        const size_t nwp = (size_t)b->nwp;
        const uint64_t hin = anchored ? 1 : 0;
        const int r = b->m - 1;
        uint64_t Pv = ~(uint64_t)0;
        uint64_t Mv = 0;
        int32_t d = b->m;
        int64_t e0 = e_base + 1;
        int cnt = 0;
        int32_t cmin = INT32_MAX;
        int st;
        for(int64_t i = 0; i < n; i++){
                const uint64_t Eq = peq[(size_t)map[t[i]] * nwp];
                const uint64_t Xv = Eq | Mv;
                const uint64_t Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
                uint64_t Ph = Mv | ~(Xh | Pv);
                uint64_t Mh = Pv & Xh;
                d += (int32_t)((Ph >> r) & 1) - (int32_t)((Mh >> r) & 1);
                Ph = (Ph << 1) | hin;
                Mh = Mh << 1;
                Pv = Mh | ~(Xv | Ph);
                Mv = Ph & Xv;
                buf[cnt++] = d;
                cmin = d < cmin ? d : cmin;
                if(cnt == TLD__BPM_CHUNK){
                        if((st = sink(ctx, buf, e0, cnt, cmin)) != 0){
                                return st;
                        }
                        e0 += cnt;
                        cnt = 0;
                        cmin = INT32_MAX;
                }
        }
        return cnt ? sink(ctx, buf, e0, cnt, cmin) : 0;
}

/* m > 64: the words of each column one after another. ws holds 2 * nw words. */
static int tld__bpm_scan_block(const tld_bpm *b, const uint64_t *peq, const uint8_t *t, int64_t n,
                               int anchored, int64_t e_base, uint64_t *ws, tld__bpm_sink sink, void *ctx)
{
        int32_t buf[TLD__BPM_CHUNK];
        const uint16_t *map = b->map;
        const size_t nwp = (size_t)b->nwp;
        const int last = b->nw - 1;
        const int r = (b->m - 1) % 64;
        const uint64_t hin = anchored ? 1 : 0;
        uint64_t *P = ws;
        uint64_t *M = ws + b->nw;
        int32_t d = b->m;
        int64_t e0 = e_base + 1;
        int cnt = 0;
        int32_t cmin = INT32_MAX;
        int st;
        for(int w = 0; w < b->nw; w++){
                P[w] = ~(uint64_t)0;
                M[w] = 0;
        }
        for(int64_t i = 0; i < n; i++){
                const uint64_t *row = peq + (size_t)map[t[i]] * nwp;
                uint64_t hp = hin;
                uint64_t hn = 0;
                for(int w = 0; w < last; w++){
                        const uint64_t Pv = P[w];
                        const uint64_t Mv = M[w];
                        uint64_t Eq = row[w];
                        const uint64_t Xv = Eq | Mv;
                        Eq |= hn;
                        const uint64_t Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
                        uint64_t Ph = Mv | ~(Xh | Pv);
                        uint64_t Mh = Pv & Xh;
                        const uint64_t op = Ph >> 63;
                        const uint64_t on = Mh >> 63;
                        Ph = (Ph << 1) | hp;
                        Mh = (Mh << 1) | hn;
                        P[w] = Mh | ~(Xv | Ph);
                        M[w] = Ph & Xv;
                        hp = op;
                        hn = on;
                }
                {
                        const uint64_t Pv = P[last];
                        const uint64_t Mv = M[last];
                        uint64_t Eq = row[last];
                        const uint64_t Xv = Eq | Mv;
                        Eq |= hn;
                        const uint64_t Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
                        uint64_t Ph = Mv | ~(Xh | Pv);
                        uint64_t Mh = Pv & Xh;
                        d += (int32_t)((Ph >> r) & 1) - (int32_t)((Mh >> r) & 1);
                        Ph = (Ph << 1) | hp;
                        Mh = (Mh << 1) | hn;
                        P[last] = Mh | ~(Xv | Ph);
                        M[last] = Ph & Xv;
                }
                buf[cnt++] = d;
                cmin = d < cmin ? d : cmin;
                if(cnt == TLD__BPM_CHUNK){
                        if((st = sink(ctx, buf, e0, cnt, cmin)) != 0){
                                return st;
                        }
                        e0 += cnt;
                        cnt = 0;
                        cmin = INT32_MAX;
                }
        }
        return cnt ? sink(ctx, buf, e0, cnt, cmin) : 0;
}

/* Four 64-bit lanes: one __m256i (AVX2) or two uint64x2_t (NEON). */
#if defined(TLD__BPM_HAVE_AVX2)
typedef __m256i tld__u4;
#define TLD__U4_FN static inline __attribute__((always_inline, target("avx2")))
TLD__U4_FN tld__u4 tld__u4_load(const uint64_t *p) { return _mm256_loadu_si256((const __m256i *)(const void *)p); }
TLD__U4_FN void tld__u4_store(uint64_t *p, tld__u4 a) { _mm256_storeu_si256((__m256i *)(void *)p, a); }
TLD__U4_FN tld__u4 tld__u4_set(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
        return _mm256_set_epi64x((long long)d, (long long)c, (long long)b, (long long)a);
}
TLD__U4_FN tld__u4 tld__u4_and(tld__u4 a, tld__u4 b) { return _mm256_and_si256(a, b); }
TLD__U4_FN tld__u4 tld__u4_or(tld__u4 a, tld__u4 b) { return _mm256_or_si256(a, b); }
TLD__U4_FN tld__u4 tld__u4_xor(tld__u4 a, tld__u4 b) { return _mm256_xor_si256(a, b); }
TLD__U4_FN tld__u4 tld__u4_add(tld__u4 a, tld__u4 b) { return _mm256_add_epi64(a, b); }
TLD__U4_FN tld__u4 tld__u4_not(tld__u4 a) { return _mm256_xor_si256(a, _mm256_set1_epi64x(-1)); }
TLD__U4_FN tld__u4 tld__u4_shl1(tld__u4 a) { return _mm256_slli_epi64(a, 1); }
TLD__U4_FN tld__u4 tld__u4_shr63(tld__u4 a) { return _mm256_srli_epi64(a, 63); }
/* [prev lane 3, cur lane 0, cur lane 1, cur lane 2] */
TLD__U4_FN tld__u4 tld__u4_shift_in(tld__u4 prev, tld__u4 cur)
{
        const tld__u4 c = _mm256_permute4x64_epi64(cur, _MM_SHUFFLE(2, 1, 0, 3));
        const tld__u4 p = _mm256_permute4x64_epi64(prev, _MM_SHUFFLE(2, 1, 0, 3));
        return _mm256_blend_epi32(c, p, 0x03);
}
/* lanes whose mask is all ones take a, the others b */
TLD__U4_FN tld__u4 tld__u4_sel(tld__u4 mask, tld__u4 a, tld__u4 b) { return _mm256_blendv_epi8(b, a, mask); }
#elif defined(TLD__BPM_HAVE_NEON)
typedef struct { uint64x2_t lo, hi; } tld__u4;
#define TLD__U4_FN static inline __attribute__((always_inline))
TLD__U4_FN tld__u4 tld__u4_load(const uint64_t *p) { tld__u4 r = { vld1q_u64(p), vld1q_u64(p + 2) }; return r; }
TLD__U4_FN void tld__u4_store(uint64_t *p, tld__u4 a) { vst1q_u64(p, a.lo); vst1q_u64(p + 2, a.hi); }
TLD__U4_FN tld__u4 tld__u4_set(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
        tld__u4 r = { vcombine_u64(vcreate_u64(a), vcreate_u64(b)), vcombine_u64(vcreate_u64(c), vcreate_u64(d)) };
        return r;
}
TLD__U4_FN tld__u4 tld__u4_and(tld__u4 a, tld__u4 b) { tld__u4 r = { vandq_u64(a.lo, b.lo), vandq_u64(a.hi, b.hi) }; return r; }
TLD__U4_FN tld__u4 tld__u4_or(tld__u4 a, tld__u4 b) { tld__u4 r = { vorrq_u64(a.lo, b.lo), vorrq_u64(a.hi, b.hi) }; return r; }
TLD__U4_FN tld__u4 tld__u4_xor(tld__u4 a, tld__u4 b) { tld__u4 r = { veorq_u64(a.lo, b.lo), veorq_u64(a.hi, b.hi) }; return r; }
TLD__U4_FN tld__u4 tld__u4_add(tld__u4 a, tld__u4 b) { tld__u4 r = { vaddq_u64(a.lo, b.lo), vaddq_u64(a.hi, b.hi) }; return r; }
TLD__U4_FN tld__u4 tld__u4_not(tld__u4 a)
{
        tld__u4 r = { vreinterpretq_u64_u32(vmvnq_u32(vreinterpretq_u32_u64(a.lo))),
                      vreinterpretq_u64_u32(vmvnq_u32(vreinterpretq_u32_u64(a.hi))) };
        return r;
}
TLD__U4_FN tld__u4 tld__u4_shl1(tld__u4 a) { tld__u4 r = { vshlq_n_u64(a.lo, 1), vshlq_n_u64(a.hi, 1) }; return r; }
TLD__U4_FN tld__u4 tld__u4_shr63(tld__u4 a) { tld__u4 r = { vshrq_n_u64(a.lo, 63), vshrq_n_u64(a.hi, 63) }; return r; }
TLD__U4_FN tld__u4 tld__u4_shift_in(tld__u4 prev, tld__u4 cur)
{
        tld__u4 r = { vextq_u64(prev.hi, cur.lo, 1), vextq_u64(cur.lo, cur.hi, 1) };
        return r;
}
TLD__U4_FN tld__u4 tld__u4_sel(tld__u4 mask, tld__u4 a, tld__u4 b)
{
        tld__u4 r = { vbslq_u64(mask.lo, a.lo, b.lo), vbslq_u64(mask.hi, a.hi, b.hi) };
        return r;
}
#endif

#if defined(TLD__BPM_HAVE_AVX2) || defined(TLD__BPM_HAVE_NEON)
/* The class of text byte i, or class 0 outside the text (lanes that have not started
 * or have finished; their results are never read). */
#define TLD__BPM_CLS(i) (((i) >= 0 && (i) < n) ? (size_t)map[t[(i)]] : 0)

/* nw >= 2: the wavefront. ws holds 4 * nwp words. */
static TLD__BPM_TARGET int tld__bpm_scan_wave(const tld_bpm *b, const uint64_t *peq, const uint8_t *t, int64_t n,
                                              int anchored, int64_t e_base, uint64_t *ws, tld__bpm_sink sink, void *ctx)
{
        int32_t buf[TLD__BPM_CHUNK];
        const uint16_t *map = b->map;
        const int nw = b->nw;
        const size_t nwp = (size_t)b->nwp;
        const int nv = b->nwp / 4;
        const int lv = (nw - 1) / 4;            /* vector and lane of the last word */
        const int ll = (nw - 1) % 4;
        const int r = (b->m - 1) % 64;
        const int64_t steps = n + nw - 1;
        const int64_t safe_from = 4 * (int64_t)nv - 1; /* from here every lane is inside the text ... */
        uint64_t *P = ws;
        uint64_t *M = ws + nwp;
        uint64_t *HP = ws + 2 * nwp;            /* each lane's carry out of the previous step */
        uint64_t *HN = ws + 3 * nwp;
        const tld__u4 zero = tld__u4_set(0, 0, 0, 0);
        const tld__u4 first_hp = tld__u4_set(0, 0, 0, anchored ? 1 : 0);
        uint64_t lane[4];
        int32_t d = b->m;
        int64_t e0 = e_base + 1;
        int cnt = 0;
        int32_t cmin = INT32_MAX;
        int st;
        for(size_t w = 0; w < nwp; w++){
                P[w] = ~(uint64_t)0;
                M[w] = 0;
                HP[w] = 0;
                HN[w] = 0;
        }
        for(int64_t s = 0; s < steps; s++){
                const int edge = s < safe_from || s >= n;  /* ... up to here */
                tld__u4 prevp = first_hp;
                tld__u4 prevn = zero;
                int64_t ph_r = 0;
                int64_t mh_r = 0;
                for(int v = 0; v < nv; v++){
                        const size_t b0 = 4 * (size_t)v;
                        const tld__u4 hpv = tld__u4_load(HP + b0);
                        const tld__u4 hnv = tld__u4_load(HN + b0);
                        const tld__u4 hp = tld__u4_shift_in(prevp, hpv);
                        const tld__u4 hn = tld__u4_shift_in(prevn, hnv);
                        const tld__u4 Pv = tld__u4_load(P + b0);
                        const tld__u4 Mv = tld__u4_load(M + b0);
                        const int64_t i0 = s - (int64_t)b0;
                        size_t c0, c1, c2, c3;
                        prevp = hpv;
                        prevn = hnv;
                        if(edge){
                                c0 = TLD__BPM_CLS(i0);
                                c1 = TLD__BPM_CLS(i0 - 1);
                                c2 = TLD__BPM_CLS(i0 - 2);
                                c3 = TLD__BPM_CLS(i0 - 3);
                        }else{
                                c0 = map[t[i0]];
                                c1 = map[t[i0 - 1]];
                                c2 = map[t[i0 - 2]];
                                c3 = map[t[i0 - 3]];
                        }
                        tld__u4 Eq = tld__u4_set(peq[c0 * nwp + b0], peq[c1 * nwp + b0 + 1],
                                                 peq[c2 * nwp + b0 + 2], peq[c3 * nwp + b0 + 3]);
                        const tld__u4 Xv = tld__u4_or(Eq, Mv);
                        Eq = tld__u4_or(Eq, hn);
                        const tld__u4 Xh = tld__u4_or(tld__u4_xor(tld__u4_add(tld__u4_and(Eq, Pv), Pv), Pv), Eq);
                        tld__u4 Ph = tld__u4_or(Mv, tld__u4_not(tld__u4_or(Xh, Pv)));
                        tld__u4 Mh = tld__u4_and(Pv, Xh);
                        if(v == lv){
                                tld__u4_store(lane, Ph);
                                ph_r = (int64_t)((lane[ll] >> r) & 1);
                                tld__u4_store(lane, Mh);
                                mh_r = (int64_t)((lane[ll] >> r) & 1);
                        }
                        const tld__u4 op = tld__u4_shr63(Ph);
                        const tld__u4 on = tld__u4_shr63(Mh);
                        Ph = tld__u4_or(tld__u4_shl1(Ph), hp);
                        Mh = tld__u4_or(tld__u4_shl1(Mh), hn);
                        tld__u4 nP = tld__u4_or(Mh, tld__u4_not(tld__u4_or(Xv, Ph)));
                        tld__u4 nM = tld__u4_and(Ph, Xv);
                        if(i0 < 3){             /* lanes that have not started keep their column */
                                const uint64_t on1 = ~(uint64_t)0;
                                const tld__u4 act = tld__u4_set(i0 >= 0 ? on1 : 0, i0 >= 1 ? on1 : 0,
                                                                i0 >= 2 ? on1 : 0, i0 >= 3 ? on1 : 0);
                                nP = tld__u4_sel(act, nP, Pv);
                                nM = tld__u4_sel(act, nM, Mv);
                        }
                        tld__u4_store(P + b0, nP);
                        tld__u4_store(M + b0, nM);
                        tld__u4_store(HP + b0, op);
                        tld__u4_store(HN + b0, on);
                }
                if(s >= nw - 1){                /* the last word is at text byte s - (nw - 1) */
                        d += (int32_t)(ph_r - mh_r);
                        buf[cnt++] = d;
                        cmin = d < cmin ? d : cmin;
                        if(cnt == TLD__BPM_CHUNK){
                                if((st = sink(ctx, buf, e0, cnt, cmin)) != 0){
                                        return st;
                                }
                                e0 += cnt;
                                cnt = 0;
                                cmin = INT32_MAX;
                        }
                }
        }
        return cnt ? sink(ctx, buf, e0, cnt, cmin) : 0;
}
#undef TLD__BPM_CLS
#endif

/* m > 64, for tld_bpm_find only: the block kernel with Ukkonen's cut-off (Ukkonen 1985;
 * in blocks as in Hyyro 2003 and edlib). Only distances <= k are needed exactly; for a
 * larger one it is enough to know that it is larger. Blocks 0 .. y are computed; every
 * cell of the blocks below y is > k. Such cells can only feed cells that are > k as well
 * (a cell is the smallest of its neighbours plus 0 or 1), so leaving them out changes no
 * cell <= k. A block is added when the bottom of block y was <= k in the last column
 * (the lowest row <= k moves down by at most one per column); its last column is taken
 * as one more than the row above it, row by row, values that are all > k like the true
 * ones, which again leaves every cell <= k exact. A block is dropped when its bottom row
 * is >= k + 64: no row of it can then be <= k. Row m is handed out exactly while the last
 * block is computed and it is <= k, and as k + 1 otherwise. ws holds 3 * nw words. */
static int tld__bpm_scan_band(const tld_bpm *b, const uint64_t *peq, const uint8_t *t, int64_t n,
                              int32_t k, int64_t e_base, uint64_t *ws, tld__bpm_sink sink, void *ctx)
{
        int32_t buf[TLD__BPM_CHUNK];
        const uint16_t *map = b->map;
        const size_t nwp = (size_t)b->nwp;
        const int last = b->nw - 1;
        const int r = (b->m - 1) % 64;
        const int32_t over = k + 1;
        uint64_t *P = ws;
        uint64_t *M = ws + b->nw;
        int64_t *S = (int64_t *)(void *)(ws + 2 * (size_t)b->nw);      /* each block's bottom row */
        int64_t dm = b->m;              /* row m, while the last block is computed */
        int y = k / 64;                 /* column 0 is j, so rows past k are > k */
        int64_t e0 = e_base + 1;
        int cnt = 0;
        int32_t cmin = INT32_MAX;
        int st;
        if(y > last){
                y = last;
        }
        for(int w = 0; w <= y; w++){
                P[w] = ~(uint64_t)0;
                M[w] = 0;
                S[w] = 64 * (int64_t)(w + 1);
        }
        for(int64_t i = 0; i < n; i++){
                const uint64_t *row = peq + (size_t)map[t[i]] * nwp;
                uint64_t hp = 0;
                uint64_t hn = 0;
                int32_t d = over;
                if(y < last && S[y] <= k){
                        y++;
                        P[y] = ~(uint64_t)0;
                        M[y] = 0;
                        S[y] = S[y - 1] + 64;
                        if(y == last){
                                dm = S[y - 1] + r + 1;
                        }
                }
                for(int w = 0; w <= y; w++){
                        const uint64_t Pv = P[w];
                        const uint64_t Mv = M[w];
                        uint64_t Eq = row[w];
                        const uint64_t Xv = Eq | Mv;
                        Eq |= hn;
                        const uint64_t Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
                        uint64_t Ph = Mv | ~(Xh | Pv);
                        uint64_t Mh = Pv & Xh;
                        const uint64_t op = Ph >> 63;
                        const uint64_t on = Mh >> 63;
                        if(w == last){
                                dm += (int64_t)((Ph >> r) & 1) - (int64_t)((Mh >> r) & 1);
                                d = dm <= k ? (int32_t)dm : over;
                        }
                        Ph = (Ph << 1) | hp;
                        Mh = (Mh << 1) | hn;
                        P[w] = Mh | ~(Xv | Ph);
                        M[w] = Ph & Xv;
                        S[w] += (int64_t)op - (int64_t)on;
                        hp = op;
                        hn = on;
                }
                while(y > 0 && S[y] >= (int64_t)k + 64){
                        y--;
                }
                buf[cnt++] = d;
                cmin = d < cmin ? d : cmin;
                if(cnt == TLD__BPM_CHUNK){
                        if((st = sink(ctx, buf, e0, cnt, cmin)) != 0){
                                return st;
                        }
                        e0 += cnt;
                        cnt = 0;
                        cmin = INT32_MAX;
                }
        }
        return cnt ? sink(ctx, buf, e0, cnt, cmin) : 0;
}

/* m <= 64: four stretches of text at once. Stretch j is the T bytes from a[j] (all inside
 * the text); its distances go to sinks[j] (ctxs[j]) for end positions a[j] + 1 ..
 * a[j] + T. Each stretch is the word kernel on its own; the four are written out side by
 * side so their steps overlap. Returns -1 when a sink fails. */
static int tld__bpm_scan_word4(const tld_bpm *b, const uint8_t *t, const int64_t *a, int64_t T,
                               tld__bpm_sink const *sinks, void *const *ctxs)
{
        int32_t buf[4][TLD__BPM_CHUNK];
        int32_t cmin[4];
        const uint16_t *map = b->map;
        const uint64_t *peq = b->peq;
        const size_t nwp = (size_t)b->nwp;
        const int r = b->m - 1;
        const uint8_t *t0 = t + a[0], *t1 = t + a[1], *t2 = t + a[2], *t3 = t + a[3];
        uint64_t P0 = ~(uint64_t)0, P1 = ~(uint64_t)0, P2 = ~(uint64_t)0, P3 = ~(uint64_t)0;
        uint64_t M0 = 0, M1 = 0, M2 = 0, M3 = 0;
        int32_t d0 = b->m, d1 = b->m, d2 = b->m, d3 = b->m;
        for(int64_t i0 = 0; i0 < T; i0 += TLD__BPM_CHUNK){
                const int cnt = (int)(T - i0 < TLD__BPM_CHUNK ? T - i0 : TLD__BPM_CHUNK);
                cmin[0] = cmin[1] = cmin[2] = cmin[3] = INT32_MAX;
#define TLD__BPM_STEP(P, M, D, TP, J)                                                    \
                {                                                                       \
                        const uint64_t Eq = peq[(size_t)map[TP[i0 + ii]] * nwp];        \
                        const uint64_t Xv = Eq | M;                                     \
                        const uint64_t Xh = (((Eq & P) + P) ^ P) | Eq;                  \
                        uint64_t Ph = M | ~(Xh | P);                                    \
                        uint64_t Mh = P & Xh;                                           \
                        D += (int32_t)((Ph >> r) & 1) - (int32_t)((Mh >> r) & 1);       \
                        Ph <<= 1;                                                       \
                        Mh <<= 1;                                                       \
                        P = Mh | ~(Xv | Ph);                                            \
                        M = Ph & Xv;                                                    \
                        buf[J][ii] = D;                                                 \
                        cmin[J] = D < cmin[J] ? D : cmin[J];                            \
                }
                for(int ii = 0; ii < cnt; ii++){
                        TLD__BPM_STEP(P0, M0, d0, t0, 0)
                        TLD__BPM_STEP(P1, M1, d1, t1, 1)
                        TLD__BPM_STEP(P2, M2, d2, t2, 2)
                        TLD__BPM_STEP(P3, M3, d3, t3, 3)
                }
#undef TLD__BPM_STEP
                for(int j = 0; j < 4; j++){
                        if(sinks[j](ctxs[j], buf[j], a[j] + 1 + i0, cnt, cmin[j]) < 0){
                                return -1;
                        }
                }
        }
        return 0;
}

/* Scan text t[0, n) (text position e_base + i for t[i]) with the pattern (rev: the
 * reversed pattern), anchored at t[0] or not; ws holds 4 * nwp words. Returns the last
 * sink result: 0 done, 1 stopped, -1 failed. */
static int tld__bpm_scan(const tld_bpm *b, int rev, const uint8_t *t, int64_t n, int anchored,
                         int64_t e_base, uint64_t *ws, tld__bpm_sink sink, void *ctx)
{
        const uint64_t *peq = rev ? b->rpeq : b->peq;
        switch(b->kernel){
        case TLD__BPM_WORD:
                return tld__bpm_scan_word(b, peq, t, n, anchored, e_base, sink, ctx);
#if defined(TLD__BPM_HAVE_AVX2)
        case TLD__BPM_AVX2:
                return tld__bpm_scan_wave(b, peq, t, n, anchored, e_base, ws, sink, ctx);
#elif defined(TLD__BPM_HAVE_NEON)
        case TLD__BPM_NEON:
                return tld__bpm_scan_wave(b, peq, t, n, anchored, e_base, ws, sink, ctx);
#endif
        default:
                return tld__bpm_scan_block(b, peq, t, n, anchored, e_base, ws, sink, ctx);
        }
}

/* ------------------------------------------------------------------------ */
/* starts, matches, the best match                                          */
/* ------------------------------------------------------------------------ */

/* Working memory of one call: a scan forward, a scan backward from a match's end, and
 * the reversed text before that end. */
struct tld__bpm_work {
        uint64_t *ws_fwd;
        uint64_t *ws_rev;
        uint8_t *rbuf;          /* 2 * m bytes: a match is at most m + d <= 2m long */
};

static int tld__bpm_work_alloc(const tld_bpm *b, struct tld__bpm_work *w)
{
        const size_t words = 4 * (size_t)b->nwp;
        w->ws_fwd = NULL;
        w->ws_rev = NULL;
        w->rbuf = NULL;
        MMALLOC(w->ws_fwd, 2 * words * sizeof(uint64_t));
        w->ws_rev = w->ws_fwd + words;
        MMALLOC(w->rbuf, 2 * (size_t)b->m);
        return OK;
ERROR:
        MFREE(w->ws_fwd);
        return FAIL;
}

static void tld__bpm_work_free(struct tld__bpm_work *w)
{
        MFREE(w->ws_fwd);
        MFREE(w->rbuf);
}

struct tld__bpm_start_ctx {
        int32_t d;
        int64_t j;              /* the longest stretch so far with distance d */
};

static int tld__bpm_start_sink(void *ctx, const int32_t *d, int64_t e0, int cnt, int32_t dmin)
{
        struct tld__bpm_start_ctx *c = ctx;
        if(dmin > c->d){                /* no stretch in this chunk has distance d */
                return 0;
        }
        for(int i = 0; i < cnt; i++){
                if(d[i] == c->d){
                        c->j = e0 + i;
                }
        }
        return 0;
}

/* The start of the match ending at e with distance d: e - j for the largest j with
 * distance(pattern, t[e - j, e)) == d, j <= m + d. */
static int tld__bpm_start(const tld_bpm *b, const uint8_t *t, int64_t e, int32_t d,
                          struct tld__bpm_work *w, int64_t *start)
{
        int64_t len = (int64_t)b->m + d;
        struct tld__bpm_start_ctx c;
        if(len > e){
                len = e;
        }
        for(int64_t j = 0; j < len; j++){
                w->rbuf[j] = t[e - 1 - j];
        }
        c.d = d;
        c.j = d == b->m ? 0 : -1;       /* j = 0, the empty stretch, has distance m */
        RUN(tld__bpm_scan(b, 1, w->rbuf, len, 1, 0, w->ws_rev, tld__bpm_start_sink, &c) == 0 ? OK : FAIL);
        ASSERT(c.j >= 0, "Internal error: no start for the match ending at %lld (distance %d)",
               (long long)e, (int)d);
        *start = e - c.j;
        return OK;
ERROR:
        return FAIL;
}

/* The candidate test for the end positions of one stretch of text: an end in [lo, hi)
 * whose distance is <= k and no larger than either neighbour's. */
struct tld__bpm_det {
        int64_t lo;
        int64_t hi;
        int64_t ep;             /* the end position waiting for its right neighbour, or -1 */
        int32_t dp;             /* its distance */
        int32_t dl;             /* the distance at ep - 1 (m before the first position) */
};

/* Candidates of one stretch, kept until the stretches before it are done. */
struct tld__bpm_cand {
        int64_t e;
        int32_t d;
};
struct tld__bpm_cands {
        struct tld__bpm_cand *x;
        int64_t n;
        int64_t n_alloc;
};

struct tld__bpm_find_ctx {
        const tld_bpm *b;
        const uint8_t *t;
        int64_t n;
        struct tld__bpm_work *w;
        tld_bpm_hit *hits;
        int64_t n_hits;
        int64_t n_alloc;
        struct tld__bpm_det det;        /* of the stretch scanned one at a time */
        int32_t k;
        int band;               /* scan with the cut-off */
        int first_only;         /* stop once the first match is settled */
};

/* a is the better of two overlapping matches */
static int tld__bpm_better(const tld_bpm_hit *a, const tld_bpm_hit *b)
{
        if(a->dist != b->dist){
                return a->dist < b->dist;
        }
        if(a->start != b->start){
                return a->start < b->start;
        }
        return a->end > b->end;
}

/* A local minimum of the distance at end e, in text order: find its start and keep it
 * unless an overlapping kept match is better; the kept matches it beats are dropped. */
static int tld__bpm_candidate(struct tld__bpm_find_ctx *c, int64_t e, int32_t d)
{
        tld_bpm_hit h;
        int64_t j;
        h.end = e;
        h.dist = (int)d;
        RUN(tld__bpm_start(c->b, c->t, e, d, c->w, &h.start));
        /* kept matches end in order, so the ones overlapping h are the last few */
        j = c->n_hits;
        while(j > 0 && c->hits[j - 1].end > h.start){
                if(!tld__bpm_better(&h, &c->hits[j - 1])){
                        return OK;
                }
                j--;
        }
        c->n_hits = j;
        if(c->n_hits == c->n_alloc){
                int64_t na = c->n_alloc ? c->n_alloc * 2 : 16;
                MREALLOC(c->hits, (size_t)na * sizeof(tld_bpm_hit));
                c->n_alloc = na;
        }
        c->hits[c->n_hits++] = h;
        return OK;
ERROR:
        return FAIL;
}

static void tld__bpm_det_init(struct tld__bpm_det *s, int64_t lo, int64_t hi, int32_t m)
{
        s->lo = lo;
        s->hi = hi;
        s->ep = -1;
        s->dp = m;
        s->dl = m;
}

/* Feed the distance d at end e; e - 1 is then decided. A candidate goes to out, or
 * straight to c when out is NULL. */
static int tld__bpm_det_step(struct tld__bpm_det *s, int64_t e, int32_t d, struct tld__bpm_find_ctx *c,
                             struct tld__bpm_cands *out)
{
        if(s->ep >= 0){
                if(s->ep >= s->lo && s->ep < s->hi && s->dp <= c->k && s->dp <= s->dl && s->dp <= d){
                        if(out){
                                if(out->n == out->n_alloc){
                                        int64_t na = out->n_alloc ? out->n_alloc * 2 : 64;
                                        MREALLOC(out->x, (size_t)na * sizeof(struct tld__bpm_cand));
                                        out->n_alloc = na;
                                }
                                out->x[out->n].e = s->ep;
                                out->x[out->n].d = s->dp;
                                out->n++;
                        }else{
                                RUN(tld__bpm_candidate(c, s->ep, s->dp));
                        }
                }
                s->dl = s->dp;
        }
        s->ep = e;
        s->dp = d;
        return OK;
ERROR:
        return FAIL;
}

/* Feed a chunk. When every distance in it is above k no position in it is a candidate:
 * only its first distance is needed (the right neighbour of the position waiting from
 * the last chunk) and its last two (the new waiting position and its left neighbour);
 * the state is then what stepping through the whole chunk would leave. */
static int tld__bpm_det_chunk(struct tld__bpm_det *s, const int32_t *d, int64_t e0, int cnt, int32_t dmin,
                              struct tld__bpm_find_ctx *c, struct tld__bpm_cands *out)
{
        if(dmin > c->k){
                RUN(tld__bpm_det_step(s, e0, d[0], c, out));
                if(cnt > 1){
                        s->dl = d[cnt - 2];
                        s->ep = e0 + cnt - 1;
                        s->dp = d[cnt - 1];
                }
                return OK;
        }
        for(int i = 0; i < cnt; i++){
                RUN(tld__bpm_det_step(s, e0 + i, d[i], c, out));
        }
        return OK;
ERROR:
        return FAIL;
}

/* every later match starts at or after e - (m + k), so the first kept match is final */
static int tld__bpm_settled(const struct tld__bpm_find_ctx *c, int64_t e)
{
        return c->n_hits > 0 && e - ((int64_t)c->b->m + c->k) >= c->hits[0].end;
}

static int tld__bpm_find_sink(void *ctx, const int32_t *d, int64_t e0, int cnt, int32_t dmin)
{
        struct tld__bpm_find_ctx *c = ctx;
        if(!c->first_only || dmin > c->k){
                if(tld__bpm_det_chunk(&c->det, d, e0, cnt, dmin, c, NULL) != OK){
                        return -1;
                }
                return c->first_only && tld__bpm_settled(c, e0 + cnt - 1) ? 1 : 0;
        }
        for(int i = 0; i < cnt; i++){
                if(tld__bpm_det_step(&c->det, e0 + i, d[i], c, NULL) != OK){
                        return -1;
                }
                if(tld__bpm_settled(c, e0 + i)){
                        return 1;
                }
        }
        return 0;
}

/* Candidates ending in [lo, hi), one stretch scanned on its own, straight to c. A match
 * ending at e starts at most m + k <= 2m bytes before it, so a scan from lo - 1 - 2m
 * gives the same distances from lo - 1 on as a scan from the start of the text. Returns
 * 0, or 1 when the first match is settled (first_only). */
static int tld__bpm_find_seg(struct tld__bpm_find_ctx *c, int64_t lo, int64_t hi)
{
        const tld_bpm *b = c->b;
        int64_t a = lo - 1 - 2 * (int64_t)b->m;
        const int64_t end = hi <= c->n ? hi : c->n;     /* bytes [a, end): ends a + 1 .. end */
        int st;
        if(a < 0){
                a = 0;
        }
        tld__bpm_det_init(&c->det, lo, hi, b->m);
        if(c->band){
                st = tld__bpm_scan_band(b, b->peq, c->t + a, end - a, c->k, a, c->w->ws_fwd, tld__bpm_find_sink, c);
        }else{
                st = tld__bpm_scan(b, 0, c->t + a, end - a, 0, a, c->w->ws_fwd, tld__bpm_find_sink, c);
        }
        ASSERT(st >= 0, "Search failed");
        if(st == 0 && hi > c->n && c->det.ep >= 0){     /* the last position has no right neighbour */
                RUN(tld__bpm_det_step(&c->det, c->det.ep + 1, INT32_MAX, c, NULL));
        }
        return st;
ERROR:
        return -1;
}

struct tld__bpm_seg_ctx {
        struct tld__bpm_find_ctx *c;
        struct tld__bpm_det det;
        struct tld__bpm_cands *out;
};

static int tld__bpm_seg_sink(void *ctx, const int32_t *d, int64_t e0, int cnt, int32_t dmin)
{
        struct tld__bpm_seg_ctx *g = ctx;
        return tld__bpm_det_chunk(&g->det, d, e0, cnt, dmin, g->c, g->out) == OK ? 0 : -1;
}

/* m <= 64: candidates ending in [R, R + 4Q), four stretches of Q ends at once; each also
 * scans the 2m + 1 bytes before its stretch and the one end after it. The candidates
 * then go to c in text order. Needs R >= 2m + 1 and R + 4Q <= n. */
static int tld__bpm_find_word4(struct tld__bpm_find_ctx *c, int64_t R, int64_t Q, struct tld__bpm_cands *out)
{
        const int64_t warm = 2 * (int64_t)c->b->m + 1;
        struct tld__bpm_seg_ctx g[4];
        tld__bpm_sink sinks[4];
        void *ctxs[4];
        int64_t a[4];
        for(int j = 0; j < 4; j++){
                const int64_t lo = R + j * Q;
                g[j].c = c;
                g[j].out = &out[j];
                out[j].n = 0;
                tld__bpm_det_init(&g[j].det, lo, lo + Q, c->b->m);
                a[j] = lo - warm;
                sinks[j] = tld__bpm_seg_sink;
                ctxs[j] = &g[j];
        }
        ASSERT(tld__bpm_scan_word4(c->b, c->t, a, Q + warm, sinks, ctxs) == 0, "Search failed");
        for(int j = 0; j < 4; j++){
                for(int64_t i = 0; i < out[j].n; i++){
                        RUN(tld__bpm_candidate(c, out[j].x[i].e, out[j].x[i].d));
                }
        }
        return OK;
ERROR:
        return FAIL;
}

static void tld__bpm_find_init(struct tld__bpm_find_ctx *c, const tld_bpm *b, const uint8_t *t, int64_t n,
                               int32_t k, int first_only, struct tld__bpm_work *w)
{
        c->b = b;
        c->t = t;
        c->n = n;
        c->w = w;
        c->hits = NULL;
        c->n_hits = 0;
        c->n_alloc = 0;
        c->k = k;
        c->first_only = first_only;
        c->band = b->nw > 1 && (int64_t)(k + 1) * TLD__BPM_BAND_RATIO <= b->m;
        tld__bpm_det_init(&c->det, 1, n + 1, b->m);
}

/* All matches with distance <= k. Short patterns in long texts: the first 2m ends on
 * their own, then four stretches at a time, then the rest on its own. */
static int tld__bpm_find_all(struct tld__bpm_find_ctx *c)
{
        struct tld__bpm_cands out[4] = { { NULL, 0, 0 }, { NULL, 0, 0 }, { NULL, 0, 0 }, { NULL, 0, 0 } };
        const int64_t n = c->n;
        int64_t R = 2 * (int64_t)c->b->m + 1;
        if(c->b->kernel != TLD__BPM_WORD || n - R < 4 * (int64_t)TLD__BPM_IL_QMIN){
                ASSERT(tld__bpm_find_seg(c, 1, n + 1) == 0, "Search failed");
                return OK;
        }
        ASSERT(tld__bpm_find_seg(c, 1, R) == 0, "Search failed");
        while(n - R >= 4 * (int64_t)TLD__BPM_IL_QMIN){
                int64_t Q = (n - R) / 4;
                if(Q > TLD__BPM_IL_QMAX){
                        Q = TLD__BPM_IL_QMAX;
                }
                RUN(tld__bpm_find_word4(c, R, Q, out));
                R += 4 * Q;
        }
        ASSERT(tld__bpm_find_seg(c, R, n + 1) == 0, "Search failed");
        for(int j = 0; j < 4; j++){
                MFREE(out[j].x);
        }
        return OK;
ERROR:
        for(int j = 0; j < 4; j++){
                MFREE(out[j].x);
        }
        return FAIL;
}

int tld_bpm_find(const tld_bpm *b, const char *text, int64_t len, int k, tld_bpm_hit **hits, int64_t *n_hits)
{
        struct tld__bpm_work w = { NULL, NULL, NULL };
        struct tld__bpm_find_ctx c;
        c.hits = NULL;
        ASSERT(b != NULL, "No pattern");
        ASSERT(text != NULL, "No text");
        ASSERT(hits != NULL, "No output pointer for the matches");
        ASSERT(n_hits != NULL, "No output pointer for the number of matches");
        ASSERT(*hits == NULL, "*hits must be NULL");
        ASSERT(len >= 0, "Text length must not be negative (got %lld)", (long long)len);
        ASSERT(k >= 0, "k must not be negative (got %d)", k);
        *n_hits = 0;
        if(k > b->m){
                k = b->m;
        }
        if(len == 0){
                return OK;
        }
        RUN(tld__bpm_work_alloc(b, &w));
        tld__bpm_find_init(&c, b, (const uint8_t *)text, len, k, 0, &w);
        RUN(tld__bpm_find_all(&c));
        tld__bpm_work_free(&w);
        *hits = c.hits;
        *n_hits = c.n_hits;
        return OK;
ERROR:
        MFREE(c.hits);
        tld__bpm_work_free(&w);
        return FAIL;
}

struct tld__bpm_min_ctx {
        int32_t d;
        int64_t e;              /* the first end in [lo, hi) with distance d */
        int64_t lo;
        int64_t hi;
};

static int tld__bpm_min_sink(void *ctx, const int32_t *d, int64_t e0, int cnt, int32_t dmin)
{
        struct tld__bpm_min_ctx *c = ctx;
        if(dmin >= c->d){               /* nothing here is smaller than the minimum so far */
                return 0;
        }
        for(int i = 0; i < cnt; i++){
                const int64_t e = e0 + i;
                if(d[i] < c->d && e >= c->lo && e < c->hi){
                        c->d = d[i];
                        c->e = e;
                        if(c->d == 0){
                                return 1;       /* nothing beats an exact match */
                        }
                }
        }
        return 0;
}

/* The smallest distance and the first end with it: as tld__bpm_find_all, the first 2m
 * ends, then four stretches at a time, then the rest; a stretch's minimum counts only
 * when it is smaller than every one before it. */
static int tld__bpm_min_all(const tld_bpm *b, const uint8_t *t, int64_t n, uint64_t *ws, struct tld__bpm_min_ctx *mc)
{
        const int64_t warm = 2 * (int64_t)b->m + 1;
        int64_t R = warm;
        mc->d = INT32_MAX;
        mc->e = -1;
        mc->lo = 1;
        mc->hi = n + 1;
        if(b->kernel != TLD__BPM_WORD || n - R < 4 * (int64_t)TLD__BPM_IL_QMIN){
                ASSERT(tld__bpm_scan(b, 0, t, n, 0, 0, ws, tld__bpm_min_sink, mc) >= 0, "Search failed");
                return OK;
        }
        mc->hi = R;
        ASSERT(tld__bpm_scan(b, 0, t, R - 1, 0, 0, ws, tld__bpm_min_sink, mc) >= 0, "Search failed");
        while(mc->d > 0 && n - R >= 4 * (int64_t)TLD__BPM_IL_QMIN){
                struct tld__bpm_min_ctx g[4];
                tld__bpm_sink sinks[4];
                void *ctxs[4];
                int64_t a[4];
                int64_t Q = (n - R) / 4;
                if(Q > TLD__BPM_IL_QMAX){
                        Q = TLD__BPM_IL_QMAX;
                }
                for(int j = 0; j < 4; j++){
                        g[j].d = mc->d;         /* only a smaller distance is of use */
                        g[j].e = -1;
                        g[j].lo = R + j * Q;
                        g[j].hi = R + (j + 1) * Q;
                        a[j] = g[j].lo - warm;
                        sinks[j] = tld__bpm_min_sink;
                        ctxs[j] = &g[j];
                }
                ASSERT(tld__bpm_scan_word4(b, t, a, Q + warm, sinks, ctxs) == 0, "Search failed");
                for(int j = 0; j < 4; j++){
                        if(g[j].e >= 0 && g[j].d < mc->d){
                                mc->d = g[j].d;
                                mc->e = g[j].e;
                        }
                }
                R += 4 * Q;
        }
        if(mc->d > 0 && R <= n){
                struct tld__bpm_min_ctx g;
                int64_t a = R - warm;
                g.d = mc->d;
                g.e = -1;
                g.lo = R;
                g.hi = n + 1;
                ASSERT(tld__bpm_scan(b, 0, t + a, n - a, 0, a, ws, tld__bpm_min_sink, &g) >= 0, "Search failed");
                if(g.e >= 0 && g.d < mc->d){
                        mc->d = g.d;
                        mc->e = g.e;
                }
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_bpm_best(const tld_bpm *b, const char *text, int64_t len, tld_bpm_hit *hit)
{
        struct tld__bpm_work w = { NULL, NULL, NULL };
        struct tld__bpm_find_ctx c;
        struct tld__bpm_min_ctx mc;
        const uint8_t *t = (const uint8_t *)text;
        c.hits = NULL;
        ASSERT(b != NULL, "No pattern");
        ASSERT(text != NULL, "No text");
        ASSERT(hit != NULL, "No output pointer");
        ASSERT(len >= 0, "Text length must not be negative (got %lld)", (long long)len);
        if(len == 0){
                hit->start = 0;
                hit->end = 0;
                hit->dist = b->m;
                return OK;
        }
        RUN(tld__bpm_work_alloc(b, &w));
        /* pass 1: the smallest distance and where it first occurs */
        RUN(tld__bpm_min_all(b, t, len, w.ws_fwd, &mc));
        /* pass 2: the first match with that distance, which ends at mc.e or later (none
         * before mc.e is that small) */
        tld__bpm_find_init(&c, b, t, len, mc.d, 1, &w);
        ASSERT(tld__bpm_find_seg(&c, mc.e, len + 1) >= 0, "Search failed");
        ASSERT(c.n_hits > 0, "Internal error: no match with distance %d", (int)mc.d);
        *hit = c.hits[0];
        MFREE(c.hits);
        tld__bpm_work_free(&w);
        return OK;
ERROR:
        MFREE(c.hits);
        tld__bpm_work_free(&w);
        return FAIL;
}

static int tld__bpm_last_sink(void *ctx, const int32_t *d, int64_t e0, int cnt, int32_t dmin)
{
        (void)e0;
        (void)dmin;
        *(int32_t *)ctx = d[cnt - 1];
        return 0;
}

/* Anchored at the first text byte, row m after the last byte is the distance of the
 * whole text. */
int tld_bpm_distance(const tld_bpm *b, const char *text, int64_t len, int *dist)
{
        uint64_t *ws = NULL;
        int32_t d;
        ASSERT(b != NULL, "No pattern");
        ASSERT(text != NULL, "No text");
        ASSERT(dist != NULL, "No output pointer");
        ASSERT(len >= 0 && len <= (int64_t)INT32_MAX - b->m,
               "Text length must be 0 to %lld (got %lld)", (long long)INT32_MAX - b->m, (long long)len);
        d = b->m;
        if(len > 0){
                if(b->kernel != TLD__BPM_WORD){         /* the word kernel needs no working memory */
                        MMALLOC(ws, 4 * (size_t)b->nwp * sizeof(uint64_t));
                }
                ASSERT(tld__bpm_scan(b, 0, (const uint8_t *)text, len, 1, 0, ws, tld__bpm_last_sink, &d) == 0,
                       "Distance failed");
                MFREE(ws);
        }
        *dist = (int)d;
        return OK;
ERROR:
        MFREE(ws);
        return FAIL;
}

#undef TLD__BPM_HAVE_AVX2
#undef TLD__BPM_HAVE_NEON
#undef TLD__BPM_TARGET
#undef TLD__BPM_AVX2_MIN
#undef TLD__BPM_NEON_MIN
#undef TLD__BPM_CHUNK
#undef TLD__BPM_IL_QMAX
#undef TLD__BPM_IL_QMIN
#undef TLD__BPM_BAND_RATIO
#undef TLD__U4_FN

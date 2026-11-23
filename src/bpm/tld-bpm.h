/* bpm: approximate string search: where a pattern occurs in a text with at most k edits
 * (substitutions, insertions, deletions), by Myers' bit-parallel algorithm, for patterns
 * and texts of any length. AVX2 or NEON kernels are chosen at run time and give the same
 * results as the plain ones. A compiled pattern is read-only: threads may share it. */
#ifndef TLD_BPM_H
#define TLD_BPM_H

#include "../core/tld-core.h"
#include <stdint.h>

/* A compiled pattern (opaque). Create with tld_bpm_alloc, free with tld_bpm_free. A
 * search only reads it, so several threads may search with one pattern at once. */
typedef struct tld_bpm tld_bpm;

/* One match: text[start, end) aligns to the pattern with dist edits (its Levenshtein
 * distance). start == end only when dist is the pattern length (every byte deleted). */
typedef struct {
        int64_t start;
        int64_t end;
        int dist;
} tld_bpm_hit;

/* Flags for tld_bpm_alloc. TLD_BPM_NOCASE: an ASCII letter in the pattern matches
 * either case in the text (other bytes match only themselves). TLD_BPM_PLAIN: never use
 * the AVX2 or NEON kernels; results are the same, so this is for tests and timing. */
#define TLD_BPM_NOCASE 1
#define TLD_BPM_PLAIN 2

/* Compile the len (> 0) bytes at pattern into *bpm; the bytes may be anything, including
 * 0, and are copied. flags is 0 or TLD_BPM_NOCASE and TLD_BPM_PLAIN or'ed together. The
 * pattern takes about 16 bytes per distinct byte for every 64 bytes of length (a
 * 10000-byte DNA pattern about 13 KB). Free with tld_bpm_free. */
TLD_API int tld_bpm_alloc(tld_bpm **bpm, const char *pattern, int len, int flags);

/* Free a compiled pattern; NULL is ignored. */
TLD_API void tld_bpm_free(tld_bpm *b);

/* The best match of the pattern in the len (>= 0) bytes at text: the smallest distance;
 * of the matches with that distance the first one tld_bpm_find would return for k = that
 * distance. An empty text gives {0, 0, pattern length}. Time grows with
 * len * (pattern length / 64), plus a pass over at most twice the pattern length.
 *
 *   tld_bpm *b = NULL;
 *   tld_bpm_hit h;
 *   const char text[] = "a call to tld_json_get_string";
 *   RUN(tld_bpm_alloc(&b, "tld_jsn_get_str", 15, 0));
 *   RUN(tld_bpm_best(b, text, (int64_t)(sizeof text - 1), &h));
 *   ASSERT(h.dist == 1 && h.start == 10 && h.end == 26, "got %d at [%lld, %lld)", h.dist,
 *          (long long)h.start, (long long)h.end);
 *   tld_bpm_free(b);
 */
TLD_API int tld_bpm_best(const tld_bpm *b, const char *text, int64_t len, tld_bpm_hit *hit);

/* Every match with at most k (>= 0) edits in the len (>= 0) bytes at text, in order, none
 * overlapping another: *hits (which must be NULL) gets an array of *n_hits matches, freed
 * with MFREE, or stays NULL when there are none. A match ends where the distance is
 * smallest locally and starts where the longest stretch of text with that distance does;
 * where two such matches overlap, the one with the smaller distance is kept (then the
 * one that starts first, then the longer one), deciding from left to right. k above the
 * pattern length is taken as the pattern length. An empty text has no matches.
 *
 *   tld_bpm *b = NULL;
 *   tld_bpm_hit *hits = NULL;
 *   int64_t n = 0;
 *   const char text[] = "colour, color or colr";
 *   RUN(tld_bpm_alloc(&b, "color", 5, 0));
 *   RUN(tld_bpm_find(b, text, (int64_t)(sizeof text - 1), 1, &hits, &n));
 *   ASSERT(n == 3, "got %lld matches", (long long)n);
 *   ASSERT(hits[1].start == 8 && hits[1].end == 13 && hits[1].dist == 0, "not color");
 *   ASSERT(hits[2].start == 17 && hits[2].end == 21 && hits[2].dist == 1, "not colr");
 *   MFREE(hits);
 *   tld_bpm_free(b);
 */
TLD_API int tld_bpm_find(const tld_bpm *b, const char *text, int64_t len, int k, tld_bpm_hit **hits, int64_t *n_hits);

/* The Levenshtein distance between the pattern and all len (>= 0) bytes at text, into
 * *dist: the fewest substitutions, insertions and deletions that turn one into the other
 * (with TLD_BPM_NOCASE, letters that differ only in case are equal). An empty text gives
 * the pattern length. len is at most INT32_MAX minus the pattern length. To compare one
 * string with many, compile it once and call this for each of the others; for a pattern
 * of at most 64 bytes the call allocates nothing.
 *
 *   tld_bpm *b = NULL;
 *   int d = -1;
 *   RUN(tld_bpm_alloc(&b, "kitten", 6, 0));
 *   RUN(tld_bpm_distance(b, "sitting", 7, &d));
 *   ASSERT(d == 3, "got %d", d);
 *   tld_bpm_free(b);
 */
TLD_API int tld_bpm_distance(const tld_bpm *b, const char *text, int64_t len, int *dist);

/* The kernel searches with b run: "word" (pattern of at most 64 bytes), "block" (longer
 * patterns), "avx2" or "neon" (long patterns, when the CPU has the instructions and
 * TLD_BPM_PLAIN was not given). The string is static; NULL b gives NULL. */
TLD_API const char *tld_bpm_kernel(const tld_bpm *b);

#endif

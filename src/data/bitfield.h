/* bitfield: fixed-length bit sets with set, get and clear, OR, copy and (masked)
 * Hamming distance. */
#ifndef BITFIELD_H
#define BITFIELD_H

#include "../core/tld-core.h"

#include <stdint.h>

/* A set of len bits, stored in alloc_len 64-bit words m. Create with tl_bitfield_alloc,
 * free with tl_bitfield_free. */
typedef struct {
        uint64_t* m;
        int32_t len;
        int32_t alloc_len;
} tl_bitfield;

/* A new bitfield of size bits, all 0, in *bit_f. Free with tl_bitfield_free. */
tld_external  int tl_bitfield_alloc(tl_bitfield **bit_f, int32_t size);

/* Free a bitfield; NULL is ignored. */
tld_external void tl_bitfield_free(tl_bitfield *b);

/* Clear every bit. */
tld_external  int tl_bitfield_reset(tl_bitfield *b);

/* Set bit index to 1, read it into *bit (0 or 1), or clear it to 0. FAIL, without a
 * message, when index is outside [0, len). */
tld_external  int tl_bitfield_set(tl_bitfield *b, int32_t index);
tld_external  int tl_bitfield_get(tl_bitfield *b, int32_t index, uint8_t* bit);
tld_external  int tl_bitfield_clear(tl_bitfield *b, int32_t index);

/* Copy the bits of src into dst; both must have the same length. */
tld_external  int tl_bitfield_copy(tl_bitfield *dst, tl_bitfield *src);

/* ret = b1 | b2, bit by bit; all three must have the same length. */
tld_external  int tl_bitfield_or(tl_bitfield *b1, tl_bitfield *b2,tl_bitfield* ret);

/* The number of positions at which b1 and b2 differ into *dist, counting only positions
 * set in mask (NULL: all). The bitfields (and mask) must have the same length. */
tld_external  int tl_bitfield_hamming(tl_bitfield *b1, tl_bitfield *b2,tl_bitfield *mask, int32_t *dist);

#endif

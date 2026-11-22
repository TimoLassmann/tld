/* bitfield: fixed-length bit sets with set, get and clear, OR, copy and (masked)
 * Hamming distance. */
#ifndef TLD_BITFIELD_H
#define TLD_BITFIELD_H

#include "../core/tld-core.h"

#include <stdint.h>

/* A set of len bits, stored in alloc_len 64-bit words m. Create with tld_bitfield_alloc,
 * free with tld_bitfield_free. */
typedef struct {
        uint64_t* m;
        int32_t len;
        int32_t alloc_len;
} tld_bitfield;

/* A new bitfield of size bits, all 0, in *bit_f. Free with tld_bitfield_free. */
TLD_API  int tld_bitfield_alloc(tld_bitfield **bit_f, int32_t size);

/* Free a bitfield; NULL is ignored. */
TLD_API void tld_bitfield_free(tld_bitfield *b);

/* Clear every bit. */
TLD_API  int tld_bitfield_reset(tld_bitfield *b);

/* Set bit index to 1, read it into *bit (0 or 1), or clear it to 0. FAIL, without a
 * message, when index is outside [0, len). */
TLD_API  int tld_bitfield_set(tld_bitfield *b, int32_t index);
TLD_API  int tld_bitfield_get(tld_bitfield *b, int32_t index, uint8_t* bit);
TLD_API  int tld_bitfield_clear(tld_bitfield *b, int32_t index);

/* Copy the bits of src into dst; both must have the same length. */
TLD_API  int tld_bitfield_copy(tld_bitfield *dst, tld_bitfield *src);

/* ret = b1 | b2, bit by bit; all three must have the same length. */
TLD_API  int tld_bitfield_or(tld_bitfield *b1, tld_bitfield *b2,tld_bitfield* ret);

/* The number of positions at which b1 and b2 differ into *dist, counting only positions
 * set in mask (NULL: all). The bitfields (and mask) must have the same length. */
TLD_API  int tld_bitfield_hamming(tld_bitfield *b1, tld_bitfield *b2,tld_bitfield *mask, int32_t *dist);

#endif

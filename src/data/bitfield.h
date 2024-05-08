#ifndef BITFIELD_H
#define BITFIELD_H

#include "../core/tld-core.h"

#include <stdint.h>

typedef struct {
        uint64_t* m;
        int32_t len;
} tl_bitfield;

tld_external  int tl_bitfield_set(tl_bitfield *b, int32_t index);
tld_external  int tl_bitfield_get(tl_bitfield *b, int32_t index);
tld_external  int tl_bitfield_clear(tl_bitfield *b, int32_t index);

tld_external  int tl_bitfield_alloc(tl_bitfield **bit_f, int32_t size);
tld_external void tl_bitfield_free(tl_bitfield *b);
#endif

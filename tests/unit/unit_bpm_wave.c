/* unit-bpm-wave: unit-bpm with the AVX2/NEON wavefront used from 2 words on (normally
 * from 2 words with AVX2 and 6 with NEON), without the cut-off, so the wavefront is
 * checked against the reference for every pattern length over 64 bytes on every CPU
 * that has it; and with stretches of 16 to 97 end positions for the four-at-once scan,
 * so short texts hold many stretch boundaries. Built from the single header ./tld.h. */
#define TLD__BPM_AVX2_MIN 2
#define TLD__BPM_NEON_MIN 2
#define TLD__BPM_BAND_RATIO 1000000
#define TLD__BPM_IL_QMIN 16
#define TLD__BPM_IL_QMAX 97
#define TLD_IMPLEMENTATION
#include "tld.h"
#include "unit_bpm.c"

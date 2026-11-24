/* unit-bpm-band: unit-bpm with the cut-off (the band kernel) used by tld_bpm_find for
 * every pattern over 64 bytes and every k, and stretches of 8 to 53 end positions for
 * the four-at-once scan. Built from the single header ./tld.h. */
#define TLD__BPM_BAND_RATIO 0
#define TLD__BPM_IL_QMIN 8
#define TLD__BPM_IL_QMAX 53
#define TLD_IMPLEMENTATION
#include "tld.h"
#include "unit_bpm.c"

/* Single-header use: the file with TLD_IMPLEMENTATION needs POSIX.1-2008 and the
 * BSD/XSI extras (realpath, localtime_r, strnlen, ...). A strict -std=c11 build
 * hides them on glibc, so ask for them here, before the first system header. */
#if defined(TLD_IMPLEMENTATION) && !defined(TLD__FEATURES)
#define TLD__FEATURES
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif

#ifndef TLD_H
#define TLD_H

/* Standard headers the library and most programs using it need. */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <float.h>

#include <math.h>

#include "core/tld-core.h"
#include "alloc/tld-alloc.h"
#include "string/tld-string.h"
#include "path/tld-path.h"
#include "timing/tld-timing.h"
#include "bitfield/tld-bitfield.h"
#include "hash/tld-hash.h"
#include "json/tld-json_struct.h"
#include "json/tld-json_mem.h"
#include "json/tld-json.h"
#include "template/tld-template.h"
#include "rng/tld-rng.h"
#include "sort/tld-sort.h"
#include "shuffle/tld-shuffle.h"
#include "stats/tld-stats.h"
#include "stats/tld-quantile.h"
#include "stats/tld-auc.h"
#include "logsum/tld-logsum.h"
#include "kdtree/tld-kdtree.h"
#include "tpool/tld-tpool.h"

#endif

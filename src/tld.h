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

/* Let's include a number of standard headers I commonly use  */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <float.h>

#include <math.h>

#include "core/tld-core.h"

#include "alloc/tld-alloc.h"

#include "rng/tld-rng.h"

#include "kdtree/tld-kdtree.h"
#include "hash/hash.h"

#include "logsum/tld-logsum.h"


/* #include "tpool/tpool_internal.h" */

#include "tpool/tpool.h"

#include "json/tld-json_struct.h"
#include "json/tld-json.h"
#include "json/tld-json_mem.h"
#include "json/tld-json_misc.h"

#include "timing/tld-timing.h"
#include "string/str.h"
#include "template/template.h"

#include "stats/basic.h"
#include "stats/norm_quantile.h"
#include "stats/auc.h"

#include "sort/tld-sort.h"
#include "shuffle/tld-shuffle.h"
#include "misc/misc.h"

#include "data/bitfield.h"

#endif

/* json misc: type guessing for unquoted text (used by JS). */
#ifndef TLD_JSON_MISC_H
#define TLD_JSON_MISC_H

#include "tld-json_struct.h"

#ifdef TLD_JSON_MISC_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

/* The JSON type the len characters at s would get from JS, into *type: TLD_JSON_INT or
 * TLD_JSON_DBL when the whole text is a number ([+-]digits[.digits][e[+-]digits]),
 * TLD_JSON_BOOL_TRUE / _FALSE for true, True, TRUE / false, False, FALSE, otherwise
 * TLD_JSON_STR (also for empty text). Always OK. */
EXTERN int detect_type(char*s, int len, tld_json_type* type);

#undef TLD_JSON_MISC_IMPORT
#undef EXTERN


#endif

/* json misc: type guessing for unquoted text (used by JS). */
#ifndef TLD_JSON_MISC_H
#define TLD_JSON_MISC_H

#include "tld-json_struct.h"

/* The JSON type the len characters at s would get from JS, into *type: TLD_JSON_INT or
 * TLD_JSON_DBL when the whole text is a number ([+-]digits[.digits][e[+-]digits]),
 * TLD_JSON_BOOL_TRUE / _FALSE for true, True, TRUE / false, False, FALSE, otherwise
 * TLD_JSON_STR (also for empty text). Always OK. */
static int tld__json_detect_type(char*s, int len, tld_json_type* type);

#endif

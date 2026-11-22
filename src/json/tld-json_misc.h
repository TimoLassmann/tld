/* json misc: type guessing for unquoted text (used by JS) and token tests kept from the
 * old tokenising parser. */
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

/* 1 if v is a string value consisting of exactly that one character ({, }, [, ], :
 * or ,), else 0. */
EXTERN int is_open_obj(tld_json_val *v);
EXTERN int is_close_obj(tld_json_val *v);
EXTERN int is_open_arr(tld_json_val *v);
EXTERN int is_close_arr(tld_json_val *v);
EXTERN int is_colon(tld_json_val *v);
EXTERN int is_comma(tld_json_val *v);

#undef TLD_JSON_MISC_IMPORT
#undef EXTERN


#endif

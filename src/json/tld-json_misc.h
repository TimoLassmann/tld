#ifndef TLD_JSON_MISC_H
#define TLD_JSON_MISC_H

#include "tld-json_struct.h"

#ifdef TLD_JSON_MISC_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN int detect_type(char*s, int len, tld_json_type* type);

EXTERN int is_open_obj(tld_json_val *v);
EXTERN int is_close_obj(tld_json_val *v);
EXTERN int is_open_arr(tld_json_val *v);
EXTERN int is_close_arr(tld_json_val *v);
EXTERN int is_colon(tld_json_val *v);
EXTERN int is_comma(tld_json_val *v);

#undef TLD_JSON_MISC_IMPORT
#undef EXTERN


#endif

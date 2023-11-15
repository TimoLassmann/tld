#ifndef TLD_JSON_H
#define TLD_JSON_H

#include <stdint.h>
#include <stdio.h>


#include "tld-json_struct.h"

#ifdef TLD_JSON_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN  int tld_json_parse(tld_strbuf *t, tld_json_obj **out);


EXTERN int tld_json_obj_get_str(tld_json_obj *n, char *key, tld_strbuf* res);


EXTERN void tld_json_obj_free(tld_json_obj *n);

EXTERN int tld_json_obj_print(tld_json_obj *n, FILE *out);
EXTERN int tld_json_arr_print(tld_json_arr *n, FILE *out);
#undef TLD_JSON_IMPORT
#undef EXTERN


#endif

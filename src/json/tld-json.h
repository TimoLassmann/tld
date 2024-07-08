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


EXTERN int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret);
/* EXTERN int tld_json_obj_get_str(tld_json_obj *n, char *key, tld_strbuf* res); */
/* EXTERN  int tld_json_obj_get(tld_json_obj *n, char *key, tld_json_val **ret); */

EXTERN void tld_json_obj_free(tld_json_obj *n);

EXTERN int tld_json_obj_print(tld_json_obj *n, FILE *out);
EXTERN int tld_json_arr_print(tld_json_arr *n, FILE *out);

EXTERN  int tld_json_ret_alloc(tld_json_ret** ret);
EXTERN void tld_json_ret_free(tld_json_ret* n);
#undef TLD_JSON_IMPORT
#undef EXTERN


#endif

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

#define JS(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_STRING, .value = (void*)(v)})
#define JO(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_OBJ, .value = (void*)(v)})
#define JA(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_ARR, .value = (void*)(v)})




#define JSON_OBJ(...) json_obj_add_multiple(NULL, (tld_json_decoration[]){__VA_ARGS__}, \
                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))

#define JSON_ARR(...) json_arr_add_multiple(NULL, (tld_json_decoration[]){__VA_ARGS__}, \
                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))

EXTERN tld_json_obj *json_obj_add_multiple(tld_json_obj *obj, tld_json_decoration *kvs, int count);
EXTERN tld_json_arr *json_arr_add_multiple(tld_json_arr *arr, tld_json_decoration *kvs, int count);

/* EXTERN int tld_json_arr_add(tld_json_arr **obj, tld_strbuf*val); */

EXTERN  int tld_json_parse(tld_strbuf *t, tld_json_obj **out);


EXTERN int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret);
/* EXTERN int tld_json_obj_get_str(tld_json_obj *n, char *key, tld_strbuf* res); */
/* EXTERN  int tld_json_obj_get(tld_json_obj *n, char *key, tld_json_val **ret); */



EXTERN int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent);
EXTERN int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b, int indent);
EXTERN int tld_json_obj_print(tld_json_obj *n, FILE *out);
EXTERN int tld_json_arr_print(tld_json_arr *n, FILE *out);

/* EXTERN  int tld_json_ret_alloc(tld_json_ret** ret); */
/* EXTERN void tld_json_ret_free(tld_json_ret* n); */
#undef TLD_JSON_IMPORT
#undef EXTERN


#endif

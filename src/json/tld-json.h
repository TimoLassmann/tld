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

/* Typed decorations. JS above guesses the type from the text ("0.2" becomes
   a number); these do not. JSTR(k,s) is always a string, JNUM(k,d) a number
   (non-finite values are written as null), JINT(k,l) an integer (values
   outside int32_t are stored as double), JBOOL(k,b) true/false, JNULL(k) null.
   A NULL string in JSTR skips the entry, like JS. */
#define JSTR(k,v)  ((tld_json_decoration){.key =(char*)k, .type = VAL_JSTR, .value = (void*)(v)})
#define JNUM(k,v)  ((tld_json_decoration){.key =(char*)k, .type = VAL_NUM,  .value = (void*)&(double){(v)}})
#define JINT(k,v)  ((tld_json_decoration){.key =(char*)k, .type = VAL_INT,  .value = (void*)&(long){(v)}})
#define JBOOL(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_BOOL, .value = (void*)&(int){(v)}})
#define JNULL(k)   ((tld_json_decoration){.key =(char*)k, .type = VAL_NULL, .value = (void*)""})




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
EXTERN int tld_json_set_val(tld_json_obj *n, char *key, tld_json_val *v);

/* Path getters. A path is dot-separated keys with [i] for array elements,
   e.g. "message.content", "embeddings[0]", "models[2].name". tld_json_parse
   stores a top-level array under the key "Top Level Arr"; address it as
   "Top Level Arr[0]..." or simply "[0]...". Keys containing '.' or '[' cannot
   be addressed. All return OK, or FAIL (leaving *out unchanged) when the path
   does not exist or the value has another type. Strings, objects and arrays
   are borrowed from the tree. get_num accepts integers; get_int accepts
   integral doubles inside the int64_t range. JSON null has type
   TLD_JSON_UNDEF and fails every getter. An empty path in get_obj returns
   obj itself. */
EXTERN int tld_json_get_str(tld_json_obj *obj, const char *path, const char **out);
EXTERN int tld_json_get_num(tld_json_obj *obj, const char *path, double *out);
EXTERN int tld_json_get_int(tld_json_obj *obj, const char *path, int64_t *out);
EXTERN int tld_json_get_bool(tld_json_obj *obj, const char *path, int *out);
EXTERN int tld_json_get_obj(tld_json_obj *obj, const char *path, tld_json_obj **out);
EXTERN int tld_json_get_arr(tld_json_obj *obj, const char *path, tld_json_arr **out);



EXTERN int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent);
EXTERN int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b, int indent);
EXTERN int tld_json_obj_print(tld_json_obj *n, FILE *out);
EXTERN int tld_json_arr_print(tld_json_arr *n, FILE *out);

/* EXTERN  int tld_json_ret_alloc(tld_json_ret** ret); */
/* EXTERN void tld_json_ret_free(tld_json_ret* n); */
#undef TLD_JSON_IMPORT
#undef EXTERN


#endif

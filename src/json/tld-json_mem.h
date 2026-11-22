/* json memory: allocate, deep-copy and free JSON objects, arrays and values. */
#ifndef TLD_JSON_MEM_H
#define TLD_JSON_MEM_H

#include "tld-json_struct.h"

/* A new empty object in *obj. Free with tld_json_obj_free. */
TLD_API  int tld_json_obj_alloc(tld_json_obj **obj);

/* Free an object with all its keys and values; NULL is ignored. */
TLD_API void tld_json_obj_free(tld_json_obj *n);

/* Deep copy of the object s into a new object in *target (free with tld_json_obj_free). */
TLD_API  int tld_json_obj_copy(tld_json_obj **target, tld_json_obj *s);

/* Make room for more entries: chk_space grows n when every slot is used (n == n_alloc),
 * realloc always grows it by half. Only needed by code that fills key[n] / v[n]
 * directly; TLD_JSON_OBJECT and the parser do this themselves. */
TLD_API  int tld__json_obj_chk_space(tld_json_obj *n);
TLD_API  int tld__json_obj_realloc(tld_json_obj *n);

/* A new empty array in *arr. Free with tld_json_arr_free. */
TLD_API  int tld_json_arr_alloc(tld_json_arr **arr);

/* Free an array with all its values; NULL is ignored. */
TLD_API void tld_json_arr_free(tld_json_arr *n);

/* Deep copy of the array s into a new array in *t (free with tld_json_arr_free). */
TLD_API  int tld_json_arr_copy(tld_json_arr **t, tld_json_arr *s);

/* Make room for more values: chk_space grows n when every slot is used, realloc always
 * grows it by half. Only needed by code that fills v[n] directly. */
TLD_API  int tld__json_arr_chk_space(tld_json_arr *n);
TLD_API  int tld__json_arr_realloc(tld_json_arr *n);

/* A new value of type TLD_JSON_NULL (null) in *node. Free with tld_json_val_free. */
TLD_API   int tld_json_val_alloc(tld_json_val **node);

/* Free a value and everything it owns; NULL is ignored. */
TLD_API  void tld_json_val_free(tld_json_val *n);

/* Deep copy s into t. Whatever t held is released first, so t may change type; s may
 * live inside t. */
TLD_API   int tld_json_val_copy(tld_json_val *t, tld_json_val *s);

#endif

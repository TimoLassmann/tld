/* json memory: allocate, deep-copy and free JSON objects, arrays and values. */
#ifndef TLD_JSON_MEM_H
#define TLD_JSON_MEM_H

#include "tld-json_struct.h"

#ifdef TLD_JSON_MEM_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

/* Deep copy of the object s into a new object in *target (free with tld_json_obj_free). */
EXTERN  int tld_json_obj_copy(tld_json_obj **target, tld_json_obj *s);

/* A new empty object in *obj. Free with tld_json_obj_free. */
EXTERN  int tld_json_obj_alloc(tld_json_obj **obj);

/* Make room for more entries: chk_space grows n when every slot is used (n == n_alloc),
 * realloc always grows it by half. Only needed by code that fills key[n] / v[n]
 * directly; JSON_OBJ and the parser do this themselves. */
EXTERN  int tld_json_obj_chk_space(tld_json_obj *n);
EXTERN  int tld_json_obj_realloc(tld_json_obj *n);

/* Free an object with all its keys and values; NULL is ignored. */
EXTERN void tld_json_obj_free(tld_json_obj *n);

/* Deep copy s into t. Whatever t held is released first, so t may change type; s may
 * live inside t. */
EXTERN   int tld_json_val_copy(tld_json_val *t, tld_json_val *s);

/* A new value of type TLD_JSON_UNDEF (null) in *node. Free with tld_json_val_free. */
EXTERN   int tld_json_val_alloc(tld_json_val **node);

/* Free a value and everything it owns; NULL is ignored. */
EXTERN  void tld_json_val_free(tld_json_val *n);


/* Deep copy of the array s into a new array in *t (free with tld_json_arr_free). */
EXTERN  int tld_json_arr_copy(tld_json_arr **t, tld_json_arr *s);

/* A new empty array in *arr. Free with tld_json_arr_free. */
EXTERN  int tld_json_arr_alloc(tld_json_arr **arr);

/* Make room for more values: chk_space grows n when every slot is used, realloc always
 * grows it by half. Only needed by code that fills v[n] directly. */
EXTERN  int tld_json_arr_chk_space(tld_json_arr *n);
EXTERN  int tld_json_arr_realloc(tld_json_arr *n);

/* Free an array with all its values; NULL is ignored. */
EXTERN void tld_json_arr_free(tld_json_arr *n);



#undef TLD_JSON_MEM_IMPORT
#undef EXTERN


#endif

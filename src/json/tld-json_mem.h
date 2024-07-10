#ifndef TLD_JSON_MEM_H
#define TLD_JSON_MEM_H

#include "tld-json_struct.h"

#ifdef TLD_JSON_MEM_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN  int tld_json_obj_copy(tld_json_obj **target, tld_json_obj *s);
EXTERN  int tld_json_obj_alloc(tld_json_obj **obj);
EXTERN  int tld_json_obj_chk_space(tld_json_obj *n);
EXTERN  int tld_json_obj_realloc(tld_json_obj *n);
EXTERN void tld_json_obj_free(tld_json_obj *n);

EXTERN   int tld_json_val_copy(tld_json_val *t, tld_json_val *s);
EXTERN   int tld_json_val_alloc(tld_json_val **node);
EXTERN  void tld_json_val_free(tld_json_val *n);


EXTERN  int tld_json_arr_copy(tld_json_arr **t, tld_json_arr *s);
EXTERN  int tld_json_arr_alloc(tld_json_arr **arr);
EXTERN  int tld_json_arr_chk_space(tld_json_arr *n);
EXTERN  int tld_json_arr_realloc(tld_json_arr *n);
EXTERN void tld_json_arr_free(tld_json_arr *n);



#undef TLD_JSON_MEM_IMPORT
#undef EXTERN


#endif

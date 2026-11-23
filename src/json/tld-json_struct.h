/* json types: the JSON tree. An object (tld_json_obj) maps n keys to values, an array
 * (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its
 * tld_json_type. Containers own everything below them. */
#ifndef TLD_JSON_STRUCT_H
#define TLD_JSON_STRUCT_H

#include <stdint.h>

typedef struct tld_strbuf tld_strbuf;
typedef struct tld_json_obj tld_json_obj;
typedef struct tld_json_arr tld_json_arr;

/* Type tag of a tld_json_val. JSON null is TLD_JSON_NULL. */
typedef enum tld_json_type {
        TLD_JSON_OBJ,
        TLD_JSON_ARR,
        TLD_JSON_STR,
        TLD_JSON_DBL,
        TLD_JSON_INT,
        TLD_JSON_BOOL_TRUE,
        TLD_JSON_BOOL_FALSE,
        TLD_JSON_NULL
} tld_json_type;

/* One JSON value. type selects the member of value: obj, arr, str, d_num (TLD_JSON_DBL),
 * i_num (TLD_JSON_INT) or b_num (the two BOOL types). Integers outside int32_t are stored
 * as TLD_JSON_DBL. */
typedef struct tld_json_val{
        tld_json_type type;
        union{
                tld_json_obj* obj;
                tld_json_arr* arr;
                tld_strbuf* str;
                double d_num;
                int32_t i_num;
                int8_t  b_num;
        } value;
} tld_json_val;

/* A JSON object: key[i] maps to v[i] for i < n, in document order (duplicate keys are
 * kept); n_alloc slots exist. Made by tld_json_parse, TLD_JSON_OBJECT or tld_json_obj_alloc;
 * freed with tld_json_obj_free. top_level_arr is 1 in the object tld_json_parse makes for
 * a top-level array: while it holds just that array, tld_json_dump writes the array. */
typedef struct tld_json_obj{
        tld_strbuf** key;
        tld_json_val** v;
        int n;
        int n_alloc;
        int top_level_arr;
} tld_json_obj;

/* A JSON array of n values v[0..n-1] (read_head is unused). Made by TLD_JSON_ARRAY or
 * tld_json_arr_alloc; freed with tld_json_arr_free. */
typedef struct tld_json_arr {
        tld_json_val** v;
        int read_head;
        int n;
        int n_alloc;
} tld_json_arr;

/* One entry for TLD_JSON_OBJECT / TLD_JSON_ARRAY, made by TLD_JAUTO, TLD_JOBJ, TLD_JARR, TLD_JSTR, TLD_JNUM, TLD_JINT, TLD_JBOOL or
 * TLD_JNULL. */
typedef struct {
        char* key;
        void* value;
        /* TLD__JVAL_AUTO guesses the type from the text (TLD_JAUTO); the values after
           TLD__JVAL_ARR are the typed decorations TLD_JSTR, TLD_JNUM, TLD_JINT, TLD_JBOOL, TLD_JNULL. */
        enum { TLD__JVAL_AUTO, TLD__JVAL_OBJ, TLD__JVAL_ARR, TLD__JVAL_STR, TLD__JVAL_NUM, TLD__JVAL_INT, TLD__JVAL_BOOL, TLD__JVAL_NULL } type;
} tld_json_decoration;

#endif

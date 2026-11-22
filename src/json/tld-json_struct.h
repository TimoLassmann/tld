/* json types: the JSON tree. An object (tld_json_obj) maps n keys to values, an array
 * (tld_json_arr) holds n values, and each value (tld_json_val) is tagged with its
 * tld_json_type. Containers own everything below them. */
#ifndef TLD_JSON_STRUCT_H
#define TLD_JSON_STRUCT_H

#include <stdint.h>

typedef struct tld_string_buffer tld_strbuf;
typedef struct tld_json_obj tld_json_obj;
typedef struct tld_json_arr tld_json_arr;

/* Type tag of a tld_json_val. JSON null is TLD_JSON_UNDEF. */
typedef enum tld_json_type {
        TLD_JSON_OBJ,
        TLD_JSON_ARR,
        TLD_JSON_STR,
        TLD_JSON_DBL,
        TLD_JSON_INT,
        TLD_JSON_BOOL_TRUE,
        TLD_JSON_BOOL_FALSE,
        TLD_JSON_UNDEF
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
 * kept); n_alloc slots exist. Made by tld_json_parse, JSON_OBJ or tld_json_obj_alloc;
 * freed with tld_json_obj_free. */
typedef struct tld_json_obj{
        tld_strbuf** key;
        tld_json_val** v;
        int n;
        int n_alloc;

} tld_json_obj;

/* A JSON array of n values v[0..n-1] (read_head is unused). Made by JSON_ARR or
 * tld_json_arr_alloc; freed with tld_json_arr_free. */
typedef struct tld_json_arr {
        tld_json_val** v;
        int read_head;
        int n;
        int n_alloc;
} tld_json_arr;

/* One entry for JSON_OBJ / JSON_ARR, made by JS, JO, JA, JSTR, JNUM, JINT, JBOOL or
 * JNULL. */
typedef struct {
        char* key;
        void* value;
        /* VAL_STRING guesses the type from the text (JS); the values after
           VAL_ARR are the typed decorations JSTR, JNUM, JINT, JBOOL, JNULL. */
        enum { VAL_STRING, VAL_OBJ, VAL_ARR, VAL_JSTR, VAL_NUM, VAL_INT, VAL_BOOL, VAL_NULL } type;
} tld_json_decoration;

#endif

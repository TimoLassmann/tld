#ifndef TLD_JSON_STRUCT_H
#define TLD_JSON_STRUCT_H

#include <stdint.h>

#ifdef TLD_JSON_STRUCT_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

typedef struct tld_string_buffer tld_strbuf;
/* typedef struct tld_json_val tld_json_val; */
typedef struct tld_json_obj tld_json_obj;
typedef struct tld_json_arr tld_json_arr;

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

typedef struct tld_json_val{
        tld_json_type type;
        union{
                tld_json_obj* obj;
                tld_json_arr* arr;
                tld_strbuf* str;
                double d_num;
                uint32_t i_num;
                uint8_t  b_num;
        } value;
} tld_json_val;

typedef struct tld_json_obj{
        tld_strbuf** key;
        tld_json_val** v;
        int n;
        int n_alloc;

        /* tld_json_node* next; */
} tld_json_obj;

typedef struct tld_json_arr {
        tld_json_val** v;
        int read_head;
        int n;
        int n_alloc;
} tld_json_arr;


#undef TLD_JSON_STRUCT_IMPORT
#undef EXTERN


#endif

#ifndef TLD_HASH_STRUCT_H
#define TLD_HASH_STRUCT_H
#include "../string/tld-string.h"

typedef struct tld_hash_entry {
        tld_strbuf* key;
        tld_strbuf* value;
        struct tld_hash_entry* next; /* For handling collisions using chaining */
} tld_hash_entry;

typedef struct tld_hash {
        tld_hash_entry** table; /* Array of pointers to entries */
        tld_hash_entry* cur_entry;
        int cur_index;
        int table_size; /* Size of the hash table */
        int n; /* Number of entries */
        int n_rep;
        int status;
} tld_hash;

#endif

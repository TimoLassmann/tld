#include "hash.h"
#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/str.h"

#include <stdio.h>

#define LOAD_FACTOR_THRESHOLD 0.75

#include "hash_struct.h"

tld_internal  int tld_hash_entry_alloc(tld_hash_entry **entry);
tld_internal void tld_hash_entry_free(tld_hash_entry *n);

tld_internal unsigned int fnv1a_hash(char *str, int table_size);
tld_internal int tld_hash_resize(tld_hash *h);

static unsigned int fnv1a_hash(char *str, int table_size)
{
        unsigned int hash = 2166136261u;
        int c;
        while ((c = *str++)) {
                hash ^= c;
                hash *= 16777619;
        }
        return hash % table_size;
}

int tld_hash_add(tld_hash **hash, char *key, char *val)
{
        tld_hash* h = NULL;
        if(*hash){
                h = *hash;
        }else{
                RUN(tld_hash_alloc(&h, 1024));
        }


        ASSERT(h != NULL, "No Map");

        // Check load factor
        double load_factor = (double)h->n / (double)h->table_size;

        if (load_factor > LOAD_FACTOR_THRESHOLD) {
                RUN(tld_hash_resize(h)); // Double the table size
        }


        ASSERT(h != NULL, "No Map");

        unsigned int index = fnv1a_hash(key, h->table_size);

        // Check if key already exists
        tld_hash_entry* current = h->table[index];
        while (current) {
                if (strcmp(TLD_STR(current->key), key) == 0) {
                        // Key exists, replace the value
                        tld_strbuf_clear(current->value);
                        tld_append(current->value, val);
                        return OK;
                }
                current = current->next;
        }

        // Key does not exist, create a new entry
        tld_hash_entry* new_entry = NULL;
        tld_hash_entry_alloc(&new_entry);

        tld_append(new_entry->key, key);
        tld_append(new_entry->value, val);
        new_entry->next = h->table[index];
        h->table[index] = new_entry;
        h->n++;

        *hash = h;
        return OK;
ERROR:
        return FAIL;
}

int tld_hash_get(tld_hash *h, char *key, char **val)
{
        ASSERT(h != NULL, "No Map!!");
        ASSERT(key != NULL, "No key");

        unsigned int index = fnv1a_hash(key, h->table_size);
        tld_hash_entry* entry = h->table[index];

        while (entry) {
                if (strcmp(TLD_STR(entry->key), key) == 0) {
                        *val = TLD_STR(entry->value);
                        return OK;
                }
                entry = entry->next;
        }

        *val = NULL; // Key not found
        return FAIL;
ERROR:
        return FAIL;
}

int tld_hash_rename(tld_hash *h, char *key, char* new_key) {
        ASSERT(h != NULL, "No Map!!");
        ASSERT(key != NULL, "No key");

        unsigned int index = fnv1a_hash(key, h->table_size);
        tld_hash_entry* entry = h->table[index];

        while (entry) {
                if (strcmp(TLD_STR(entry->key), key) == 0) {
                        tld_strbuf* tmp = NULL;
                        tld_strbuf_alloc(&tmp, 64);
                        // Remove the old key from the hash table
                        tld_strbuf_copy(tmp, entry->value);

                        tld_hash_del(h, key);
                        // Add the new key with the existing replacement
                        tld_hash_add(&h, new_key, TLD_STR(tmp));
                        tld_strbuf_free(tmp);
                        return OK;
                }
                entry = entry->next;
        }

        return FAIL; // Key not found
ERROR:
        return FAIL;
}

int tld_hash_del(tld_hash *h, char *key)
{
        ASSERT(h != NULL, "No Map!!");
        ASSERT(key != NULL, "No key");

        unsigned int index = fnv1a_hash(key, h->table_size);
        tld_hash_entry* entry = h->table[index];
        tld_hash_entry* prev = NULL;

        while(entry){
                if(strcmp(TLD_STR(entry->key), key) == 0){
                        if(prev){
                                prev->next = entry->next;
                        }else{
                                h->table[index] = entry->next;
                        }
                        // Free the entry
                        tld_hash_entry_free(entry);
                        /* tld_strbuf_free(entry->key); */
                        /* tld_strbuf_free(entry->value); */
                        /* MFREE(entry); */
                        h->n--;
                        return OK;
                }
                prev = entry;
                entry = entry->next;
        }

        return FAIL; // Key not found
ERROR:
        return FAIL;
}

int tld_hash_iter_init(tld_hash *h)
{
        ASSERT(h != NULL, "No Map");
        h->cur_index = 0;
        h->cur_entry = NULL;
        while(h->cur_index < h->table_size && h->table[h->cur_index] == NULL){
                h->cur_index++;
        }

        // Set current_entry to the first non-null entry if found.
        if(h->cur_index < h->table_size){
                h->cur_entry = h->table[h->cur_index];
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_hash_iterator_next(tld_hash* h, char **key,char **value)
{
        ASSERT(h != NULL, "Iterator is NULL");
        ASSERT(key != NULL, "Key pointer is NULL");
        ASSERT(value != NULL, "Value pointer is NULL");

        // If there is no current entry, return 0 (no more entries).
        if (h->cur_entry == NULL) {
                return FAIL;
        }

        // Set the output key and value.
        *key = TLD_STR(h->cur_entry->key);
        *value = TLD_STR(h->cur_entry->value);

        // Move to the next entry in the current linked list.
        h->cur_entry = h->cur_entry->next;


        // If the current entry is NULL, move to the next index in the table.
        while(h->cur_entry == NULL && h->cur_index + 1 < h->table_size){
                h->cur_index++;
                h->cur_entry = h->table[h->cur_index];
        }

        return OK;
ERROR:
        return FAIL;
}


int tld_hash_alloc(tld_hash **hash, int size)
{
        tld_hash* h = NULL;
        int new_size = 0;
        MMALLOC(h, sizeof(tld_hash));

        static const int prime_sizes[] = {
                53, 97, 193, 389, 769, 1543, 3079, 6151,
                12289, 24593, 49157, 98317, 196613, 393241,
                786433, 1572869, 3145739, 6291469, 12582917,
                25165843, 50331653, 100663319, 201326611,
                402653189, 805306457, 1610612741
        };
        static const int num_primes = sizeof(prime_sizes) / sizeof(prime_sizes[0]);

        for (int i = 0; i < num_primes; i++) {
                if (prime_sizes[i] > size) {
                        new_size = prime_sizes[i];
                        break;
                }
        }

        h->table_size = new_size; // Choose an appropriate size
        h->table = NULL;
        // Initialize the map with a default size

        h->n = 0;

        h->cur_index = 0;
        h->cur_entry = NULL;

        MMALLOC(h->table, sizeof(tld_hash_entry*) * h->table_size);
        for (int i = 0; i < h->table_size; i++) {
                h->table[i] = NULL;
        }
        *hash = h;
        return OK;
ERROR:
        if(h){
                tld_hash_free(h);
        }
        return FAIL;
}

int tld_hash_resize(tld_hash *h)
{
        tld_hash_entry **new_table = NULL;

        int new_size = h->table_size;
        static const int prime_sizes[] = {
                53, 97, 193, 389, 769, 1543, 3079, 6151,
                12289, 24593, 49157, 98317, 196613, 393241,
                786433, 1572869, 3145739, 6291469, 12582917,
                25165843, 50331653, 100663319, 201326611,
                402653189, 805306457, 1610612741
        };
        static const int num_primes = sizeof(prime_sizes) / sizeof(prime_sizes[0]);

        for (int i = 0; i < num_primes; i++) {
                if (prime_sizes[i] > h->table_size) {
                        new_size = prime_sizes[i];
                        break;
                }
        }
        if (new_size == h->table_size) {
                // If no larger prime is found, double the size
                new_size = h->table_size * 2 + 1;
        }

        MMALLOC(new_table, sizeof(tld_hash_entry*) * new_size);
        if (!new_table) return FAIL;

        // Initialize new table
        for (int i = 0; i < new_size; i++) {
                new_table[i] = NULL;
        }

        // Rehash all existing entries
        for (int i = 0; i < h->table_size; i++) {
                tld_hash_entry *entry = h->table[i];
                while (entry) {
                        tld_hash_entry *next = entry->next;
                        unsigned int new_index = fnv1a_hash(TLD_STR(entry->key), new_size);
                        entry->next = new_table[new_index];
                        new_table[new_index] = entry;
                        entry = next;
                }
        }

        // Replace old table with new table
        MFREE(h->table);
        h->table = new_table;
        h->table_size = new_size;
        h->n_rep++; // Optionally track number of resizes
        return OK;
ERROR:
        return FAIL;

}


int tld_hash_print(tld_hash *h)
{
        if(h){
                for(int i = 0; i < h->table_size; i++){
                        tld_hash_entry* entry = h->table[i];
                        while(entry){
                                fprintf(stdout,"%s: %s\n", TLD_STR(entry->key), TLD_STR(entry->value));
                                entry = entry->next;
                        }
                }
        }
        return OK;
}

void tld_hash_free(tld_hash *h)
{
        if(h){
                for(int i = 0; i < h->table_size; i++){
                        tld_hash_entry* entry = h->table[i];
                        while(entry){
                                tld_hash_entry* next = entry->next;
                                tld_hash_entry_free(entry);
                                entry = next;
                        }
                }

                MFREE(h->table);
                MFREE(h);
        }
}


int tld_hash_entry_alloc(tld_hash_entry **entry)
{
        tld_hash_entry* e = NULL;
        MMALLOC(e, sizeof(tld_hash_entry));
        e->key = NULL;
        e->value = NULL;
        RUN(tld_strbuf_alloc(&e->key, 64));
        RUN(tld_strbuf_alloc(&e->value, 64));
        e->next = NULL;
        *entry = e;
        return OK;
ERROR:
        return FAIL;
}

void tld_hash_entry_free(tld_hash_entry *n)
{
        if(n){
                tld_strbuf_free(n->key);
                tld_strbuf_free(n->value);
                MFREE(n);
        }
}

#undef LOAD_FACTOR_THRESHOLD

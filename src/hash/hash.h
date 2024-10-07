#ifndef HASH_H
#define HASH_H

#include "../core/tld-core.h"

typedef struct tld_hash tld_hash;

tld_external  int tld_hash_iter_init(tld_hash *h);
tld_external  int tld_hash_iterator_next(tld_hash* h, char **key,char **value);

tld_external  int tld_hash_add(tld_hash **hash, char *key, char *val);
tld_external  int tld_hash_get(tld_hash *h, char *key, char **val);
tld_external  int tld_hash_rename(tld_hash *h, char *key, char* new_key);
tld_external  int tld_hash_del(tld_hash *h, char *key);

tld_external  int tld_hash_alloc(tld_hash **hash, int size);
tld_external void tld_hash_free(tld_hash *h);
tld_external  int tld_hash_print(tld_hash *h);

#endif

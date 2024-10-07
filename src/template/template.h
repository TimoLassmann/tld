#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "../core/tld-core.h"

#include "../string/str.h"
typedef struct tld_hash tld_hash;

/* typedef struct tld_template_hash tld_template_hash; */

/* tld_external  int tld_template_add(tld_template_hash **hash, char *key, char *val); */
/* tld_external  int tld_template_get(tld_template_hash *h, char *key, char **val); */
/* tld_external  int tld_template_rename_var(tld_template_hash *h, char *key, char* new_key); */
/* tld_external  int tld_template_remove(tld_template_hash *h, char *key); */

/* tld_external  int tld_template_hash_alloc(tld_template_hash **hash, int size); */
/* tld_external void tld_template_hash_free(tld_template_hash *h); */
/* tld_external  int tld_template_hash_print(tld_template_hash *h); */

tld_external int tld_template_apply(tld_strbuf *txt, tld_hash *map);
tld_external int tld_template_extract_var(tld_strbuf *txt, tld_hash **hash);
#endif

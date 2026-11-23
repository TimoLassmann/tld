/* A writable pointer into a hash table: rejected with -Wall -Werror (const). */
#include "tld.h"
void f(tld_hash *h);
void f(tld_hash *h) { char *v = NULL; tld_hash_get(h, "k", &v); v[0] = 0; }

/* hash: a string-to-string hash map (FNV-1a, chaining, grows automatically). Keys and
 * values are copied in; pointers handed out are borrowed from the map. Not locked: one
 * thread at a time per map. */
#ifndef TLD_HASH_H
#define TLD_HASH_H

#include "../core/tld-core.h"

/* Opaque string-to-string map. Create with tld_hash_alloc (or let tld_hash_add create
 * it), free with tld_hash_free. */
typedef struct tld_hash tld_hash;

/* Allocate an empty map in *hash with more than size buckets (a prime); it grows when
 * the entries outnumber 3/4 of the buckets. Free with tld_hash_free. */
TLD_API  int tld_hash_alloc(tld_hash **hash, int size);

/* Free the map and every key and value; NULL is ignored. */
TLD_API void tld_hash_free(tld_hash *h);

/* Insert key -> val (both copied), replacing the value if key exists. When *hash is NULL
 * a map is allocated first and stored in *hash. */
TLD_API  int tld_hash_add(tld_hash **hash, const char *key, const char *val);

/* Look up key: *val points to the stored value (borrowed and read-only: do not write
 * to it or free it; valid until the entry changes or the map is freed). Returns FAIL, without a message, and sets *val to NULL when key
 * is absent. */
TLD_API  int tld_hash_get(tld_hash *h, const char *key, const char **val);

/* Remove key. Returns FAIL, without a message, when key is absent. */
TLD_API  int tld_hash_del(tld_hash *h, const char *key);

/* Move the value stored under key to new_key (replacing any value there). Returns FAIL
 * when key is absent. */
TLD_API  int tld_hash_rename(tld_hash *h, const char *key, const char *new_key);

/* Start iterating over the map (entries come in no particular order). */
TLD_API  int tld_hash_iter_init(tld_hash *h);

/* Next entry: *key and *value point into the map (borrowed and read-only). Returns FAIL, without a
 * message, when there are no more entries. Do not add or delete while iterating.
 *
 *   tld_hash *h = NULL;
 *   const char *k, *v;
 *   RUN(tld_hash_add(&h, "a", "1"));
 *   RUN(tld_hash_iter_init(h));
 *   while(tld_hash_iter_next(h, &k, &v) == OK){
 *           printf("%s=%s\n", k, v);
 *   }
 *   tld_hash_free(h);
 */
TLD_API  int tld_hash_iter_next(tld_hash* h, const char **key, const char **value);

#endif

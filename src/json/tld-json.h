/* json: parse JSON text into a tree (tld_json_parse), build trees with JSON_OBJ and
 * JSON_ARR, read values by path (tld_json_get_*), search and update, and write JSON
 * text (tld_json_dump). */
#ifndef TLD_JSON_H
#define TLD_JSON_H

#include <stdint.h>
#include <stdio.h>

#include "tld-json_struct.h"

#ifdef TLD_JSON_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

/* Parse the JSON text in t (standard JSON, nesting up to 512 levels) into a new object in
 * *out; free it with tld_json_obj_free. A top-level array becomes the single entry of
 * an object, under the key "Top Level Arr". Invalid JSON prints an error naming the byte
 * offset and returns FAIL. */
EXTERN  int tld_json_parse(tld_strbuf *t, tld_json_obj **out);

/* Collect deep copies of every value stored under key, at any depth, into a new object
 * in *ret (entries named key, in document order; free with tld_json_obj_free). OK with
 * an empty result when nothing matches. */
EXTERN int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret);

/* Path getters. A path is dot-separated keys with [i] for array elements,
   e.g. "message.content", "embeddings[0]", "models[2].name". tld_json_parse
   stores a top-level array under the key "Top Level Arr"; address it as
   "Top Level Arr[0]..." or simply "[0]...". Keys containing '.' or '[' cannot
   be addressed. All return OK, or FAIL (leaving *out unchanged) when the path
   does not exist or the value has another type. Strings, objects and arrays
   are borrowed from the tree. get_num accepts integers; get_int accepts
   integral doubles inside the int64_t range. JSON null has type
   TLD_JSON_UNDEF and fails every getter. An empty path in get_obj returns
   obj itself. out may be NULL to test whether the path exists. */
EXTERN int tld_json_get_str(tld_json_obj *obj, const char *path, const char **out);
EXTERN int tld_json_get_num(tld_json_obj *obj, const char *path, double *out);
EXTERN int tld_json_get_int(tld_json_obj *obj, const char *path, int64_t *out);
EXTERN int tld_json_get_bool(tld_json_obj *obj, const char *path, int *out);
EXTERN int tld_json_get_obj(tld_json_obj *obj, const char *path, tld_json_obj **out);
EXTERN int tld_json_get_arr(tld_json_obj *obj, const char *path, tld_json_arr **out);

/* Set the value at a path to a deep copy of v. key is a path in the syntax of the
 * tld_json_get_* getters ("name", "a.b", "list[2].id", "[0]" for a parsed top-level
 * array). Everything before the last step must exist. The last step may name a key
 * that is missing (it is added at the end of its object) or the index one past the
 * end of an array (v is appended). An existing value is replaced, even by one of
 * another type, and v may be part of n. Returns FAIL for a missing parent, an index
 * further past the end, or no memory. Keys containing '.' or '[' cannot be addressed. */
EXTERN int tld_json_set_val(tld_json_obj *n, char *key, tld_json_val *v);

/* Entries for JSON_OBJ / JSON_ARR. JS(k, s): the string s, with its type guessed from
 * the text: "12" becomes an integer, "0.2" a number, true/false (also True, TRUE, ...) a
 * boolean, anything else a string. JO(k, o) / JA(k, a): a nested tld_json_obj * /
 * tld_json_arr *, which the new container takes over (do not free it yourself). An entry
 * whose value is NULL is skipped. Arrays ignore the key; pass NULL. */
#define JS(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_STRING, .value = (void*)(v)})
#define JO(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_OBJ, .value = (void*)(v)})
#define JA(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_ARR, .value = (void*)(v)})

/* Typed decorations. JS above guesses the type from the text ("0.2" becomes
   a number); these do not. JSTR(k,s) is always a string, JNUM(k,d) a number
   (non-finite values are written as null), JINT(k,l) an integer (values
   outside int32_t are stored as double), JBOOL(k,b) true/false, JNULL(k) null.
   A NULL string in JSTR skips the entry, like JS. */
#define JSTR(k,v)  ((tld_json_decoration){.key =(char*)k, .type = VAL_JSTR, .value = (void*)(v)})
#define JNUM(k,v)  ((tld_json_decoration){.key =(char*)k, .type = VAL_NUM,  .value = (void*)&(double){(v)}})
#define JINT(k,v)  ((tld_json_decoration){.key =(char*)k, .type = VAL_INT,  .value = (void*)&(long){(v)}})
#define JBOOL(k,v) ((tld_json_decoration){.key =(char*)k, .type = VAL_BOOL, .value = (void*)&(int){(v)}})
#define JNULL(k)   ((tld_json_decoration){.key =(char*)k, .type = VAL_NULL, .value = (void*)""})

/* JSON_OBJ(entry, ...): a new tld_json_obj * holding the entries in order; JSON_ARR(...)
 * a new tld_json_arr *. NULL when an entry cannot be added (out of memory); the
 * containers passed in with JO/JA are then freed too. Free the result with
 * tld_json_obj_free / tld_json_arr_free.
 *
 *   tld_json_obj *o = JSON_OBJ(JSTR("name", "x"), JINT("n", 3),
 *                              JA("v", JSON_ARR(JNUM(NULL, 0.5), JNUM(NULL, 2))));
 */
#define JSON_OBJ(...) json_obj_add_multiple(NULL, (tld_json_decoration[]){__VA_ARGS__}, \
                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))
#define JSON_ARR(...) json_arr_add_multiple(NULL, (tld_json_decoration[]){__VA_ARGS__}, \
                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))

/* Back ends of JSON_OBJ / JSON_ARR: add count entries to obj / arr (allocated when NULL)
 * and return it. They stop at the first entry that cannot be added: a container created
 * by the call is then freed, with the children of entries not yet attached, and NULL is
 * returned; a caller-supplied container is returned holding the entries added so far. */
EXTERN tld_json_obj *json_obj_add_multiple(tld_json_obj *obj, tld_json_decoration *kvs, int count);
EXTERN tld_json_arr *json_arr_add_multiple(tld_json_arr *arr, tld_json_decoration *kvs, int count);

/* Append n as JSON text to b (b is not cleared): 4 spaces of indentation per level on
 * top of indent (pass 0), strings escaped, doubles with the fewest of 15 to 17 significant
 * digits that read back exactly, inf/nan as null. A NULL n appends nothing. */
EXTERN int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent);
EXTERN int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b, int indent);

/* Debug listing of the tree (not JSON) to out, or stdout when out is NULL. */
EXTERN int tld_json_obj_print(tld_json_obj *n, FILE *out);
EXTERN int tld_json_arr_print(tld_json_arr *n, FILE *out);

#undef TLD_JSON_IMPORT
#undef EXTERN

#endif

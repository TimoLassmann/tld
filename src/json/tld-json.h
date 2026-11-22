/* json: parse JSON text into a tree (tld_json_parse), build trees with TLD_JSON_OBJECT and
 * TLD_JSON_ARRAY, read values by path (tld_json_get_*), search and update, and write JSON
 * text (tld_json_dump). */
#ifndef TLD_JSON_H
#define TLD_JSON_H

#include <stdint.h>
#include <stdio.h>

#include "tld-json_struct.h"

/* Parse the NUL-terminated JSON text (RFC 8259, nesting up to 512 levels) into a new
 * object in *out; free it with tld_json_obj_free. A top-level array becomes the single
 * entry of an object, under the key "Top Level Arr" (tld_json_dump writes it back as the
 * array). Strings must be well-formed UTF-8 without raw control characters; a lone
 * surrogate escape (\ud800) becomes U+FFFD. Numbers too large for a double are an error
 * (underflow gives 0); integers outside int32_t are stored as doubles. Invalid JSON
 * prints an error naming the byte offset and returns FAIL. tld_json_parse_n parses the len bytes
 * at text instead (a NUL byte among them is invalid JSON). For a tld_strbuf b:
 * tld_json_parse(TLD_STR(b), &o). */
TLD_API int tld_json_parse(const char *text, tld_json_obj **out);
TLD_API int tld_json_parse_n(const char *text, size_t len, tld_json_obj **out);

/* Collect deep copies of every value stored under key, at any depth, into a new object
 * in *ret (entries named key, in document order; free with tld_json_obj_free). OK with
 * an empty result when nothing matches. */
TLD_API int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret);

/* Path getters. A path is dot-separated keys with [i] for array elements,
   e.g. "message.content", "embeddings[0]", "models[2].name". tld_json_parse
   stores a top-level array under the key "Top Level Arr"; address it as
   "Top Level Arr[0]..." or simply "[0]...". Keys containing '.' or '[' cannot
   be addressed. All return OK, or FAIL (leaving *out unchanged) when the path
   does not exist or the value has another type. Strings, objects and arrays
   are borrowed from the tree. get_num accepts integers; get_int accepts
   integral doubles inside the int64_t range. JSON null has type
   TLD_JSON_NULL and fails every getter. An empty path in get_obj returns
   obj itself. out may be NULL to test whether the path exists. */
TLD_API int tld_json_get_str(tld_json_obj *obj, const char *path, const char **out);
TLD_API int tld_json_get_num(tld_json_obj *obj, const char *path, double *out);
TLD_API int tld_json_get_int(tld_json_obj *obj, const char *path, int64_t *out);
TLD_API int tld_json_get_bool(tld_json_obj *obj, const char *path, int *out);
TLD_API int tld_json_get_obj(tld_json_obj *obj, const char *path, tld_json_obj **out);
TLD_API int tld_json_get_arr(tld_json_obj *obj, const char *path, tld_json_arr **out);

/* Set the value at a path to a deep copy of v. key is a path in the syntax of the
 * tld_json_get_* getters ("name", "a.b", "list[2].id", "[0]" for a parsed top-level
 * array). Everything before the last step must exist. The last step may name a key
 * that is missing (it is added at the end of its object) or the index one past the
 * end of an array (v is appended). An existing value is replaced, even by one of
 * another type, and v may be part of n. Returns FAIL for a missing parent, an index
 * further past the end, or no memory. Keys containing '.' or '[' cannot be addressed. */
TLD_API int tld_json_set_val(tld_json_obj *n, const char *path, tld_json_val *v);

/* Entries for TLD_JSON_OBJECT / TLD_JSON_ARRAY. TLD_JAUTO(k, s): the string s, with its type guessed from
 * the text: "12" becomes an integer, "0.2" a number, true/false (also True, TRUE, ...) a
 * boolean, anything else a string. TLD_JOBJ(k, o) / TLD_JARR(k, a): a nested tld_json_obj * /
 * tld_json_arr *, which the new container takes over (do not free it yourself), also
 * when the build fails. A TLD_JAUTO or TLD_JSTR entry whose string is NULL is skipped; a TLD_JOBJ or TLD_JARR
 * entry whose value is NULL makes the build fail, since a nested TLD_JSON_OBJECT / TLD_JSON_ARRAY
 * returns NULL only when it failed. Arrays ignore the key; pass NULL. */
#define TLD_JAUTO(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_AUTO, .value = (void*)(v)})
#define TLD_JOBJ(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_OBJ, .value = (void*)(v)})
#define TLD_JARR(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_ARR, .value = (void*)(v)})

/* Typed decorations. TLD_JAUTO above guesses the type from the text ("0.2" becomes
   a number); these do not. TLD_JSTR(k,s) is always a string, TLD_JNUM(k,d) a number
   (non-finite values are written as null), TLD_JINT(k,l) an integer (values
   outside int32_t are stored as double), TLD_JBOOL(k,b) true/false, TLD_JNULL(k) null.
   A NULL string in TLD_JSTR skips the entry, like TLD_JAUTO. */
#define TLD_JSTR(k,v)  ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_STR, .value = (void*)(v)})
#define TLD_JNUM(k,v)  ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_NUM,  .value = (void*)&(double){(v)}})
#define TLD_JINT(k,v)  ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_INT,  .value = (void*)&(long){(v)}})
#define TLD_JBOOL(k,v) ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_BOOL, .value = (void*)&(int){(v)}})
#define TLD_JNULL(k)   ((tld_json_decoration){.key =(char*)k, .type = TLD__JVAL_NULL, .value = (void*)""})

/* TLD_JSON_OBJECT(entry, ...): a new tld_json_obj * holding the entries in order; TLD_JSON_ARRAY(...)
 * a new tld_json_arr *. NULL (with an error printed) when an entry cannot be added: out
 * of memory, or a TLD_JOBJ/TLD_JARR entry holding NULL. The containers passed in with TLD_JOBJ/TLD_JARR are then
 * freed too. Free the result with tld_json_obj_free / tld_json_arr_free.
 *
 *   tld_json_obj *o = TLD_JSON_OBJECT(TLD_JSTR("name", "x"), TLD_JINT("n", 3),
 *                              TLD_JARR("v", TLD_JSON_ARRAY(TLD_JNUM(NULL, 0.5), TLD_JNUM(NULL, 2))));
 */
#define TLD_JSON_OBJECT(...) tld__json_obj_build(NULL, (tld_json_decoration[]){__VA_ARGS__}, \
                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))
#define TLD_JSON_ARRAY(...) tld__json_arr_build(NULL, (tld_json_decoration[]){__VA_ARGS__}, \
                                            sizeof((tld_json_decoration[]){__VA_ARGS__})/sizeof(tld_json_decoration))

/* Back ends of TLD_JSON_OBJECT / TLD_JSON_ARRAY: add count entries to obj / arr (allocated when NULL)
 * and return it. They stop at the first entry that cannot be added and return NULL: the
 * TLD_JOBJ/TLD_JARR children not yet attached are freed, as is a container created by the call; a
 * caller-supplied container keeps the entries added so far and stays the caller's. */
TLD_API tld_json_obj *tld__json_obj_build(tld_json_obj *obj, tld_json_decoration *kvs, int count);
TLD_API tld_json_arr *tld__json_arr_build(tld_json_arr *arr, tld_json_decoration *kvs, int count);

/* Add one entry to *obj / one value to *arr, creating it when *obj / *arr is NULL:
 *
 *   tld_json_obj *req = NULL;
 *   RUN(tld_json_obj_add(&req, TLD_JSTR("model", name)));
 *   RUN(tld_json_obj_add(&req, TLD_JBOOL("stream", 0)));
 *
 * Entries are those of TLD_JSON_OBJECT: a TLD_JOBJ/TLD_JARR value passes to the container,
 * also on failure (it is then freed), and a TLD_JAUTO/TLD_JSTR entry whose string is NULL
 * is skipped (nothing is created). FAIL with an error when out of memory or for a
 * TLD_JOBJ/TLD_JARR holding NULL; the container keeps what it had (a new one is freed,
 * leaving *obj / *arr NULL). */
TLD_API int tld_json_obj_add(tld_json_obj **obj, tld_json_decoration entry);
TLD_API int tld_json_arr_add(tld_json_arr **arr, tld_json_decoration entry);

/* The indent for compact JSON: tld_json_dump(n, b, TLD_JSON_COMPACT) writes no spaces or
 * newlines outside strings. */
#define TLD_JSON_COMPACT (-1)

/* Append n as JSON text to b (b is not cleared): with indent >= 0, 4 spaces of
 * indentation per level on top of indent (pass 0); with TLD_JSON_COMPACT, no whitespace.
 * Strings are escaped (a byte that is not well-formed UTF-8 is written as \ufffd, so the
 * text is always valid JSON), doubles written with the fewest of 15 to 17 significant
 * digits that read back exactly, inf/nan as null. A parsed top-level array is written
 * as the array (see top_level_arr). A NULL n appends nothing. */
TLD_API int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent);
TLD_API int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b, int indent);

#endif

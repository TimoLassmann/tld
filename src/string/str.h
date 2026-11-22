/* string: tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy),
 * and tld_str, a pointer-and-length string with prefix and suffix tests. Inspired by
 * the rayfork str code by Sas Luca. */
#ifndef STR_H
#define STR_H

#include "../core/tld-core.h"
#include <stdint.h>

/* A string as pointer and length: len bytes at str. Owned when made by tld_strmk or
 * tld_str_alloc (free with tld_str_free); a borrowed view when made by
 * tld_strbuf_to_str or tld_char_to_str (never free). */
typedef struct tld_string {
        uint8_t* str;
        int32_t len;
} tld_str;

/* A growable byte buffer: len bytes at str, always followed by a NUL, in alloc_len
 * bytes. TLD_STR(b) gives it as char *. Appending may move str, so do not keep that
 * pointer across changes. Create with tld_strbuf_alloc, free with tld_strbuf_free. */
typedef struct tld_string_buffer {
        uint8_t* str;
        int32_t len;
        int32_t alloc_len;
} tld_strbuf;

/* TLD_STR(x): the contents of a tld_strbuf * or tld_str * as char *; TLD_STRLEN(x): its
 * length in bytes. */
#define TLD_STR(X) ((char*)(X)->str)
#define TLD_STRLEN(X) (X)->len

/* Empty the buffer (len 0); the memory is kept. */
tld_external int tld_strbuf_clear(tld_strbuf *b);

/* Replace the contents of t with a copy of s; t grows as needed. */
tld_external int tld_strbuf_copy(tld_strbuf *t, tld_strbuf *s);

/* Append the NUL-terminated string content to b; b grows as needed. */
tld_external int tld_append(tld_strbuf *b, char *content);

/* Append the single character c to b. */
tld_external int tld_append_char(tld_strbuf *b, char c);

/* Insert the NUL-terminated string content at the start of b. */
tld_external int tld_prepend(tld_strbuf *b, char *content);

/* 1 if t ends with s (tld_suffix_match) or starts with s (tld_prefix_match), else 0.
 * A boolean, not OK/FAIL. */
tld_external int tld_suffix_match(tld_str t, tld_str s);
tld_external int tld_prefix_match(tld_str t, tld_str s);

/* A borrowed tld_str view of the buffer's current contents: tld_strbuf_to_str(*b). */
tld_external tld_str tld_strbuf_to_str(tld_strbuf s);

/* A borrowed tld_str view of the C string s (len = strlen(s)). */
tld_external tld_str tld_char_to_str(char* s);

/* A new tld_str in *string holding a NUL-terminated copy of src (len = strlen(src)).
 * Free with tld_str_free. */
tld_external int tld_strmk(tld_str **string,const char *src);

/* A new empty buffer in *buffer with size bytes reserved (0 means 1024; size < 0 is an
 * error). It grows as needed. Free with tld_strbuf_free. */
tld_external int tld_strbuf_alloc(tld_strbuf **buffer, int size);

/* Free a buffer; NULL is ignored. */
tld_external void tld_strbuf_free(tld_strbuf *b);

/* A new tld_str in *string with a zeroed buffer of size bytes and len = size. Free
 * with tld_str_free. */
tld_external int tld_str_alloc(tld_str **string, int size);

/* Free a tld_str made by tld_strmk or tld_str_alloc; NULL is ignored. */
tld_external void tld_str_free(tld_str *a);

#undef STR_IMPORT
#undef EXTERN


#endif

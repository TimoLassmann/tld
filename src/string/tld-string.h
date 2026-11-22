/* string: tld_strbuf, a growable NUL-terminated byte buffer (append, prepend, copy).
 * Inspired by the rayfork str code by Sas Luca. */
#ifndef TLD_STRING_H
#define TLD_STRING_H

#include "../core/tld-core.h"
#include <stdint.h>

/* A growable byte buffer: len bytes at str, always followed by a NUL, in alloc_len
 * bytes. TLD_STR(b) gives it as char *. Appending may move str, so do not keep that
 * pointer across changes. Create with tld_strbuf_alloc, free with tld_strbuf_free.
 * A buffer holds at most INT32_MAX - 1 bytes; an append that would pass that fails
 * with an error and leaves the buffer unchanged. */
typedef struct tld_strbuf {
        uint8_t* str;
        int32_t len;
        int32_t alloc_len;
} tld_strbuf;

/* TLD_STR(b): the contents of the tld_strbuf * b as char *; TLD_STRLEN(b): its length in
 * bytes. */
#define TLD_STR(X) ((char*)(X)->str)
#define TLD_STRLEN(X) (X)->len

/* A new empty buffer in *buffer with size bytes reserved (0 means 1024; size < 0 is an
 * error). It grows as needed. Free with tld_strbuf_free. */
TLD_API int tld_strbuf_alloc(tld_strbuf **buffer, int size);

/* Free a buffer; NULL is ignored. */
TLD_API void tld_strbuf_free(tld_strbuf *b);

/* Empty the buffer (len 0); the memory is kept. */
TLD_API int tld_strbuf_clear(tld_strbuf *b);

/* Replace the contents of t with a copy of s; t grows as needed. */
TLD_API int tld_strbuf_copy(tld_strbuf *t, tld_strbuf *s);

/* Append the NUL-terminated string content to b; b grows as needed. */
TLD_API int tld_strbuf_append(tld_strbuf *b, const char *content);

/* Append the single character c to b. */
TLD_API int tld_strbuf_append_char(tld_strbuf *b, char c);

/* Insert the NUL-terminated string content at the start of b. */
TLD_API int tld_strbuf_prepend(tld_strbuf *b, const char *content);

#endif

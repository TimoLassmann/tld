#include "str.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

tld_internal int tld_strbuf_resize(tld_strbuf *b, int new_size);
tld_internal int tld_strbuf_ensure_size(tld_strbuf *b, size_t size);
tld_internal ptrdiff_t tld__strbuf_offset(const tld_strbuf *b, const char *p);

int tld_strbuf_alloc(tld_strbuf **buffer, int size)
{
        tld_strbuf* b = NULL;
        ASSERT(size >= 0, "Negative buffer size: %d", size);
        MMALLOC(b,sizeof(tld_strbuf));
        if(size == 0){
                size = 1024;
        }
        b->alloc_len = size;
        b->len = 0;
        b->str = NULL;
        galloc(&b->str, b->alloc_len);
        b->str[0] = 0;

        *buffer = b;
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

void tld_strbuf_free(tld_strbuf *b)
{
        if(b){
                if(b->str){
                        gfree(b->str);
                }
                MFREE(b);
        }
}

int tld_str_alloc(tld_str **string, int size)
{
        tld_str* a = NULL;
        MMALLOC(a, sizeof(tld_str));
        a->len = size;
        a->str = NULL;
        galloc(&a->str, a->len);
        *string = a;
        return OK;
ERROR:
        tld_str_free(a);
        return FAIL;
}

void tld_str_free(tld_str *a)
{
        if(a){
                if(a->str){
                        gfree(a->str);
                }
                MFREE(a);
        }
}

int tld_strmk(tld_str **string,const char *src)
{
        size_t size = strlen(src);
        tld_str* a = NULL;
        ASSERT(size < (size_t)INT32_MAX, "String of %zu bytes is too long", size);
        /* buffer holds the characters plus the terminating NUL; len excludes the NUL */
        RUN(tld_str_alloc(&a,(int)size + 1));
        memcpy(a->str,src, size + 1);
        a->len = (int32_t)size;

        *string = a;
        return OK;
ERROR:
        return FAIL;
}

int tld_strbuf_clear(tld_strbuf *b)
{
        ASSERT(b != NULL,"No buffer");
        b->len = 0;
        b->str[b->len] = 0;

        return OK;
ERROR:
        return FAIL;
}

int tld_strbuf_resize(tld_strbuf *b, int new_size)
{
        uint8_t* tmp = NULL;
        if (new_size > b->alloc_len){
                galloc(&tmp , new_size);

                memcpy(tmp,b->str,b->len);
                tmp[b->len] = 0;

                gfree(b->str);
                b->str = tmp;
                b->alloc_len = new_size;
        }
        return OK;
ERROR:
        if(tmp){
                gfree(tmp);
        }
        return FAIL;
}

/* Make room for size more bytes and the NUL. Sizes are size_t so nothing wraps; a
   buffer cannot grow past INT32_MAX bytes (len and alloc_len are int32_t). */
int tld_strbuf_ensure_size(tld_strbuf *b, size_t size)
{
        ASSERT(b->len >= 0 && size <= (size_t)INT32_MAX - (size_t)b->len - 1,
               "String buffer would exceed %d bytes", INT32_MAX);
        size_t need = (size_t)b->len + size + 1;
        if(need > (size_t)b->alloc_len){
                size_t grow = (size_t)b->alloc_len * 2;
                if(grow < need){
                        grow = need;
                }
                if(grow > (size_t)INT32_MAX){
                        grow = (size_t)INT32_MAX;
                }
                RUN(tld_strbuf_resize(b, (int)grow));
        }
        return OK;
ERROR:
        return FAIL;
}

/* The offset of p in b's bytes, or -1 when p points elsewhere. Text passed to an
   append can come from the buffer itself (TLD_STR(b)), and growing the buffer moves
   it, so callers re-derive the pointer from the offset after growing. */
ptrdiff_t tld__strbuf_offset(const tld_strbuf *b, const char *p)
{
        uintptr_t start = (uintptr_t) b->str;
        uintptr_t q = (uintptr_t) p;
        if(q >= start && q < start + (uintptr_t) b->alloc_len){
                return (ptrdiff_t) (q - start);
        }
        return -1;
}

int tld_strbuf_copy(tld_strbuf *t, tld_strbuf *s)
{
        if(t == s){
                return OK;
        }
        /* room for s->len bytes in total; t is unchanged if this fails */
        RUN(tld_strbuf_ensure_size(t, s->len > t->len ? (size_t)(s->len - t->len) : 0));

        memcpy(t->str, s->str, s->len);
        t->len = s->len;
        t->str[t->len] = 0;
        return OK;
ERROR:
        return FAIL;
}

int tld_append(tld_strbuf *b, const char *content)
{
        size_t size = strlen(content);
        ptrdiff_t off = tld__strbuf_offset(b, content);

        RUN(tld_strbuf_ensure_size(b, size));
        if(off >= 0){
                content = TLD_STR(b) + off;
        }
        memmove(b->str + b->len, content, size);
        b->len += (int32_t)size;
        b->str[b->len] = 0;

        return OK;
ERROR:
        return FAIL;
}

int tld_append_char(tld_strbuf *b, char c)
{
        RUN(tld_strbuf_ensure_size(b, 1));
        b->str[b->len] = c;
        b->len++;
        b->str[b->len] = 0;

        return OK;
ERROR:
        return FAIL;
}

int tld_prepend(tld_strbuf *b, const char *content)
{
        size_t size = strlen(content);
        ptrdiff_t off = tld__strbuf_offset(b, content);

        RUN(tld_strbuf_ensure_size(b, size));
        memmove(b->str + size, b->str, b->len);
        if(off >= 0){
                /* the text came from b and has just moved size bytes right */
                content = TLD_STR(b) + off + size;
        }
        memmove(b->str, content, size);
        b->len += (int32_t)size;
        b->str[b->len] = 0;
        return OK;
ERROR:
        return FAIL;
}

int tld_suffix_match(tld_str t, tld_str s)
{

        if (t.len < s.len) return 0;
        int offset = t.len - s.len;
        int cmp = memcmp(t.str + offset, s.str, s.len);
        return cmp == 0;
}

int tld_prefix_match(tld_str t, tld_str s)
{
        if (t.len < s.len) return 0;
        int cmp = memcmp(t.str, s.str, s.len);
        return cmp == 0;
}

tld_str tld_strbuf_to_str(tld_strbuf s)
{
    tld_str r = {0};
    r.str = s.str;
    r.len = s.len;
    return r;
}

tld_str tld_char_to_str(char* s)
{
        int size = strlen(s);
        tld_str r = {
                .str = (uint8_t*) s,
                .len = size,
        };
        return r;
}

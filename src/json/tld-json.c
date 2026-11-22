#include "../alloc/tld-alloc.h"
#include "../string/tld-string.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tld-json_struct.h"
#include "tld-json_mem.h"
#include "tld-json_misc.h"

#include "tld-json.h"

/* Maximum nesting depth of objects / arrays accepted by the parser. */
#define TLD__JSON_MAX_DEPTH 512

/* Key under which tld_json_parse stores a top-level array. */
#define TLD__JSON_TOP_LEVEL_ARR "Top Level Arr"

typedef struct tld__json_parser {
        const uint8_t* s;
        int len;
        int pos;
        int depth;
        /* scratch buffer for decoded strings */
        char* buf;
        int b_len;
        int b_alloc;
        const char* err;
} tld__json_parser;

static int tld__json_dump_val(tld_json_val *val, tld_strbuf *b);
static int tld__json_dump_any(tld_json_val *v, tld_strbuf *b, int indent);
static int tld__json_dump_str(tld_strbuf *b, const uint8_t *s, int len);
static int tld__json_add_indent(tld_strbuf *b, int indent);
static int tld__json_get_arr_str(tld_json_arr *n, char *key, tld_json_obj *res);

static int tld__json_add_val(char *buf, int len, tld_json_val *val);
static int tld__json_val_set_str(tld_json_val *v, const char *s, int len);
static void tld__json_val_set_int64(tld_json_val *v, long long x);
static int tld__json_val_from_decoration(tld_json_val *v, tld_json_decoration *d);

static int tld__json_search(tld_json_obj *n, char *key, tld_json_obj* r);

/* parser */
static int tld__jp_fail(tld__json_parser *p, const char *msg);
static void tld__jp_ws(tld__json_parser *p);
static int tld__jp_push(tld__json_parser *p, uint8_t c);
static int tld__jp_push_utf8(tld__json_parser *p, uint32_t cp);
static int tld__jp_hex4(tld__json_parser *p, uint32_t *cp);
static int tld__jp_string(tld__json_parser *p);
static int tld__jp_utf8_len(const uint8_t *s, int avail);
static int tld__jp_number(tld__json_parser *p, tld_json_val *v);
static int tld__jp_literal(tld__json_parser *p, const char *lit);
static int tld__jp_value(tld__json_parser *p, tld_json_val *v);
static int tld__jp_obj(tld__json_parser *p, tld_json_obj **out);
static int tld__jp_arr(tld__json_parser *p, tld_json_arr **out);

/* path getters */
static int tld__json_path_resolve(tld_json_obj *root, const char *path, tld_json_val **out);

static tld_json_obj *tld__json_obj_add_entry(tld_json_obj *obj, tld_json_decoration *d);
static tld_json_arr *tld__json_arr_add_entry(tld_json_arr *arr, tld_json_decoration *d);

/* ------------------------------------------------------------------ */
/* Parser: recursive descent directly over the input text.             */
/* ------------------------------------------------------------------ */

int tld__jp_fail(tld__json_parser *p, const char *msg)
{
        if(p->err == NULL){
                p->err = msg;
        }
        return FAIL;
}

void tld__jp_ws(tld__json_parser *p)
{
        while(p->pos < p->len){
                uint8_t c = p->s[p->pos];
                if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
                        p->pos++;
                }else{
                        break;
                }
        }
}

/* Append one byte to the scratch buffer, always leaving room for a
   terminating 0. */
int tld__jp_push(tld__json_parser *p, uint8_t c)
{
        if(p->b_len + 1 >= p->b_alloc){
                int new_alloc = p->b_alloc + p->b_alloc / 2 + 16;
                MREALLOC(p->buf, new_alloc);
                p->b_alloc = new_alloc;
        }
        p->buf[p->b_len] = (char)c;
        p->b_len++;
        return OK;
ERROR:
        return FAIL;
}

int tld__jp_push_utf8(tld__json_parser *p, uint32_t cp)
{
        if(cp < 0x80){
                RUN(tld__jp_push(p, (uint8_t)cp));
        }else if(cp < 0x800){
                RUN(tld__jp_push(p, (uint8_t)(0xC0 | (cp >> 6))));
                RUN(tld__jp_push(p, (uint8_t)(0x80 | (cp & 0x3F))));
        }else if(cp < 0x10000){
                RUN(tld__jp_push(p, (uint8_t)(0xE0 | (cp >> 12))));
                RUN(tld__jp_push(p, (uint8_t)(0x80 | ((cp >> 6) & 0x3F))));
                RUN(tld__jp_push(p, (uint8_t)(0x80 | (cp & 0x3F))));
        }else{
                RUN(tld__jp_push(p, (uint8_t)(0xF0 | (cp >> 18))));
                RUN(tld__jp_push(p, (uint8_t)(0x80 | ((cp >> 12) & 0x3F))));
                RUN(tld__jp_push(p, (uint8_t)(0x80 | ((cp >> 6) & 0x3F))));
                RUN(tld__jp_push(p, (uint8_t)(0x80 | (cp & 0x3F))));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld__jp_hex4(tld__json_parser *p, uint32_t *cp)
{
        uint32_t v = 0;
        if(p->pos + 4 > p->len){
                return tld__jp_fail(p, "truncated \\u escape");
        }
        for(int i = 0; i < 4; i++){
                uint8_t c = p->s[p->pos + i];
                v <<= 4;
                if(c >= '0' && c <= '9'){
                        v |= (uint32_t)(c - '0');
                }else if(c >= 'a' && c <= 'f'){
                        v |= (uint32_t)(c - 'a' + 10);
                }else if(c >= 'A' && c <= 'F'){
                        v |= (uint32_t)(c - 'A' + 10);
                }else{
                        return tld__jp_fail(p, "invalid hex digit in \\u escape");
                }
        }
        p->pos += 4;
        *cp = v;
        return OK;
}

/* The length (2 to 4) of the well-formed UTF-8 sequence at s, which starts with a byte
   >= 0x80, or 0 when it is not one (RFC 3629: no overlong forms, no surrogates, nothing
   above U+10FFFF, no truncated sequence). avail bytes are readable. */
int tld__jp_utf8_len(const uint8_t *s, int avail)
{
        const uint8_t c = s[0];
        int n = 0;
        uint8_t lo = 0x80;
        uint8_t hi = 0xBF;
        if(c >= 0xC2 && c <= 0xDF){
                n = 2;
        }else if(c >= 0xE0 && c <= 0xEF){
                n = 3;
                if(c == 0xE0){
                        lo = 0xA0;              /* overlong below U+0800 */
                }else if(c == 0xED){
                        hi = 0x9F;              /* surrogates U+D800..U+DFFF */
                }
        }else if(c >= 0xF0 && c <= 0xF4){
                n = 4;
                if(c == 0xF0){
                        lo = 0x90;              /* overlong below U+10000 */
                }else if(c == 0xF4){
                        hi = 0x8F;              /* above U+10FFFF */
                }
        }else{
                return 0;                       /* 0x80..0xC1, 0xF5..0xFF */
        }
        if(avail < n || s[1] < lo || s[1] > hi){
                return 0;
        }
        for(int k = 2; k < n; k++){
                if(s[k] < 0x80 || s[k] > 0xBF){
                        return 0;
                }
        }
        return n;
}

/* Parse a quoted string starting at p->s[p->pos] == '"'. The decoded UTF-8
   bytes end up in p->buf (p->b_len bytes, 0-terminated). */
int tld__jp_string(tld__json_parser *p)
{
        p->pos++;               /* opening quote */
        p->b_len = 0;
        while(1){
                uint8_t c;
                if(p->pos >= p->len){
                        return tld__jp_fail(p, "unterminated string");
                }
                c = p->s[p->pos];
                p->pos++;
                if(c == '"'){
                        break;
                }else if(c == '\\'){
                        uint8_t e;
                        if(p->pos >= p->len){
                                return tld__jp_fail(p, "unterminated string");
                        }
                        e = p->s[p->pos];
                        p->pos++;
                        switch(e){
                        case '"':  if(tld__jp_push(p, '"'))  return FAIL; break;
                        case '\\': if(tld__jp_push(p, '\\')) return FAIL; break;
                        case '/':  if(tld__jp_push(p, '/'))  return FAIL; break;
                        case 'b':  if(tld__jp_push(p, '\b')) return FAIL; break;
                        case 'f':  if(tld__jp_push(p, '\f')) return FAIL; break;
                        case 'n':  if(tld__jp_push(p, '\n')) return FAIL; break;
                        case 'r':  if(tld__jp_push(p, '\r')) return FAIL; break;
                        case 't':  if(tld__jp_push(p, '\t')) return FAIL; break;
                        case 'u':{
                                uint32_t cp = 0;
                                if(tld__jp_hex4(p, &cp) != OK){
                                        return FAIL;
                                }
                                if(cp >= 0xD800 && cp <= 0xDBFF){
                                        /* high surrogate: needs a following low surrogate */
                                        uint32_t lo = 0;
                                        if(p->pos + 6 <= p->len &&
                                           p->s[p->pos] == '\\' &&
                                           p->s[p->pos+1] == 'u'){
                                                int save = p->pos;
                                                p->pos += 2;
                                                if(tld__jp_hex4(p, &lo) != OK){
                                                        return FAIL;
                                                }
                                                if(lo >= 0xDC00 && lo <= 0xDFFF){
                                                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                                }else{
                                                        /* not a pair: emit replacement, re-read the next escape */
                                                        p->pos = save;
                                                        cp = 0xFFFD;
                                                }
                                        }else{
                                                cp = 0xFFFD;
                                        }
                                }else if(cp >= 0xDC00 && cp <= 0xDFFF){
                                        /* lone low surrogate */
                                        cp = 0xFFFD;
                                }
                                if(cp == 0){
                                        return tld__jp_fail(p, "\\u0000 is not supported in strings");
                                }
                                if(tld__jp_push_utf8(p, cp) != OK){
                                        return FAIL;
                                }
                                break;
                        }
                        default:
                                return tld__jp_fail(p, "invalid escape sequence");
                        }
                }else if(c == 0){
                        return tld__jp_fail(p, "NUL byte in string");
                }else if(c < 0x20){
                        return tld__jp_fail(p, "control character in string (escape it, e.g. \\n or \\u0001)");
                }else if(c < 0x80){
                        if(tld__jp_push(p, c) != OK){
                                return FAIL;
                        }
                }else{
                        /* a UTF-8 sequence: copied through once it is well formed */
                        int n = tld__jp_utf8_len(p->s + p->pos - 1, p->len - (p->pos - 1));
                        if(n == 0){
                                p->pos--;
                                return tld__jp_fail(p, "invalid UTF-8 in string");
                        }
                        for(int k = 0; k < n; k++){
                                if(tld__jp_push(p, p->s[p->pos - 1 + k]) != OK){
                                        return FAIL;
                                }
                        }
                        p->pos += n - 1;
                }
        }
        p->buf[p->b_len] = 0;
        return OK;
}

int tld__jp_number(tld__json_parser *p, tld_json_val *v)
{
        char tmp[64];
        char* num = tmp;
        char* heap = NULL;
        int start = p->pos;
        int is_int = 1;
        int len;

        if(p->pos < p->len && p->s[p->pos] == '-'){
                p->pos++;
        }
        if(p->pos < p->len && p->s[p->pos] == '0'){
                p->pos++;
        }else if(p->pos < p->len && p->s[p->pos] >= '1' && p->s[p->pos] <= '9'){
                while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9'){
                        p->pos++;
                }
        }else{
                return tld__jp_fail(p, "invalid number");
        }
        if(p->pos < p->len && p->s[p->pos] == '.'){
                int d = 0;
                is_int = 0;
                p->pos++;
                while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9'){
                        p->pos++;
                        d++;
                }
                if(!d){
                        return tld__jp_fail(p, "invalid number: no digits after '.'");
                }
        }
        if(p->pos < p->len && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E')){
                int d = 0;
                is_int = 0;
                p->pos++;
                if(p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')){
                        p->pos++;
                }
                while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9'){
                        p->pos++;
                        d++;
                }
                if(!d){
                        return tld__jp_fail(p, "invalid number: no digits in exponent");
                }
        }

        len = p->pos - start;
        if(len >= (int)sizeof(tmp)){
                MMALLOC(heap, len + 1);
                num = heap;
        }
        memcpy(num, p->s + start, len);
        num[len] = 0;

        if(is_int){
                long long x;
                errno = 0;
                x = strtoll(num, NULL, 10);
                if(errno == ERANGE){
                        v->type = TLD_JSON_DBL;
                        v->value.d_num = strtod(num, NULL);
                }else{
                        tld__json_val_set_int64(v, x);
                }
        }else{
                v->type = TLD_JSON_DBL;
                v->value.d_num = strtod(num, NULL);
        }
        if(heap){
                MFREE(heap);
        }
        if(v->type == TLD_JSON_DBL && isinf(v->value.d_num)){
                /* a value a double cannot hold; underflow to 0 is accepted */
                p->pos = start;
                return tld__jp_fail(p, "number out of range");
        }
        return OK;
ERROR:
        return tld__jp_fail(p, "out of memory");
}

int tld__jp_literal(tld__json_parser *p, const char *lit)
{
        int l = (int)strlen(lit);
        if(p->pos + l > p->len || memcmp(p->s + p->pos, lit, l) != 0){
                return tld__jp_fail(p, "invalid literal");
        }
        p->pos += l;
        return OK;
}

/* Parse one value into the empty slot v. On failure v is left untouched
   (TLD_JSON_NULL) and anything allocated on the way is freed. */
int tld__jp_value(tld__json_parser *p, tld_json_val *v)
{
        uint8_t c;
        tld__jp_ws(p);
        if(p->pos >= p->len){
                return tld__jp_fail(p, "unexpected end of input");
        }
        c = p->s[p->pos];
        switch(c){
        case '{':{
                tld_json_obj* o = NULL;
                if(tld__jp_obj(p, &o) != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_OBJ;
                v->value.obj = o;
                break;
        }
        case '[':{
                tld_json_arr* a = NULL;
                if(tld__jp_arr(p, &a) != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_ARR;
                v->value.arr = a;
                break;
        }
        case '"':
                if(tld__jp_string(p) != OK){
                        return FAIL;
                }
                if(tld__json_val_set_str(v, p->buf, p->b_len) != OK){
                        return tld__jp_fail(p, "out of memory");
                }
                break;
        case 't':
                if(tld__jp_literal(p, "true") != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_BOOL_TRUE;
                v->value.b_num = 1;
                break;
        case 'f':
                if(tld__jp_literal(p, "false") != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_BOOL_FALSE;
                v->value.b_num = 0;
                break;
        case 'n':
                if(tld__jp_literal(p, "null") != OK){
                        return FAIL;
                }
                v->type = TLD_JSON_NULL;
                break;
        default:
                if(c == '-' || (c >= '0' && c <= '9')){
                        return tld__jp_number(p, v);
                }
                return tld__jp_fail(p, "unexpected character");
        }
        return OK;
}

int tld__jp_obj(tld__json_parser *p, tld_json_obj **out)
{
        tld_json_obj* n = NULL;

        if(p->depth >= TLD__JSON_MAX_DEPTH){
                return tld__jp_fail(p, "nesting too deep");
        }
        p->depth++;
        p->pos++;               /* '{' */

        if(tld_json_obj_alloc(&n) != OK){
                tld__jp_fail(p, "out of memory");
                goto FAIL_OBJ;
        }
        tld__jp_ws(p);
        if(p->pos < p->len && p->s[p->pos] == '}'){
                p->pos++;
                goto DONE;
        }
        while(1){
                tld__jp_ws(p);
                if(p->pos >= p->len){
                        tld__jp_fail(p, "unexpected end of input in object");
                        goto FAIL_OBJ;
                }
                if(p->s[p->pos] != '"'){
                        tld__jp_fail(p, "expected string as object key");
                        goto FAIL_OBJ;
                }
                if(tld__jp_string(p) != OK){
                        goto FAIL_OBJ;
                }
                if(tld_strbuf_append(n->key[n->n], p->buf) != OK){
                        tld__jp_fail(p, "out of memory");
                        goto FAIL_OBJ;
                }
                tld__jp_ws(p);
                if(p->pos >= p->len || p->s[p->pos] != ':'){
                        tld__jp_fail(p, "expected ':'");
                        goto FAIL_OBJ;
                }
                p->pos++;
                if(tld__jp_value(p, n->v[n->n]) != OK){
                        goto FAIL_OBJ;
                }
                n->n++;
                if(tld__json_obj_chk_space(n) != OK){
                        tld__jp_fail(p, "out of memory");
                        goto FAIL_OBJ;
                }
                tld__jp_ws(p);
                if(p->pos >= p->len){
                        tld__jp_fail(p, "unexpected end of input in object");
                        goto FAIL_OBJ;
                }
                if(p->s[p->pos] == ','){
                        p->pos++;
                }else if(p->s[p->pos] == '}'){
                        p->pos++;
                        break;
                }else{
                        tld__jp_fail(p, "expected ',' or '}'");
                        goto FAIL_OBJ;
                }
        }
DONE:
        p->depth--;
        *out = n;
        return OK;
FAIL_OBJ:
        p->depth--;
        if(n){
                tld_json_obj_free(n);
        }
        return FAIL;
}

int tld__jp_arr(tld__json_parser *p, tld_json_arr **out)
{
        tld_json_arr* n = NULL;

        if(p->depth >= TLD__JSON_MAX_DEPTH){
                return tld__jp_fail(p, "nesting too deep");
        }
        p->depth++;
        p->pos++;               /* '[' */

        if(tld_json_arr_alloc(&n) != OK){
                tld__jp_fail(p, "out of memory");
                goto FAIL_ARR;
        }
        tld__jp_ws(p);
        if(p->pos < p->len && p->s[p->pos] == ']'){
                p->pos++;
                goto DONE;
        }
        while(1){
                if(tld__jp_value(p, n->v[n->n]) != OK){
                        goto FAIL_ARR;
                }
                n->n++;
                if(tld__json_arr_chk_space(n) != OK){
                        tld__jp_fail(p, "out of memory");
                        goto FAIL_ARR;
                }
                tld__jp_ws(p);
                if(p->pos >= p->len){
                        tld__jp_fail(p, "unexpected end of input in array");
                        goto FAIL_ARR;
                }
                if(p->s[p->pos] == ','){
                        p->pos++;
                }else if(p->s[p->pos] == ']'){
                        p->pos++;
                        break;
                }else{
                        tld__jp_fail(p, "expected ',' or ']'");
                        goto FAIL_ARR;
                }
        }
DONE:
        p->depth--;
        *out = n;
        return OK;
FAIL_ARR:
        p->depth--;
        if(n){
                tld_json_arr_free(n);
        }
        return FAIL;
}

/* ------------------------------------------------------------------ */
/* Value helpers                                                       */
/* ------------------------------------------------------------------ */

/* Store len bytes of s as a string in the empty slot v. */
int tld__json_val_set_str(tld_json_val *v, const char *s, int len)
{
        tld_strbuf* b = NULL;
        RUN(tld_strbuf_alloc(&b, len + 1));
        RUN(tld_strbuf_append_n(b, s, (size_t) len));
        v->type = TLD_JSON_STR;
        v->value.str = b;
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

/* Integers that fit in int32_t are stored as TLD_JSON_INT, larger ones as
   TLD_JSON_DBL (the value layout only has an int32_t). */
void tld__json_val_set_int64(tld_json_val *v, long long x)
{
        if(x >= INT32_MIN && x <= INT32_MAX){
                v->type = TLD_JSON_INT;
                v->value.i_num = (int32_t)x;
        }else{
                v->type = TLD_JSON_DBL;
                v->value.d_num = (double)x;
        }
}

int tld_json_parse(const char *text, tld_json_obj **out)
{
        ASSERT(text != NULL, "No input");
        return tld_json_parse_n(text, strlen(text), out);
ERROR:
        return FAIL;
}

int tld_json_parse_n(const char *text, size_t len, tld_json_obj **out)
{
        tld__json_parser p;
        tld_json_obj* n = NULL;
        tld_json_arr* a = NULL;

        p.s = NULL;
        p.len = 0;
        p.pos = 0;
        p.depth = 0;
        p.buf = NULL;
        p.b_len = 0;
        p.b_alloc = 0;
        p.err = NULL;

        ASSERT(text != NULL, "No input");
        ASSERT(out != NULL, "No output pointer");
        ASSERT(len <= (size_t) INT32_MAX, "JSON text of %zu bytes is too long", len);

        p.s = (const uint8_t *) text;
        p.len = (int) len;

        p.b_alloc = 256;
        MMALLOC(p.buf, p.b_alloc);

        tld__jp_ws(&p);
        if(p.pos >= p.len){
                tld__jp_fail(&p, "empty input");
                goto PARSE_ERROR;
        }
        if(p.s[p.pos] == '{'){
                if(tld__jp_obj(&p, &n) != OK){
                        goto PARSE_ERROR;
                }
        }else if(p.s[p.pos] == '['){
                if(tld__jp_arr(&p, &a) != OK){
                        goto PARSE_ERROR;
                }
                RUN(tld_json_obj_alloc(&n));
                RUN(tld_strbuf_append(n->key[0], TLD__JSON_TOP_LEVEL_ARR));
                n->v[0]->type = TLD_JSON_ARR;
                n->v[0]->value.arr = a;
                a = NULL;
                n->n++;
                n->top_level_arr = 1;
                RUN(tld__json_obj_chk_space(n));
        }else{
                tld__jp_fail(&p, "text does not start with { or [");
                goto PARSE_ERROR;
        }
        tld__jp_ws(&p);
        if(p.pos < p.len){
                tld__jp_fail(&p, "trailing characters after JSON value");
                goto PARSE_ERROR;
        }

        MFREE(p.buf);
        *out = n;
        return OK;
PARSE_ERROR:
        ERROR_MSG("JSON parse error at byte %d: %s", p.pos, p.err ? p.err : "unknown error");
ERROR:
        if(a){
                tld_json_arr_free(a);
        }
        if(n){
                tld_json_obj_free(n);
        }
        if(p.buf){
                MFREE(p.buf);
        }
        return FAIL;
}

int tld__json_search(tld_json_obj *n, char *key, tld_json_obj* r)
{
        if(n){
                for(int i = 0; i < n->n;i++){
                        if(strcmp(TLD_STR(n->key[i]), key) == 0){
                                RUN(tld_strbuf_append(r->key[r->n], TLD_STR(n->key[i])));
                                RUN(tld_json_val_copy(r->v[r->n], n->v[i]));
                                r->n++;
                                RUN(tld__json_obj_chk_space(r));
                                /* a match is not searched inside; later
                                   siblings still are */
                                continue;
                        }

                        if(n->v[i]->type == TLD_JSON_ARR){
                                RUN(tld__json_get_arr_str(n->v[i]->value.arr,key,r));
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                RUN(tld__json_search(n->v[i]->value.obj, key,r));
                        }
                }
        }

        return OK;
ERROR:
        return FAIL;
}

int tld__json_get_arr_str(tld_json_arr *n, char *key, tld_json_obj* res)
{
        if(n){
                for(int i = 0; i < n->n;i++){
                        if(n->v[i]->type == TLD_JSON_ARR){
                                RUN(tld__json_get_arr_str(n->v[i]->value.arr,key,res));
                        }else if(n->v[i]->type == TLD_JSON_OBJ){
                                RUN(tld__json_search(n->v[i]->value.obj, key,res));
                        }
                }
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_search(tld_json_obj *n, char *key, tld_json_obj **ret)
{
        tld_json_obj* r = NULL;
        ASSERT(n != NULL, "No object");
        ASSERT(key != NULL, "No key");
        ASSERT(ret != NULL, "No output pointer");
        RUN(tld_json_obj_alloc(&r));
        RUN(tld__json_search(n, key, r));
        *ret = r;
        return OK;
ERROR:
        if(r){
                tld_json_obj_free(r);
        }
        return FAIL;
}

/* ------------------------------------------------------------------ */
/* Path getters                                                        */
/* ------------------------------------------------------------------ */

/* Resolve "a.b[2].c". *out is NULL when the path is empty (the root
   object itself). A path starting with '[' indexes the array that
   tld_json_parse wraps under "Top Level Arr". */
int tld__json_path_resolve(tld_json_obj *root, const char *path, tld_json_val **out)
{
        tld_json_val* v = NULL;
        tld_json_obj* o = root;
        tld_json_arr* a = NULL;
        const char* p = path;
        int first = 1;

        if(root == NULL || path == NULL){
                return FAIL;
        }

        if(*p == '['){
                /* address the wrapped top-level array */
                v = NULL;
                for(int i = 0; i < root->n; i++){
                        if(strcmp(TLD_STR(root->key[i]), TLD__JSON_TOP_LEVEL_ARR) == 0){
                                v = root->v[i];
                                break;
                        }
                }
                if(v == NULL || v->type != TLD_JSON_ARR){
                        return FAIL;
                }
                o = NULL;
                a = v->value.arr;
                first = 0;
        }

        while(*p){
                if(*p == '['){
                        long idx = 0;
                        int digits = 0;
                        p++;
                        while(*p >= '0' && *p <= '9'){
                                if(digits >= 9){
                                        return FAIL;
                                }
                                idx = idx * 10 + (*p - '0');
                                digits++;
                                p++;
                        }
                        if(!digits || *p != ']'){
                                return FAIL;
                        }
                        p++;
                        if(a == NULL || idx >= a->n){
                                return FAIL;
                        }
                        v = a->v[idx];
                }else{
                        const char* k;
                        int klen;
                        tld_json_val* found = NULL;
                        if(!first){
                                if(*p != '.'){
                                        return FAIL;
                                }
                                p++;
                        }
                        k = p;
                        while(*p && *p != '.' && *p != '['){
                                p++;
                        }
                        klen = (int)(p - k);
                        if(klen == 0 || o == NULL){
                                return FAIL;
                        }
                        for(int i = 0; i < o->n; i++){
                                if(o->key[i]->len == klen &&
                                   memcmp(o->key[i]->str, k, klen) == 0){
                                        found = o->v[i];
                                        break;
                                }
                        }
                        if(found == NULL){
                                return FAIL;
                        }
                        v = found;
                }
                first = 0;
                o = NULL;
                a = NULL;
                if(v->type == TLD_JSON_OBJ){
                        o = v->value.obj;
                }else if(v->type == TLD_JSON_ARR){
                        a = v->value.arr;
                }
        }
        *out = v;
        return OK;
}

int tld_json_get_str(tld_json_obj *obj, const char *path, const char **out)
{
        ASSERT(obj != NULL, "No object");
        ASSERT(path != NULL, "No path");
        tld_json_val* v = NULL;
        if(tld__json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type != TLD_JSON_STR || v->value.str == NULL){
                return FAIL;
        }
        if(out){
                *out = TLD_STR(v->value.str);
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_num(tld_json_obj *obj, const char *path, double *out)
{
        ASSERT(obj != NULL, "No object");
        ASSERT(path != NULL, "No path");
        tld_json_val* v = NULL;
        if(tld__json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type == TLD_JSON_DBL){
                if(out){
                        *out = v->value.d_num;
                }
        }else if(v->type == TLD_JSON_INT){
                if(out){
                        *out = (double)v->value.i_num;
                }
        }else{
                return FAIL;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_int(tld_json_obj *obj, const char *path, int64_t *out)
{
        ASSERT(obj != NULL, "No object");
        ASSERT(path != NULL, "No path");
        tld_json_val* v = NULL;
        if(tld__json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type == TLD_JSON_INT){
                if(out){
                        *out = (int64_t)v->value.i_num;
                }
        }else if(v->type == TLD_JSON_DBL){
                double d = v->value.d_num;
                /* only integral values inside the int64_t range */
                if(!isfinite(d) || d != floor(d) ||
                   d < -9223372036854775808.0 || d >= 9223372036854775808.0){
                        return FAIL;
                }
                if(out){
                        *out = (int64_t)d;
                }
        }else{
                return FAIL;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_bool(tld_json_obj *obj, const char *path, int *out)
{
        ASSERT(obj != NULL, "No object");
        ASSERT(path != NULL, "No path");
        tld_json_val* v = NULL;
        if(tld__json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type == TLD_JSON_BOOL_TRUE){
                if(out){
                        *out = 1;
                }
        }else if(v->type == TLD_JSON_BOOL_FALSE){
                if(out){
                        *out = 0;
                }
        }else{
                return FAIL;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_obj(tld_json_obj *obj, const char *path, tld_json_obj **out)
{
        ASSERT(obj != NULL, "No object");
        ASSERT(path != NULL, "No path");
        tld_json_val* v = NULL;
        if(tld__json_path_resolve(obj, path, &v) != OK){
                return FAIL;
        }
        if(v == NULL){
                /* empty path: the object itself */
                if(out){
                        *out = obj;
                }
                return OK;
        }
        if(v->type != TLD_JSON_OBJ || v->value.obj == NULL){
                return FAIL;
        }
        if(out){
                *out = v->value.obj;
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_get_arr(tld_json_obj *obj, const char *path, tld_json_arr **out)
{
        ASSERT(obj != NULL, "No object");
        ASSERT(path != NULL, "No path");
        tld_json_val* v = NULL;
        if(tld__json_path_resolve(obj, path, &v) != OK || v == NULL){
                return FAIL;
        }
        if(v->type != TLD_JSON_ARR || v->value.arr == NULL){
                return FAIL;
        }
        if(out){
                *out = v->value.arr;
        }
        return OK;
ERROR:
        return FAIL;
}

/* Make sure obj (allocated here when NULL) has a free slot at index n BEFORE anything is
 * written to it. On failure an object allocated here is freed. */
static int tld__json_obj_slot(tld_json_obj *obj, tld_json_obj **np)
{
        tld_json_obj *n = obj;
        if(n == NULL){
                RUN(tld_json_obj_alloc(&n));
        }
        if(n->n >= n->n_alloc){
                RUN(tld__json_obj_realloc(n));
        }
        *np = n;
        return OK;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
        }
        *np = NULL;
        return FAIL;
}

static int tld__json_arr_slot(tld_json_arr *arr, tld_json_arr **np)
{
        tld_json_arr *n = arr;
        if(n == NULL){
                RUN(tld_json_arr_alloc(&n));
        }
        if(n->n >= n->n_alloc){
                RUN(tld__json_arr_realloc(n));
        }
        *np = n;
        return OK;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
        }
        *np = NULL;
        return FAIL;
}

/* Set the value at path (same syntax as the tld_json_get_* getters) to a deep copy of v.
   The last step may name a missing key (it is added) or the index one past the end of an
   array (the value is appended); everything before it must exist. */
int tld_json_set_val(tld_json_obj *n, char *key, tld_json_val *v)
{
        const char* path = key;
        const char* last = NULL;
        tld_json_val* pv = NULL;
        tld_json_obj* po = NULL;
        tld_json_arr* pa = NULL;
        char* prefix = NULL;
        size_t plen;

        ASSERT(n != NULL, "No JSON object");
        ASSERT(path != NULL && *path, "No path");
        ASSERT(v != NULL, "No value");

        /* the last step starts at the last '.' or '[' (keys contain neither) */
        for(const char* c = path; *c; c++){
                if(*c == '.' || *c == '['){
                        last = c;
                }
        }
        if(last == NULL){
                last = path;            /* a single key */
        }
        plen = (size_t)(last - path);

        if(plen == 0 && *last == '['){
                /* "[i]": the array that tld_json_parse wraps under "Top Level Arr" */
                for(int i = 0; i < n->n; i++){
                        if(strcmp(TLD_STR(n->key[i]), TLD__JSON_TOP_LEVEL_ARR) == 0){
                                pv = n->v[i];
                                break;
                        }
                }
                if(pv == NULL || pv->type != TLD_JSON_ARR){
                        ERROR_MSG("Path %s: no top-level array", path);
                }
                pa = pv->value.arr;
        }else if(plen == 0){
                po = n;
        }else{
                MMALLOC(prefix, plen + 1);
                memcpy(prefix, path, plen);
                prefix[plen] = 0;
                if(tld__json_path_resolve(n, prefix, &pv) != OK || pv == NULL){
                        ERROR_MSG("Path %s: %s does not exist", path, prefix);
                }
                if(pv->type == TLD_JSON_OBJ){
                        po = pv->value.obj;
                }else if(pv->type == TLD_JSON_ARR){
                        pa = pv->value.arr;
                }
                MFREE(prefix);
        }

        if(*last == '['){
                long idx = 0;
                int digits = 0;
                const char* c = last + 1;
                while(*c >= '0' && *c <= '9' && digits < 9){
                        idx = idx * 10 + (*c - '0');
                        digits++;
                        c++;
                }
                if(!digits || *c != ']' || c[1] != 0){
                        ERROR_MSG("Path %s: bad array index", path);
                }
                if(pa == NULL){
                        ERROR_MSG("Path %s: not an array", path);
                }
                if(idx < pa->n){
                        RUN(tld_json_val_copy(pa->v[idx], v));
                }else if(idx == pa->n){
                        tld_json_arr* r = NULL;
                        RUN(tld__json_arr_slot(pa, &r));
                        RUN(tld_json_val_copy(pa->v[pa->n], v));
                        pa->n++;
                }else{
                        ERROR_MSG("Path %s: index %ld is past the end (%d entries)", path, idx, pa->n);
                }
                return OK;
        }

        {
                const char* k = (*last == '.') ? last + 1 : last;
                int klen = (int)strlen(k);
                tld_json_obj* r = NULL;
                if(klen == 0){
                        ERROR_MSG("Path %s: empty key", path);
                }
                if(po == NULL){
                        ERROR_MSG("Path %s: not an object", path);
                }
                for(int i = 0; i < po->n; i++){
                        if(po->key[i]->len == klen && memcmp(po->key[i]->str, k, klen) == 0){
                                RUN(tld_json_val_copy(po->v[i], v));
                                return OK;
                        }
                }
                RUN(tld__json_obj_slot(po, &r));
                tld_strbuf_clear(po->key[po->n]);
                RUN(tld_strbuf_append(po->key[po->n], (char*)k));
                RUN(tld_json_val_copy(po->v[po->n], v));
                po->n++;
        }
        return OK;
ERROR:
        MFREE(prefix);
        return FAIL;
}

static void tld__json_free_unattached(tld_json_decoration *kvs, int from, int count)
{
        for(int k = from; k < count; k++){
                if(kvs[k].value == NULL){
                        continue;
                }
                if(kvs[k].type == TLD__JVAL_OBJ){
                        tld_json_obj_free((tld_json_obj*)kvs[k].value);
                }else if(kvs[k].type == TLD__JVAL_ARR){
                        tld_json_arr_free((tld_json_arr*)kvs[k].value);
                }
        }
}

/* The add helpers put the entry d into the next slot and return the (possibly new)
 * container. When the entry cannot be added it is dropped (an object or array it holds
 * stays unattached); a container allocated by the call is freed and NULL returned. */
tld_json_obj *tld__json_obj_add_entry(tld_json_obj *obj, tld_json_decoration *d)
{
        tld_json_obj* n = NULL;
        RUN(tld__json_obj_slot(obj, &n));
        tld_strbuf_clear(n->key[n->n]);
        RUN(tld_strbuf_append(n->key[n->n], d->key ? d->key : ""));
        RUN(tld__json_val_from_decoration(n->v[n->n], d));
        n->n++;
        return n;
ERROR:
        if(obj == NULL && n){
                tld_json_obj_free(n);
                return NULL;
        }
        return obj;
}

tld_json_arr *tld__json_arr_add_entry(tld_json_arr *arr, tld_json_decoration *d)
{
        tld_json_arr* n = NULL;
        RUN(tld__json_arr_slot(arr, &n));
        RUN(tld__json_val_from_decoration(n->v[n->n], d));
        n->n++;
        return n;
ERROR:
        if(arr == NULL && n){
                tld_json_arr_free(n);
                return NULL;
        }
        return arr;
}

/* Used by TLD_JAUTO(): the type is guessed from the text (callers rely on
   TLD_JAUTO("k","0.2") producing a number). */
int tld__json_add_val(char *buf, int len, tld_json_val *val)
{
        tld_json_type type = TLD_JSON_STR;
        tld__json_detect_type(buf,len, &type);

        switch (type) {
        case TLD_JSON_DBL:
                val->type = TLD_JSON_DBL;
                val->value.d_num = strtod(buf, NULL);
                break;
        case TLD_JSON_INT:{
                long long x;
                errno = 0;
                x = strtoll(buf, NULL, 10);
                if(errno == ERANGE){
                        val->type = TLD_JSON_DBL;
                        val->value.d_num = strtod(buf, NULL);
                }else{
                        tld__json_val_set_int64(val, x);
                }
                break;
        }
        case TLD_JSON_BOOL_TRUE:
                val->type = TLD_JSON_BOOL_TRUE;
                val->value.b_num = 1;
                break;
        case TLD_JSON_BOOL_FALSE:
                val->type = TLD_JSON_BOOL_FALSE;
                val->value.b_num = 0;
                break;
        case TLD_JSON_STR:
                RUN(tld__json_val_set_str(val, buf, len));
                break;
        case TLD_JSON_ARR:
        case TLD_JSON_OBJ:
        case TLD_JSON_NULL:
                ERROR_MSG("This should never happen!!@");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

/* Fill an empty (TLD_JSON_NULL) slot from a decoration of any kind. An object or
   array is attached, not copied. */
int tld__json_val_from_decoration(tld_json_val *v, tld_json_decoration *d)
{
        switch(d->type){
        case TLD__JVAL_AUTO:{
                char* s = (char*)d->value;
                RUN(tld__json_add_val(s, (int)strlen(s), v));
                break;
        }
        case TLD__JVAL_OBJ:
                v->type = TLD_JSON_OBJ;
                v->value.obj = (tld_json_obj*)d->value;
                break;
        case TLD__JVAL_ARR:
                v->type = TLD_JSON_ARR;
                v->value.arr = (tld_json_arr*)d->value;
                break;
        case TLD__JVAL_STR:{
                const char* s = (const char*)d->value;
                RUN(tld__json_val_set_str(v, s, (int)strlen(s)));
                break;
        }
        case TLD__JVAL_NUM:
                v->type = TLD_JSON_DBL;
                v->value.d_num = *(double*)d->value;
                break;
        case TLD__JVAL_INT:
                tld__json_val_set_int64(v, *(long*)d->value);
                break;
        case TLD__JVAL_BOOL:
                if(*(int*)d->value){
                        v->type = TLD_JSON_BOOL_TRUE;
                        v->value.b_num = 1;
                }else{
                        v->type = TLD_JSON_BOOL_FALSE;
                        v->value.b_num = 0;
                }
                break;
        case TLD__JVAL_NULL:
                v->type = TLD_JSON_NULL;
                break;
        default:
                ERROR_MSG("Unexpected decoration type %d", (int)d->type);
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

/* TLD_JSON_OBJECT / TLD_JSON_ARRAY back ends. They stop at the first entry that cannot be added and
 * return NULL: the TLD_JOBJ/TLD_JARR children of that entry and the later ones are freed, as is a
 * container created by the call; a caller-supplied container keeps the entries added
 * so far and stays the caller's. A TLD_JOBJ/TLD_JARR entry with a NULL value is such a failure
 * (a nested TLD_JSON_OBJECT / TLD_JSON_ARRAY returns NULL only when it failed). */
tld_json_obj *tld__json_obj_build(tld_json_obj *obj, tld_json_decoration *kvs, int count)
{
        tld_json_obj *in = obj;
        int i = 0;
        for (i = 0; i < count; i++) {
                int before = obj ? obj->n : 0;
                if(kvs[i].value == NULL){
                        if(kvs[i].type == TLD__JVAL_OBJ || kvs[i].type == TLD__JVAL_ARR){
                                ERROR_MSG("Entry %d holds no object or array (did a nested TLD_JSON_OBJECT / TLD_JSON_ARRAY fail?)", i + 1);
                        }
                        continue;
                }
                obj = tld__json_obj_add_entry(obj, &kvs[i]);
                if(obj == NULL || obj->n != before + 1){
                        ERROR_MSG("Could not add entry %d", i + 1);
                }
        }
        return obj;
ERROR:
        tld__json_free_unattached(kvs, i, count);
        if(in == NULL){
                tld_json_obj_free(obj);
        }
        return NULL;
}

tld_json_arr *tld__json_arr_build(tld_json_arr *arr, tld_json_decoration *kvs, int count)
{
        tld_json_arr *in = arr;
        int i = 0;
        for (i = 0; i < count; i++) {
                int before = arr ? arr->n : 0;
                if(kvs[i].value == NULL){
                        if(kvs[i].type == TLD__JVAL_OBJ || kvs[i].type == TLD__JVAL_ARR){
                                ERROR_MSG("Entry %d holds no object or array (did a nested TLD_JSON_OBJECT / TLD_JSON_ARRAY fail?)", i + 1);
                        }
                        continue;
                }
                arr = tld__json_arr_add_entry(arr, &kvs[i]);
                if(arr == NULL || arr->n != before + 1){
                        ERROR_MSG("Could not add entry %d", i + 1);
                }
        }
        return arr;
ERROR:
        tld__json_free_unattached(kvs, i, count);
        if(in == NULL){
                tld_json_arr_free(arr);
        }
        return NULL;
}

/* Write s as a quoted JSON string, escaping '"', '\\' and control
   characters. Well-formed UTF-8 is written unchanged; a byte that is not part of a
   well-formed sequence becomes \ufffd (U+FFFD), so the text is always valid JSON. */
int tld__json_dump_str(tld_strbuf *b, const uint8_t *s, int len)
{
        static const char hex[] = "0123456789abcdef";
        char* out = NULL;
        int o = 0;

        MMALLOC(out, (size_t)len * 6 + 3);
        out[o++] = '"';
        for(int i = 0; i < len; i++){
                uint8_t c = s[i];
                switch(c){
                case '"':  out[o++] = '\\'; out[o++] = '"';  break;
                case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
                case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
                case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
                case '\t': out[o++] = '\\'; out[o++] = 't';  break;
                case '\b': out[o++] = '\\'; out[o++] = 'b';  break;
                case '\f': out[o++] = '\\'; out[o++] = 'f';  break;
                default:
                        if(c < 0x20){
                                out[o++] = '\\';
                                out[o++] = 'u';
                                out[o++] = '0';
                                out[o++] = '0';
                                out[o++] = hex[c >> 4];
                                out[o++] = hex[c & 0xF];
                        }else if(c < 0x80){
                                out[o++] = (char)c;
                        }else{
                                int n = tld__jp_utf8_len(s + i, len - i);
                                if(n){
                                        memcpy(out + o, s + i, (size_t) n);
                                        o += n;
                                        i += n - 1;
                                }else{
                                        memcpy(out + o, "\\ufffd", 6);
                                        o += 6;
                                }
                        }
                        break;
                }
        }
        out[o++] = '"';
        out[o] = 0;
        RUN(tld_strbuf_append(b, out));
        MFREE(out);
        return OK;
ERROR:
        if(out){
                MFREE(out);
        }
        return FAIL;
}

int tld__json_dump_val(tld_json_val *val, tld_strbuf *b)
{
        char buf[40];
        if(val->type == TLD_JSON_STR){
                if(val->value.str){
                        RUN(tld__json_dump_str(b, val->value.str->str, val->value.str->len));
                }else{
                        RUN(tld_strbuf_append(b, "null"));
                }
        }else if(val->type == TLD_JSON_INT) {
                snprintf(buf, sizeof(buf), "%d", val->value.i_num);
                RUN(tld_strbuf_append(b, buf));
        }else if(val->type == TLD_JSON_DBL){
                double d = val->value.d_num;
                if(!isfinite(d)){
                        /* inf / nan are not valid JSON */
                        RUN(tld_strbuf_append(b, "null"));
                }else{
                        /* shortest of %.15g .. %.17g that reads back exactly */
                        for(int prec = 15; prec <= 17; prec++){
                                snprintf(buf, sizeof(buf), "%.*g", prec, d);
                                if(strtod(buf, NULL) == d){
                                        break;
                                }
                        }
                        RUN(tld_strbuf_append(b, buf));
                }
        }else if(val->type == TLD_JSON_BOOL_TRUE){
                RUN(tld_strbuf_append(b, "true"));
        }else if(val->type == TLD_JSON_BOOL_FALSE){
                RUN(tld_strbuf_append(b, "false"));
        }else{
                RUN(tld_strbuf_append(b, "null"));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld__json_add_indent(tld_strbuf *b, int indent)
{
        static const char spaces[] = "                                ";   /* 32 */
        const int n_spaces = (int) sizeof spaces - 1;
        while(indent > 0){
                int k = indent < n_spaces ? indent : n_spaces;
                RUN(tld_strbuf_append(b, spaces + n_spaces - k));
                indent -= k;
        }
        return OK;
ERROR:
        return FAIL;
}

/* ------------------------------------------------------------------ */
/* Writer                                                              */
/* ------------------------------------------------------------------ */

/* One value of an object or array: nested containers recurse with the deeper indent
   (or stay compact), NULL containers are written as null. */
static int tld__json_dump_any(tld_json_val *v, tld_strbuf *b, int indent)
{
        if(v->type == TLD_JSON_ARR){
                if(v->value.arr){
                        RUN(tld_json_dump_arr(v->value.arr, b, indent));
                }else{
                        RUN(tld_strbuf_append(b, "null"));
                }
        }else if(v->type == TLD_JSON_OBJ){
                if(v->value.obj){
                        RUN(tld_json_dump(v->value.obj, b, indent));
                }else{
                        RUN(tld_strbuf_append(b, "null"));
                }
        }else{
                RUN(tld__json_dump_val(v, b));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_dump(tld_json_obj *n, tld_strbuf *b,int indent)
{
        const int compact = indent < 0;
        const int inner = compact ? TLD_JSON_COMPACT : indent + 4;
        ASSERT(b != NULL, "No output buffer");
        if(n && n->top_level_arr && n->n == 1 && n->v[0]->type == TLD_JSON_ARR && n->v[0]->value.arr){
                /* a parsed top-level array goes back out as the array */
                return tld_json_dump_arr(n->v[0]->value.arr, b, indent);
        }
        if(n){
                RUN(tld_strbuf_append(b, compact ? "{" : "{\n"));
                for(int i = 0; i < n->n;i++){
                        RUN(tld__json_add_indent(b, inner));
                        RUN(tld__json_dump_str(b, n->key[i]->str, n->key[i]->len));
                        RUN(tld_strbuf_append(b, compact ? ":" : ": "));
                        RUN(tld__json_dump_any(n->v[i], b, inner));
                        if (i < n->n - 1) {
                                RUN(tld_strbuf_append(b, ","));
                        }
                        if(!compact){
                                RUN(tld_strbuf_append(b, "\n"));
                        }
                }
                RUN(tld__json_add_indent(b, indent));
                RUN(tld_strbuf_append(b, "}"));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_dump_arr(tld_json_arr *n, tld_strbuf *b,int indent)
{
        const int compact = indent < 0;
        const int inner = compact ? TLD_JSON_COMPACT : indent + 4;
        ASSERT(b != NULL, "No output buffer");
        if(n){
                RUN(tld_strbuf_append(b, compact ? "[" : "[\n"));
                for (int i = 0; i < n->n; i++) {
                        RUN(tld__json_add_indent(b, inner));
                        RUN(tld__json_dump_any(n->v[i], b, inner));
                        if (i < n->n - 1) {
                                RUN(tld_strbuf_append(b, ","));
                        }
                        if(!compact){
                                RUN(tld_strbuf_append(b, "\n"));
                        }
                }
                RUN(tld__json_add_indent(b, indent));
                RUN(tld_strbuf_append(b, "]"));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_json_obj_add(tld_json_obj **obj, tld_json_decoration entry)
{
        tld_json_obj *r = NULL;
        ASSERT(obj != NULL, "No object pointer");
        if(entry.value == NULL && (entry.type == TLD__JVAL_AUTO || entry.type == TLD__JVAL_STR)){
                return OK;      /* skipped, as in a build */
        }
        r = tld__json_obj_build(*obj, &entry, 1);
        if(r == NULL){
                ERROR_MSG("Could not add an entry to the object");
        }
        *obj = r;
        return OK;
ERROR:
        return FAIL;
}

int tld_json_arr_add(tld_json_arr **arr, tld_json_decoration entry)
{
        tld_json_arr *r = NULL;
        ASSERT(arr != NULL, "No array pointer");
        if(entry.value == NULL && (entry.type == TLD__JVAL_AUTO || entry.type == TLD__JVAL_STR)){
                return OK;      /* skipped, as in a build */
        }
        r = tld__json_arr_build(*arr, &entry, 1);
        if(r == NULL){
                ERROR_MSG("Could not add a value to the array");
        }
        *arr = r;
        return OK;
ERROR:
        return FAIL;
}

#undef TLD__JSON_MAX_DEPTH
#undef TLD__JSON_TOP_LEVEL_ARR

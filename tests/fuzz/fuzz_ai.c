/* tld_ai.h reply decoding: chunked bodies, JSON replies, struct unpacking, cache lines. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld.h"
#include "tld_ai.h"
#define F(X) X(str, name, "") X(num, score, "") X(int, count, "") X(bool, ok, "")
AI_STRUCT(fz, F)
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        if (n < 1 || n > 1 << 16) return 0;
        uint8_t mode = d[0] % 4; d++; n--;
        char *s = malloc(n + 1); memcpy(s, d, n); s[n] = 0;
        ai__begin();
        if (mode == 0) {
                tld_strbuf *o = NULL; tld_strbuf_alloc(&o, 16);
                ai__dechunk(s, n, o); tld_strbuf_free(o);
        } else if (mode == 1) {
                tld_json_obj *j = NULL;
                if (ai__parse(s, &j) == 0) {
                        ai__str(j, "message.content"); ai__num(j, "prompt_eval_count", 0); ai__long(j, "eval_count");
                        tld_json_obj_free(j);
                }
        } else if (mode == 2) {
                /* both unpacking paths on the same reply: into a struct (ai_fill) and into
                   a JSON object (ai_fill_dyn); the struct's strings are freed either way */
                struct fz x; const ai_type *t = fz__type();
                tld_json_obj *jo = NULL;
                ai__unpack(&x, NULL, t, s);
                ai__free_strs(&x, t);
                if (ai__unpack(NULL, &jo, t, s) == 0) tld_json_obj_free(jo);
        } else {
                if (!ai__cache) tld_hash_alloc(&ai__cache, 64);
                tld_strbuf *l = NULL; tld_strbuf_alloc(&l, (int)n + 1); tld_strbuf_append(l, s);
                int sk = 0; ai__cache_line(l, &sk); tld_strbuf_free(l);
        }
        free(s);
        return 0;
}

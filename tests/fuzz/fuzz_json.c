/* fuzz-json: tld_json_parse on arbitrary bytes, then everything done to a parsed tree:
 * getters on odd paths, dump (compact and indented), deep copy and set_val. */
#define TLD_IMPLEMENTATION
#include "tld.h"
#include <string.h>
static char *cstr(const uint8_t *d, size_t n) { char *s = malloc(n + 1); memcpy(s, d, n); s[n] = 0; return s; }
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        if (n > 1 << 16) return 0;
        char *s = cstr(d, n);
        tld_strbuf *b = NULL; tld_json_obj *o = NULL, *c = NULL; tld_strbuf *out = NULL;
        tld_strbuf_alloc(&b, (int)n + 1); tld_append(b, s);
        if (tld_json_parse(b, &o) == OK) {
                const char *str; double x; int64_t i; int bo; tld_json_obj *so; tld_json_arr *sa;
                static const char *paths[] = {"a", "a.b", "a[0]", "[0]", "[1].x", "a.b[2].c", "x[99999999999]", "a[-1]", "", ".", "[", "a..b", "a[", "a[0", "a]", "model", "message.content"};
                for (size_t k = 0; k < sizeof paths / sizeof *paths; k++) {
                        char *p = (char *)paths[k];
                        tld_json_get_str(o, p, &str); tld_json_get_num(o, p, &x); tld_json_get_int(o, p, &i);
                        tld_json_get_bool(o, p, &bo); tld_json_get_obj(o, p, &so); tld_json_get_arr(o, p, &sa);
                }
                tld_strbuf_alloc(&out, 64); tld_json_dump(o, out, 0); tld_json_dump(o, out, 2);
                if (tld_json_obj_copy(&c, o) == OK) {
                        tld_json_val *v = NULL; tld_json_val_alloc(&v); v->type = TLD_JSON_INT; v->value.i_num = 1;
                        tld_json_set_val(c, "a", v); tld_json_set_val(c, "a.b", v); tld_json_set_val(c, "[0]", v); tld_json_set_val(c, "a[0]", v);
                        tld_json_val_free(v); tld_json_obj_free(c);
                }
                tld_strbuf_free(out); tld_json_obj_free(o);
        }
        tld_strbuf_free(b); free(s);
        return 0;
}

/* unit-fix-json-build: JSON_OBJ / JSON_ARR / json_*_add_multiple grow their containers
 * before writing (they used to write first and grow after, ignoring a failed grow, and the
 * realloc counted slots that were never allocated). Builds large objects and arrays one
 * entry at a time and in bulk, then checks counts and a dump/parse round trip. */
#include "tld.h"
#include <string.h>

int main(void)
{
        tld_json_obj *o = NULL;
        tld_json_obj *p = NULL;
        tld_json_arr *a = NULL;
        tld_strbuf *b = NULL;
        char key[32];
        const char *s = NULL;
        int64_t v = 0;

        for (int i = 0; i < 5000; i++) {
                snprintf(key, sizeof key, "k%d", i);
                tld_json_obj *r = json_obj_add_multiple(o, (tld_json_decoration[]){JINT(key, i)}, 1);
                if (r == NULL) ERROR_MSG("add %d failed", i);
                o = r;
        }
        if (o->n != 5000) ERROR_MSG("object has %d entries, want 5000", o->n);
        if (o->n_alloc < o->n) ERROR_MSG("n_alloc %d < n %d", o->n_alloc, o->n);

        for (int i = 0; i < 5000; i++) {
                tld_json_arr *r = json_arr_add_multiple(a, (tld_json_decoration[]){JSTR(NULL, "x\"y")}, 1);
                if (r == NULL) ERROR_MSG("arr add %d failed", i);
                a = r;
        }
        if (a->n != 5000) ERROR_MSG("array has %d entries, want 5000", a->n);

        o = json_obj_add_multiple(o, (tld_json_decoration[]){JA("list", a)}, 1);
        a = NULL; /* now owned by o */

        RUN(tld_strbuf_alloc(&b, 1024));
        RUN(tld_json_dump(o, b, 0));
        RUN(tld_json_parse(b, &p));
        RUN(tld_json_get_int(p, "k4999", &v));
        if (v != 4999) ERROR_MSG("k4999 = %lld", (long long)v);
        RUN(tld_json_get_str(p, "list[4999]", &s));
        if (strcmp(s, "x\"y") != 0) ERROR_MSG("list[4999] = %s", s);

        tld_json_obj_free(p);
        tld_json_obj_free(o);
        tld_strbuf_free(b);
        fprintf(stdout, "unit-fix-json-build: ok\n");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

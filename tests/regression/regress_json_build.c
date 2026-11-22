/* regress-json-build: JSON_OBJ / JSON_ARR / json_*_add_multiple grow their containers
 * before writing (they used to write first and grow after, ignoring a failed grow, and the
 * realloc counted slots that were never allocated). Builds large objects and arrays one
 * entry at a time and in bulk, then checks counts and a dump/parse round trip. */
#include "tld.h"
#include "test.h"
#include <string.h>

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "",
                  "Builds a 5000-entry JSON object and a 5000-entry array one entry at a time,\n"
                  "checks the counts, then dumps, parses and reads back k4999 and list[4999].\n"
                  "Takes no arguments.");
        if (argc > 1) {
                fprintf(stderr, "regress-json-build takes no arguments (see -h)\n");
                return 2;
        }
        tld_json_obj *o = NULL;
        tld_json_obj *p = NULL;
        tld_json_arr *a = NULL;
        tld_strbuf *b = NULL;
        char key[32];
        const char *s = NULL;
        int64_t v = 0;

        for (int i = 0; i < 5000; i++) {
                snprintf(key, sizeof key, "k%d", i);
                tld_json_obj *r = tld__json_obj_build(o, (tld_json_decoration[]){JINT(key, i)}, 1);
                if (r == NULL) ERROR_MSG("add %d failed", i);
                o = r;
        }
        if (o->n != 5000) ERROR_MSG("object has %d entries, want 5000", o->n);
        if (o->n_alloc < o->n) ERROR_MSG("n_alloc %d < n %d", o->n_alloc, o->n);
        SECTION_MSG("object and array grown one entry at a time");
        KV_MSG("object", "%d entries, %d allocated", o->n, o->n_alloc);

        for (int i = 0; i < 5000; i++) {
                tld_json_arr *r = tld__json_arr_build(a, (tld_json_decoration[]){JSTR(NULL, "x\"y")}, 1);
                if (r == NULL) ERROR_MSG("arr add %d failed", i);
                a = r;
        }
        if (a->n != 5000) ERROR_MSG("array has %d entries, want 5000", a->n);
        KV_MSG("array", "%d entries", a->n);
        OK_MSG("counts are right");

        o = tld__json_obj_build(o, (tld_json_decoration[]){JA("list", a)}, 1);
        a = NULL; /* now owned by o */

        RUN(tld_strbuf_alloc(&b, 1024));
        RUN(tld_json_dump(o, b, 0));
        SECTION_MSG("dump and parse round trip");
        KV_MSG("dump", "%d bytes", TLD_STRLEN(b));
        RUN(tld_json_parse(b, &p));
        RUN(tld_json_get_int(p, "k4999", &v));
        if (v != 4999) ERROR_MSG("k4999 = %lld", (long long)v);
        RUN(tld_json_get_str(p, "list[4999]", &s));
        if (strcmp(s, "x\"y") != 0) ERROR_MSG("list[4999] = %s", s);

        tld_json_obj_free(p);
        tld_json_obj_free(o);
        tld_strbuf_free(b);
        OK_MSG("k4999 = %lld, list[4999] = %s", (long long)v, "x\"y");
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

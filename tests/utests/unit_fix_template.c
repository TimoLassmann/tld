/* Tests for the template fixes (modernisation). */
#include "tld.h"

#include <stdlib.h>
#include <string.h>

static int n_fail = 0;
static int n_check = 0;

#define CHECK(x) do {                                                   \
                n_check++;                                              \
                if(!(x)){                                               \
                        fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
                        n_fail++;                                       \
                }                                                       \
        } while(0)

/* Apply template t with the key/value pairs in kv (NULL-terminated) and
   compare with expect. expect == NULL means tld_template_apply must FAIL. */
static void check_apply(const char *t, const char **kv, const char *expect)
{
        tld_strbuf* b = NULL;
        tld_hash* h = NULL;
        int r;
        tld_strbuf_alloc(&b, 64);
        tld_append(b, (char*)t);
        tld_hash_alloc(&h, 64);
        for(int i = 0; kv && kv[i]; i += 2){
                tld_hash_add(&h, (char*)kv[i], (char*)kv[i + 1]);
        }
        r = tld_template_apply(b, h);
        if(expect){
                CHECK(r == OK);
                CHECK(strcmp(TLD_STR(b), expect) == 0);
                if(r != OK || strcmp(TLD_STR(b), expect) != 0){
                        fprintf(stderr, "  template [%s]\n  expected [%s]\n  got      [%s]\n", t, expect, TLD_STR(b));
                }
        }else{
                CHECK(r == FAIL);
                if(r != FAIL){
                        fprintf(stderr, "  template [%s] should fail\n", t);
                }
        }
        tld_hash_free(h);
        tld_strbuf_free(b);
}

static void test_else(void)
{
        const char* t = "{{IF a}}{{IF b}}X{{ENDIF}}{{ELSE}}Y{{ENDIF}}";
        const char* a1_b0[] = {"a", "1", "b", "", NULL};
        const char* a1_b1[] = {"a", "1", "b", "1", NULL};
        const char* a0_b1[] = {"a", "", "b", "1", NULL};
        const char* a1[] = {"a", "1", NULL};
        const char* none[] = {NULL};

        check_apply(t, a1_b0, "");
        check_apply(t, a1_b1, "X");
        check_apply(t, a0_b1, "Y");
        check_apply(t, a1, "");
        check_apply(t, none, "Y");

        /* deeper: the false innermost IF must not pick any outer ELSE */
        check_apply("{{IF a}}{{IF a}}{{IF b}}X{{ENDIF}}{{ELSE}}P{{ENDIF}}{{ELSE}}Q{{ENDIF}}!", a1_b0, "!");

        /* inner ELSE still works */
        check_apply("{{IF a}}<{{IF b}}X{{ELSE}}Z{{ENDIF}}>{{ELSE}}Y{{ENDIF}}", a1_b0, "<Z>");
        check_apply("{{IF a}}<{{IF b}}X{{ELSE}}Z{{ENDIF}}>{{ELSE}}Y{{ENDIF}}", a1_b1, "<X>");
        check_apply("{{IF a}}<{{IF b}}X{{ELSE}}Z{{ENDIF}}>{{ELSE}}Y{{ENDIF}}", a0_b1, "Y");

        /* IF inside ELSE */
        check_apply("{{IF a}}A{{ELSE}}{{IF b}}B{{ELSE}}C{{ENDIF}}{{ENDIF}}", a0_b1, "B");
        check_apply("{{IF a}}A{{ELSE}}{{IF b}}B{{ELSE}}C{{ENDIF}}{{ENDIF}}", none, "C");

        /* sibling IFs: a false IF without ELSE followed by an IF with one */
        check_apply("{{IF b}}B{{ENDIF}}{{IF a}}A{{ELSE}}N{{ENDIF}}", a1_b0, "A");
        check_apply("{{IF b}}B{{ENDIF}}{{IF a}}A{{ELSE}}N{{ENDIF}}", a0_b1, "BN");
}

static void test_basic(void)
{
        const char* kv[] = {"name", "Alice", "age", "101", NULL};
        check_apply("Hello, {{name}}!", kv, "Hello, Alice!");
        check_apply("{{IF age}}You are {{age}}.{{ELSE}}Unknown.{{ENDIF}}", kv, "You are 101.");
        check_apply("{{IF major}}{{major}}{{ELSE}}none{{ENDIF}}", kv, "none");
        check_apply("Hi {{missing}}.", kv, "Hi .");
        check_apply("plain text", kv, "plain text");
}

static void test_errors(void)
{
        const char* kv[] = {"name", "Alice", NULL};
        tld_strbuf* b = NULL;
        tld_hash* h = NULL;
        char* deep = NULL;
        int p = 0;

        check_apply("{{IF name}}Hello", kv, NULL);
        check_apply("{{ELSE}}x{{ENDIF}}", kv, NULL);
        check_apply("x{{ENDIF}}", kv, NULL);
        check_apply("Hello {{name", kv, NULL);
        check_apply("{{IF name}}a{{ELSE}}b{{ELSE}}c{{ENDIF}}", kv, NULL);
        check_apply("{{aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa}}", kv, NULL);

        /* the input is left unchanged on failure */
        tld_strbuf_alloc(&b, 64);
        tld_append(b, "{{IF name}}Hello");
        tld_hash_alloc(&h, 64);
        CHECK(tld_template_apply(b, h) == FAIL);
        CHECK(strcmp(TLD_STR(b), "{{IF name}}Hello") == 0);
        tld_hash_free(h);
        h = NULL;

        CHECK(tld_template_extract_var(b, &h) == FAIL);
        tld_strbuf_clear(b);
        tld_append(b, "Hello {{name");
        CHECK(tld_template_extract_var(b, &h) == FAIL);
        tld_strbuf_clear(b);
        tld_append(b, "{{IF a}}{{b}}{{ELSE}}{{c}}{{ENDIF}}");
        CHECK(tld_template_extract_var(b, &h) == OK);
        {
                char* v = NULL;
                CHECK(h != NULL);
                CHECK(tld_hash_get(h, "a", &v) == OK && v != NULL);
                v = NULL;
                CHECK(tld_hash_get(h, "b", &v) == OK && v != NULL);
                v = NULL;
                CHECK(tld_hash_get(h, "c", &v) == OK && v != NULL);
        }
        tld_hash_free(h);
        tld_strbuf_free(b);

        /* very deep IF nesting fails instead of recursing without bound */
        deep = malloc(20000 * 24 + 16);
        p = 0;
        for(int i = 0; i < 20000; i++){
                memcpy(deep + p, "{{IF name}}", 11);
                p += 11;
        }
        for(int i = 0; i < 20000; i++){
                memcpy(deep + p, "{{ENDIF}}", 9);
                p += 9;
        }
        deep[p] = 0;
        check_apply(deep, kv, NULL);

        /* moderate nesting works */
        p = 0;
        for(int i = 0; i < 100; i++){
                memcpy(deep + p, "{{IF name}}", 11);
                p += 11;
        }
        deep[p++] = 'x';
        for(int i = 0; i < 100; i++){
                memcpy(deep + p, "{{ENDIF}}", 9);
                p += 9;
        }
        deep[p] = 0;
        check_apply(deep, kv, "x");
        free(deep);
}

int main(void)
{
        test_else();
        test_basic();
        test_errors();
        fprintf(stdout, "unit_fix_template: %d checks, %d failed\n", n_check, n_fail);
        return n_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

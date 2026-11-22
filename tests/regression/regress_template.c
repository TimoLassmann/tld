/* regress-template: tests for the template fixes (modernisation). */
#include "tld.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
                const char* v = NULL;
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

/* Apply template t (no keys) with stderr sent to a temp file; return 1 when the
   call fails and what it printed contains want. */
static int fails_saying(const char *t, const char *want)
{
        char name[] = "/tmp/tld_regress_template_XXXXXX";
        char text[4096] = {0};
        tld_strbuf* b = NULL;
        tld_hash* h = NULL;
        int fd = mkstemp(name);
        int save = dup(STDERR_FILENO);
        int r;
        if(fd < 0 || save < 0){
                return 0;
        }
        tld_strbuf_alloc(&b, 64);
        tld_append(b, t);
        tld_hash_alloc(&h, 16);
        fflush(stderr);
        dup2(fd, STDERR_FILENO);
        r = tld_template_apply(b, h);
        fflush(stderr);
        dup2(save, STDERR_FILENO);
        close(save);
        lseek(fd, 0, SEEK_SET);
        if(read(fd, text, sizeof text - 1) < 0){
                text[0] = 0;
        }
        close(fd);
        unlink(name);
        tld_hash_free(h);
        tld_strbuf_free(b);
        if(r != FAIL || strstr(text, want) == NULL){
                fprintf(stderr, "  template [%s]: r=%d, wanted '%s' in:\n%s", t, r, want, text);
                return 0;
        }
        return 1;
}

/* Tags that used to be accepted ({{}} as a variable with no name, {{IF}} as a
   variable called IF, {{IF }} as an IF on "") fail, and every syntax error prints
   its reason (tag errors used to fail with only "Function ... failed"). */
static void test_reasons(void)
{
        CHECK(fails_saying("a{{}}b", "Empty tag {{}} at byte 1"));
        CHECK(fails_saying("{{IF}}x{{ENDIF}}", "IF without a name at byte 0"));
        CHECK(fails_saying("{{IF }}x{{ENDIF}}", "IF without a name at byte 0"));
        CHECK(fails_saying("Hello {{name", "Tag at byte 6 is not closed with }}"));
        CHECK(fails_saying("{{aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa}}",
                           "Tag at byte 0 is longer than 63 characters"));
        CHECK(fails_saying("x{{ENDIF}}", "ENDIF without matching IF at byte 1"));
        CHECK(fails_saying("{{ELSE}}", "ELSE without matching IF at byte 0"));
        CHECK(fails_saying("{{IF a}}{{ELSE}}{{ELSE}}{{ENDIF}}", "ELSE without matching IF at byte 16"));
        CHECK(fails_saying("{{IF a}}{{IF b}}x{{ENDIF}}", "IF without ENDIF (1 still open at the end)"));
}

/* The checks, in the order a plain run does them; arguments select by name. */
static const struct {
        const char *name;
        void (*fn)(void);
        const char *about;
} checks[] = {
        {"else", test_else, "{{IF}} ... {{ELSE}} ... {{ENDIF}} branches"},
        {"basic", test_basic, "{{key}} substitution, missing keys, plain text"},
        {"errors", test_errors, "malformed templates fail cleanly"},
        {"reasons", test_reasons, "empty and nameless tags fail; each syntax error says why"},
};

int main(int argc, char *argv[])
{
        const int n_checks = (int) (sizeof checks / sizeof checks[0]);
        test_init(&argc, argv, "[CHECK ...]",
                  "Checks for the template fixes: {{IF}}/{{ELSE}}/{{ENDIF}}, substitution, and\n"
                  "templates that must fail.\n"
                  "\n"
                  "CHECK names (default: all, in this order):\n"
                  "  else    {{IF}} ... {{ELSE}} ... {{ENDIF}} branches\n"
                  "  basic   {{key}} substitution, missing keys, plain text\n"
                  "  errors  malformed templates fail cleanly\n"
                  "  reasons empty and nameless tags fail; each syntax error says why\n"
                  "\n"
                  "Example: regress-template else errors");
        for(int i = 1; i < argc; i++){
                int k = 0;
                while(k < n_checks && strcmp(argv[i], checks[k].name) != 0){
                        k++;
                }
                if(k == n_checks){
                        fprintf(stderr, "regress-template: no check named '%s' (see -h)\n", argv[i]);
                        return 2;
                }
        }
        for(int k = 0; k < n_checks; k++){
                int want = argc == 1;
                int c0 = n_check;
                int f0 = n_fail;
                for(int i = 1; i < argc; i++){
                        want |= strcmp(argv[i], checks[k].name) == 0;
                }
                if(!want){
                        continue;
                }
                SECTION_MSG("%s: %s", checks[k].name, checks[k].about);
                checks[k].fn();
                if(n_fail == f0){
                        OK_MSG("%s: %d checks passed", checks[k].name, n_check - c0);
                }
        }
        if(n_fail){
                fprintf(stderr, "regress-template: %d checks, %d failed\n", n_check, n_fail);
                return EXIT_FAILURE;
        }
        KV_MSG("total", "%d checks, 0 failed", n_check);
        return EXIT_SUCCESS;
}

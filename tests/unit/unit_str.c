#include "tld.h"
#include "test.h"
#include <stdio.h>

static int checks(void)
{
        char test[] = "Hello";
        char test2[] = "GIRAFFE";
        tld_strbuf* b = NULL;
        SECTION_MSG("append and prepend");
        RUN(tld_strbuf_alloc(&b, 10));
        for(int i = 0; i < 5; i++){
                RUN(tld_append(b, test));
                DEBUG_MSG("after append %d: len %d", i + 1, b->len);
        }
        RUN(tld_prepend(b, test2));
        RUN(tld_append(b, test2));
        KV_MSG("result", "%s", TLD_STR(b));
        KV_MSG("length", "%d", TLD_STRLEN(b));
        ASSERT(TLD_STRLEN(b) == 39, "length %d, expected 39", TLD_STRLEN(b));
        OK_MSG("buffer grew from 10 to %d bytes", b->alloc_len);
        tld_strbuf_free(b);
        return OK;
ERROR:
        tld_strbuf_free(b);
        return FAIL;
}

/* unit-str TEXT [AFFIX ...]: the string functions on the user's text. */
static int demo(int n, char **args)
{
        tld_strbuf *b = NULL;
        tld_strbuf *c = NULL;
        SECTION_MSG("tld_strbuf on '%s'", args[0]);
        RUN(tld_strbuf_alloc(&b, 4));
        for(const char *p = args[0]; *p; p++){
                RUN(tld_append_char(b, *p));
        }
        DEBUG_MSG("appended char by char: len %d, allocated %d", b->len, b->alloc_len);
        KV_MSG("length", "%d", TLD_STRLEN(b));
        RUN(tld_strbuf_alloc(&c, 4));
        RUN(tld_strbuf_copy(c, b));
        RUN(tld_prepend(c, "<<"));
        RUN(tld_append(c, ">>"));
        fprintf(stdout, "text:      %s\n", TLD_STR(b));
        fprintf(stdout, "length:    %d\n", TLD_STRLEN(b));
        fprintf(stdout, "wrapped:   %s\n", TLD_STR(c));
        tld_str whole = tld_strbuf_to_str(*b);
        for(int i = 1; i < n; i++){
                tld_str a = tld_char_to_str(args[i]);
                fprintf(stdout, "'%s': prefix %s, suffix %s\n", args[i],
                        tld_prefix_match(whole, a) ? "yes" : "no",
                        tld_suffix_match(whole, a) ? "yes" : "no");
        }
        OK_MSG("done");
        tld_strbuf_free(b);
        tld_strbuf_free(c);
        return OK;
ERROR:
        tld_strbuf_free(b);
        tld_strbuf_free(c);
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[TEXT [AFFIX ...]]",
                  "With no arguments: checks tld_append/tld_prepend on a growing buffer.\n"
                  "With TEXT: builds a tld_strbuf from it, prints it, its length and a\n"
                  "prepended/appended copy; for each AFFIX says whether it is a prefix or a\n"
                  "suffix of TEXT (tld_prefix_match, tld_suffix_match).\n"
                  "Example: unit-str \"hello world\" hello world lo");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
        } else {
                RUN(checks());
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

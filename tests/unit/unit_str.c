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
                RUN(tld_strbuf_append(b, test));
                DEBUG_MSG("after append %d: len %d", i + 1, b->len);
        }
        RUN(tld_strbuf_prepend(b, test2));
        RUN(tld_strbuf_append(b, test2));
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

/* unit-str TEXT: the string functions on the user's text. */
static int demo(const char *text)
{
        tld_strbuf *b = NULL;
        tld_strbuf *c = NULL;
        SECTION_MSG("tld_strbuf on '%s'", text);
        RUN(tld_strbuf_alloc(&b, 4));
        for(const char *p = text; *p; p++){
                RUN(tld_strbuf_append_char(b, *p));
        }
        DEBUG_MSG("appended char by char: len %d, allocated %d", b->len, b->alloc_len);
        KV_MSG("length", "%d", TLD_STRLEN(b));
        RUN(tld_strbuf_alloc(&c, 4));
        RUN(tld_strbuf_copy(c, b));
        RUN(tld_strbuf_prepend(c, "<<"));
        RUN(tld_strbuf_append(c, ">>"));
        fprintf(stdout, "text:      %s\n", TLD_STR(b));
        fprintf(stdout, "length:    %d\n", TLD_STRLEN(b));
        fprintf(stdout, "wrapped:   %s\n", TLD_STR(c));
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
        test_init(&argc, argv, "[TEXT]",
                  "With no arguments: checks tld_strbuf_append/tld_strbuf_prepend on a growing buffer.\n"
                  "With TEXT: builds a tld_strbuf from it, prints it, its length and a\n"
                  "prepended/appended copy.\n"
                  "Example: unit-str \"hello world\"");
        if(argc > 1){
                RUN(demo(argv[1]));
        } else {
                RUN(checks());
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

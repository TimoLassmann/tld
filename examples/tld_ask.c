/* examples/tld_ask.c - ask the tld manuals a question in plain language (tld_doc.h) and
 * print the answer written for a coding agent.
 * The program is called tld-ask (the build makes build/tests/tld-ask):
 *   cc -std=c11 -I. examples/tld_ask.c -o tld-ask -lpthread -lm
 *   ./tld-ask "how do I read a JSON string?"
 *   ./tld-ask -b 1500 -n 3 "levenshtein distance between two strings"
 * -b BYTES: the byte budget of the answer (default 4096); -n N: the entries retrieved
 * (default 8). Several arguments are joined with spaces. The answer goes to stdout.
 * Exit codes: 0 answered, 1 error, 2 usage. */
#define TLD_IMPLEMENTATION
#define TLD_DOC_IMPLEMENTATION
#include "tld_doc.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(FILE *f)
{
        fprintf(f, "Usage: tld-ask [-b BYTES] [-n N] \"question\"\n"
                "  -b BYTES   byte budget of the answer (default 4096)\n"
                "  -n N       entries retrieved (default 8)\n");
}

/* The positive number in s (at most max), or -1. */
static long positive(const char *s, long max)
{
        char *end = NULL;
        long v = strtol(s, &end, 10);
        return end == s || *end || v <= 0 || v > max ? -1 : v;
}

int main(int argc, char *argv[])
{
        tld_doc_opts o = {0};
        tld_doc_answer *a = NULL;
        tld_strbuf *q = NULL;
        long v;
        int c;
        while((c = getopt(argc, argv, "b:n:h")) != -1){
                switch(c){
                case 'b':
                        if((v = positive(optarg, 1000000000L)) < 0){
                                fprintf(stderr, "tld-ask: -b needs a positive number of bytes\n");
                                return 2;
                        }
                        o.max_bytes = (int)v;
                        break;
                case 'n':
                        if((v = positive(optarg, 1000000L)) < 0){
                                fprintf(stderr, "tld-ask: -n needs a positive number of entries\n");
                                return 2;
                        }
                        o.top_n = (int)v;
                        break;
                case 'h':
                        usage(stdout);
                        return 0;
                default:
                        usage(stderr);
                        return 2;
                }
        }
        if(optind >= argc){
                usage(stderr);
                return 2;
        }
        RUN(tld_strbuf_alloc(&q, 256));
        for(int i = optind; i < argc; i++){
                RUN(tld_strbuf_append(q, argv[i]));
                if(i + 1 < argc){
                        RUN(tld_strbuf_append_char(q, ' '));
                }
        }
        RUN(tld_doc_ask(TLD_STR(q), &o, &a));
        fputs(a->text, stdout);
        tld_doc_answer_free(a);
        tld_strbuf_free(q);
        return 0;
ERROR:
        tld_doc_answer_free(a);
        tld_strbuf_free(q);
        return 1;
}

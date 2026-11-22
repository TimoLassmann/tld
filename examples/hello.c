/* examples/hello.c - smallest check that tld_ai.h reaches your Ollama server.
 *   cc -std=c11 -I. examples/hello.c -o hello -lpthread -lm
 *   ./hello                          # uses the default model (inkling-2-mini:latest)
 *   ./hello "Why is the sky blue?"   # your own prompt
 *   ./hello -vv                      # also each request (endpoint, model, host)
 *   TLD_AI_LLM=other:latest ./hello  # or pick a model; TLD_AI_HOST=http://host:11434 for
 *                                    # another machine
 * The answer goes to stdout; everything else goes to stderr, so ./hello > answer.txt
 * keeps only the answer. */
#define _POSIX_C_SOURCE 200809L /* setenv */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(FILE *f, const char *prog) {
    fprintf(f, "Usage: %s [-h] [-v] [PROMPT]\n\n"
               "Sends PROMPT (default: a question about C) to TLD_AI_LLM on TLD_AI_HOST and\n"
               "prints the answer on stdout; host, model, time and tokens go to stderr.\n\n"
               "Options:\n"
               "  -h, --help      show this help and exit\n"
               "  -v, --verbose   -vv also shows each request (TLD_VERBOSE=2)\n\n"
               "TLD_VERBOSE=0 leaves only the answer and errors.\n",
            prog);
}

int main(int argc, char **argv) {
    static struct option opts[] = {{"help", no_argument, 0, 'h'},
                                   {"verbose", no_argument, 0, 'v'},
                                   {0, 0, 0, 0}};
    int c, verbose = 0;
    while ((c = getopt_long(argc, argv, "hv", opts, NULL)) != -1) {
        switch (c) {
        case 'h': usage(stdout, argv[0]); return 0;
        case 'v': verbose++; break;
        default: usage(stderr, argv[0]); return 2;
        }
    }
    if (verbose) setenv("TLD_VERBOSE", verbose >= 2 ? "2" : "1", 1);
    const char *prompt = optind < argc ? argv[optind]
                                       : "Reply with one short sentence: what is the C programming language?";

    SECTION_MSG("tld_ai hello");
    KV_MSG("prompt", "%s", prompt);
    const char *s = ai_text("%s", prompt);
    const ai_info *in = ai_last();
    KV_MSG("host", "%s", in->host ? in->host : "?");
    KV_MSG("model", "%s", in->model ? in->model : "?");
    if (!s) {
        fprintf(stderr, "hello: %s\n", ai_error());
        return 1;
    }
    if (in->from_cache)
        OK_MSG("answered from the cache");
    else
        OK_MSG("answered in %.1f s · %ld prompt + %ld output tokens", in->seconds,
               in->prompt_tokens, in->output_tokens);
    printf("%s\n", s);
    return 0;
}

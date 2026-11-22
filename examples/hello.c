/* examples/hello.c - smallest check that tld_ai.h reaches your Ollama server.
 *   cc -std=c11 -I. examples/hello.c -o hello -lpthread -lm
 *   ./hello                          # uses the default model (inkling-2-mini:latest)
 *   TLD_AI_LLM=other:latest ./hello  # or pick a model; TLD_AI_HOST=http://host:11434 for
 *                                    # another machine; TLD_VERBOSE=2 shows each request
 * The answer goes to stdout; everything else goes to stderr, so ./hello > answer.txt
 * keeps only the answer. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include <stdio.h>

int main(void) {
    const char *prompt = "Reply with one short sentence: what is the C programming language?";

    SECTION_MSG("tld_ai hello");
    KV_MSG("prompt", "%s", prompt);
    char *s = ai_text("%s", prompt);
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

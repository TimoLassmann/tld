/* examples/hello.c - smallest check that tld_ai.h reaches your Ollama server.
 *   cc -std=c11 -I. examples/hello.c -o hello -lpthread -lm
 *   ./hello                       # uses the default model (inkling-2-mini:latest)
 *   AI_LLM=other:latest ./hello   # or pick a model; AI_HOST=http://host:11434 for another machine */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include <stdio.h>

int main(void) {
    char *s = ai_text("Reply with one short sentence: what is the C programming language?");
    if (!s) {
        fprintf(stderr, "hello: %s\n", ai_error());
        return 1;
    }
    printf("%s\n", s);
    return 0;
}

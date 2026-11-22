/* tests/ai/text.c - one ai_text call with AI_LLM; prints the reply, or the status name
 * and message on stderr. Exit 0 on success, 1 on failure. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "ai.h"
#include "status_name.h"
#include <stdio.h>

int main(int argc, char **argv) {
    char *s = ai_text("%s", argc > 1 ? argv[1] : "Say hello");
    if (!s) {
        fprintf(stderr, "text: %s: %s\n", status_name(ai_status()), ai_error());
        return 1;
    }
    printf("%s\n", s);
    return 0;
}

/* tests/ai/text.c - one ai_text call with TLD_AI_LLM; prints the reply, or the status name
 * and message on stderr. Exit 0 on success, 1 on failure. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include "test.h"
#include "status_name.h"
#include "show.h"
#include <stdio.h>

int main(int argc, char **argv) {
    test_init_keep(&argc, argv, "[PROMPT]",
                   "One ai_text call with TLD_AI_LLM; prints the reply on stdout, or the status name and\n"
                   "message on stderr (exit 1). PROMPT defaults to \"Say hello\". run.sh points TLD_AI_HOST\n"
                   "at the mock or a closed port; without it this asks the local Ollama server.\n"
                   "Example: TLD_AI_HOST=http://127.0.0.1:9 ai-text hi");
    const char *prompt = argc > 1 ? argv[1] : "Say hello";
    SECTION_MSG("ai_text");
    KV_MSG("prompt", "%s", prompt);
    const char *s = ai_text("%s", prompt);
    show_call("ai_text", s != NULL);
    if (!s) {
        fprintf(stderr, "text: %s: %s\n", status_name(ai_status()), ai_error());
        return 1;
    }
    printf("%s\n", s);
    return 0;
}

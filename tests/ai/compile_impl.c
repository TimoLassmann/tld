/* tests/ai/compile_impl.c - TU 1 of 2: tld_ai.h and tld.h implementations, compiled with
 * -std=c11 -Wall -Wextra -pedantic -Werror. Links with compile_use.c. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include "tld_ai.h" /* a second include must be harmless */
#include <stdio.h>
#include <string.h>

int compile_use(void);

int main(void) {
    const char *s = ai_fmt("%d-%s", 7, "x");
    if (!s || strcmp(s, "7-x")) return 1;
    /* TLD_AI_HOST points at a closed port in the test, so this fails fast */
    if (ai_text("hi") != NULL || ai_status() != AI_ENOSERVER) return 1;
    if (compile_use()) return 1;
    ai_reset();
    puts("compile: ok");
    return 0;
}

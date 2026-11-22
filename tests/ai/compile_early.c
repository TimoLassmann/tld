/* tests/ai/compile_early.c - the implementation after system headers that were
 * included in strict mode with _POSIX_C_SOURCE already set (so ai.h's own feature
 * macros come too late). Must still compile without warnings and work. tld.h's own
 * implementation is in compile_tld.c: it is not what this file tests. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/file.h>
#define AI_IMPLEMENTATION
#include "ai.h"

int main(void) {
    if (ai_embed("x").x != NULL || ai_status() != AI_ENOSERVER) return 1;
    puts("compile-early: ok");
    return 0;
}

/* tests/ai/opts.c - every option reaches the request body (run.sh checks the cache). */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "ai.h"
#include <stdio.h>

int main(int argc, char **argv) {
    const char *img = argc > 1 ? argv[1] : "tests/ai/img.png";
    const char *stops[] = {"END", NULL}, *imgs[] = {img, NULL};
    char *s = ai_text_with(AI_OPTS(.model = "inkling-2-mini", .system = "Be brief.",
                                   .temperature = AI_F(0), .seed = AI_I(7), .stop = stops,
                                   .think = "high", .keep_alive = "10m", .images = imgs,
                                   .extra_options = "{\"repeat_penalty\":1.1}"),
                           "Describe %s", "the image");
    if (!s) {
        fprintf(stderr, "opts: status %d: %s\n", ai_status(), ai_error());
        return 1;
    }
    printf("%s | tokens %ld/%ld cache %d\n", s, ai_last()->prompt_tokens,
           ai_last()->output_tokens, ai_last()->from_cache);
    return 0;
}

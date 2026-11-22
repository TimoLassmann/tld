/* tests/ai/opts.c - every option reaches the request body (run.sh checks the cache). */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include "test.h"
#include "show.h"
#include <stdio.h>

int main(int argc, char **argv) {
    test_init_keep(&argc, argv, "[IMAGE]",
                   "One ai_text_with call with every option set (model, system, temperature, seed,\n"
                   "stop, think, keep_alive, images, extra_options); run.sh checks the request body in\n"
                   "the cache. IMAGE defaults to tests/ai/img.png. Needs TLD_AI_HOST pointing at the mock.\n"
                   "Example: TLD_AI_HOST=http://127.0.0.1:PORT ai-opts tests/ai/img.png");
    const char *img = argc > 1 ? argv[1] : "tests/ai/img.png";
    const char *stops[] = {"END", NULL}, *imgs[] = {img, NULL};
    SECTION_MSG("ai_text_with, every option set");
    KV_MSG("image", "%s", img);
    const char *s = ai_text_with(AI_OPTS(.model = "inkling-2-mini", .system = "Be brief.",
                                   .temperature = AI_F(0), .seed = AI_I(7), .stop = stops,
                                   .think = "high", .keep_alive = "10m", .images = imgs,
                                   .extra_options = "{\"repeat_penalty\":1.1,\"seed\":7}"),
                           "Describe %s", "the image");
    show_call("ai_text_with", s != NULL);
    if (!s) {
        fprintf(stderr, "opts: status %d: %s\n", ai_status(), ai_error());
        return 1;
    }
    printf("%s | tokens %ld/%ld cache %d\n", s, ai_last()->prompt_tokens,
           ai_last()->output_tokens, ai_last()->from_cache);
    return 0;
}

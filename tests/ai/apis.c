/* tests/ai/apis.c EXPECTED [MAX_SECONDS] - calls every API that needs the server and
 * checks that each returns its failure value with ai_status() == EXPECTED (a name such
 * as AI_ENOSERVER), and that the whole run takes at most MAX_SECONDS (default 3). */
#define _POSIX_C_SOURCE 200809L
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "ai.h"
#include "status_name.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define T2(X) X(alpha, "First") X(beta, "Second")
AI_CHOICE(pick, "Which one?", T2)
#define L3(X) X(low, "Low") X(mid, "Mid") X(high, "High")
AI_SCALE(level, "How much?", L3)
#define F(X) X(str, title, "Title") X(int, n, "Count")
AI_STRUCT(rec, F)

static int bad;
static const char *want;

static void check(const char *api, int failed) {
    const char *got = status_name(ai_status());
    int ok = failed && !strcmp(got, want) && ai_error()[0];
    printf("%s  %-18s %s: %s\n", ok ? "ok  " : "BAD ", api, got, ai_error());
    if (!ok) bad = 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: apis EXPECTED_STATUS [MAX_SECONDS]\n");
        return 2;
    }
    want = argv[1];
    double max = argc > 2 ? atof(argv[2]) : 3.0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    const ai_opts *o = AI_OPTS(.temperature = AI_F(0));
    const char *texts[] = {"a", "b"};
    struct rec r;

    check("ai_text", ai_text("hello %d", 1) == NULL);
    check("ai_text_with", ai_text_with(o, "hello") == NULL);
    check("ai_yesno", ai_yesno("Yes?", "state") == -1);
    check("ai_yesno_with", ai_yesno_with(o, "Yes?", "state") == -1);
    check("ai_fill", ai_fill(&r, rec, "Make %s", "one") == -1 && !r.title && !r.n);
    check("ai_fill_with", ai_fill_with(AI_OPTS(.max_retries = AI_I(0)), &r, rec, "x") == -1);
    check("ai_choose", !ai_choose(pick, "state").ok);
    check("ai_choose_with", !ai_choose_with(o, pick, "state").ok);
    check("ai_rate", !ai_rate(level, "state").ok);
    check("ai_rate_with", !ai_rate_with(o, level, "state").ok);
    check("ai_embed", ai_embed("text").x == NULL);
    check("ai_embed_with", ai_embed_with(o, "text").x == NULL);
    check("ai_embed_all", ai_embed_all(texts, 2).x == NULL);
    check("ai_embed_all_with", ai_embed_all_with(o, texts, 2).x == NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double s = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("%s  all calls took %.2f s (limit %.1f)\n", s <= max ? "ok  " : "BAD ", s, max);
    return bad || s > max;
}

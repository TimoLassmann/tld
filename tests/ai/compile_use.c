/* tests/ai/compile_use.c - TU 2 of 2: declarations only, every macro expanded. */
#include "ai.h"

#define TEAM(X) X(red, "Red team") X(blue, "Blue team")
AI_CHOICE(team, "Which team?", TEAM)
#define SIZE(X) X(small, "Small") X(large, "Large")
AI_SCALE(size, "How big?", SIZE)
#define FIX(X) X(str, title, "Title") X(num, risk, "Risk") X(int, n, "N") X(bool, b, "B")
AI_STRUCT(fix, FIX)

int compile_use(void) {
    float a[3] = {1, 0, 0}, b[6] = {1, 0, 0, 0, 1, 0};
    ai_vec q = {3, a};
    ai_vecs m = {2, 3, b};
    int idx[2];
    double sc[2];
    if (ai_nearest(q, m, 2, idx, sc) != 2 || idx[0] != 0) return 1;
    if (ai_cosine(q, ai_row(m, 1)) != 0) return 1;
    struct fix f;
    struct team_answer t = ai_choose_with(AI_OPTS(.seed = AI_I(1)), team, "s");
    struct size_answer z = ai_rate(size, "s");
    int r = ai_fill_with(AI_OPTS(.max_retries = AI_I(0)), &f, fix, "%s", "x");
    const ai_info *li = ai_last();
    return (t.ok || z.ok || r == 0 || !li || ai_status() != AI_ENOSERVER) ? 1 : 0;
}

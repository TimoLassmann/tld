/* tests/ai/dyn.c - descriptions built at run time: ai_fill_dyn, ai_choose_dyn and
 * ai_rate_dyn against tests/ai/mock_ollama.py. Checks that each gives the same answer as
 * its macro twin (AI_STRUCT / AI_CHOICE / AI_SCALE), that ai_fill_dyn returns exactly the
 * described members as typed JSON, and that bad descriptions fail with AI_EARG before
 * any request. Needs TLD_AI_HOST at the mock. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include "test.h"
#include "show.h"
#include <stdio.h>
#include <string.h>

#define FIX(X) X(str, title, "One-line summary") X(num, risk, "Risk 0 to 1") \
               X(int, steps, "Number of steps") X(bool, db, "Schema change")
AI_STRUCT(fix, FIX)
#define TEAM(X) X(bug, "A software defect") X(billing, "Payments and refunds")
AI_CHOICE(team, "Which team should handle this ticket?", TEAM)
#define URG(X) X(l0, "none") X(l1, "someday") X(l2, "routine") X(l3, "soon") \
               X(l4, "urgent") X(l5, "critical")
AI_SCALE(urg, "How urgent is this ticket?", URG)

static const char *TICKET = "The app crashes on start and I want a refund.";
static int bad;

static void expect(const char *what, int ok, const char *detail) {
    show_call(what, ok);
    if (!ok) {
        fprintf(stderr, "dyn: %s: %s\n", what, detail ? detail : ai_error());
        bad = 1;
    }
}

static void expect_earg(const char *what, int failed) {
    int ok = failed && ai_status() == AI_EARG && ai_error()[0];
    expect(what, ok, ai_error());
}

int main(int argc, char **argv) {
    test_init_keep(&argc, argv, "",
                   "Calls ai_fill_dyn, ai_choose_dyn and ai_rate_dyn and their macro twins against\n"
                   "the mock and compares the answers; bad descriptions must fail with AI_EARG.\n"
                   "Example: TLD_AI_HOST=http://127.0.0.1:PORT ai-dyn");

    SECTION_MSG("ai_fill_dyn");
    ai_field f[] = {{.name = "title", .kind = AI_T_str, .desc = "One-line summary"},
                    {.name = "risk", .kind = AI_T_num, .desc = "Risk 0 to 1"},
                    {.name = "steps", .kind = AI_T_int, .desc = "Number of steps"},
                    {.name = "db", .kind = AI_T_bool, .desc = "Schema change"}};
    ai_type t = {.name = "fix", .f = f, .n = 4};
    const char *j = ai_fill_dyn(&t, "Propose a fix for:\n%s", TICKET);
    expect("ai_fill_dyn returns JSON", j != NULL, NULL);
    tld_json_obj *o = NULL;
    const char *s = NULL;
    double d = 0;
    int64_t v = 0;
    int b = 0;
    int typed = j && tld_json_parse(j, &o) == OK && tld_json_get_str(o, "title", &s) == OK &&
                tld_json_get_num(o, "risk", &d) == OK && tld_json_get_int(o, "steps", &v) == OK &&
                tld_json_get_bool(o, "db", &b) == OK;
    expect("ai_fill_dyn: every member, typed", typed, j);
    expect("ai_fill_dyn: members in order, nothing else",
           j && !strcmp(j, "{\"title\":\"mock title\",\"risk\":0.5,\"steps\":1,\"db\":true}"), j);
    tld_json_obj_free(o);

    struct fix m;
    int rc = ai_fill_with(AI_OPTS(.fresh = 1), &m, fix, "Propose a fix for:\n%s", TICKET);
    expect("ai_fill (macro twin)", rc == 0 && m.title && !strcmp(m.title, "mock title"), NULL);

    const char *j2 = ai_fill_dyn_with(AI_OPTS(.max_retries = AI_I(0), .system = "Be brief."), &t,
                                      "%s", TICKET);
    expect("ai_fill_dyn_with", j2 != NULL, NULL);

    SECTION_MSG("ai_choose_dyn and ai_rate_dyn");
    ai_label tl[] = {{"bug", "A software defect"}, {"billing", "Payments and refunds"}};
    ai_labels tc = {.name = "team", .question = "Which team should handle this ticket?",
                    .labels = tl, .n = 2};
    ai_pick p = ai_choose_dyn(&tc, TICKET);
    struct team_answer pm = ai_choose(team, TICKET);
    expect("ai_choose_dyn answers", p.ok && p.p, NULL);
    expect("ai_choose_dyn matches ai_choose",
           p.ok && pm.ok && p.value == (int)pm.value && p.confidence == pm.confidence &&
               p.p[0] == pm.p[0] && p.p[1] == pm.p[1],
           "answers differ");
    expect("ai_choose_dyn_with", ai_choose_dyn_with(AI_OPTS(.fresh = 1), &tc, TICKET).ok, NULL);

    ai_label ul[] = {{"l0", "none"}, {"l1", "someday"}, {"l2", "routine"},
                     {"l3", "soon"}, {"l4", "urgent"}, {"l5", "critical"}};
    ai_labels uc = {.name = "urg", .question = "How urgent is this ticket?", .labels = ul, .n = 6};
    ai_rating r = ai_rate_dyn(&uc, TICKET);
    struct urg_answer rm = ai_rate(urg, TICKET);
    expect("ai_rate_dyn matches ai_rate",
           r.ok && rm.ok && r.level == (int)rm.level && r.score == rm.score &&
               r.confidence == rm.confidence,
           "answers differ");
    expect("ai_rate_dyn_with", ai_rate_dyn_with(AI_OPTS(.fresh = 1), &uc, TICKET).ok, NULL);
    ai_labels u5 = uc;
    u5.n = 5;
    ai_rating r5 = ai_rate_dyn(&u5, TICKET);
    expect("ai_rate_dyn: a count the model rejects is AI_EHTTP",
           !r5.ok && ai_status() == AI_EHTTP && strstr(ai_error(), "exactly 6"), ai_error());

    SECTION_MSG("bad descriptions fail with AI_EARG");
    ai_field dup[] = {{.name = "a", .kind = AI_T_str}, {.name = "a", .kind = AI_T_num}};
    ai_field dot[] = {{.name = "a.b", .kind = AI_T_str}};
    ai_field kind[] = {{.name = "a", .kind = (ai_kind)9}};
    ai_field nul[] = {{.name = NULL, .kind = AI_T_str}};
    expect_earg("fill: NULL type", ai_fill_dyn(NULL, "x") == NULL);
    ai_type t0 = {.name = "t", .f = f, .n = 0}, tn = {.name = "", .f = f, .n = 1},
            td = {.name = "t", .f = dup, .n = 2}, tp = {.name = "t", .f = dot, .n = 1},
            tk = {.name = "t", .f = kind, .n = 1}, tz = {.name = "t", .f = nul, .n = 1};
    expect_earg("fill: no members", ai_fill_dyn(&t0, "x") == NULL);
    expect_earg("fill: empty type name", ai_fill_dyn(&tn, "x") == NULL);
    expect_earg("fill: member twice", ai_fill_dyn(&td, "x") == NULL);
    expect_earg("fill: '.' in a name", ai_fill_dyn(&tp, "x") == NULL);
    expect_earg("fill: unknown kind", ai_fill_dyn(&tk, "x") == NULL);
    expect_earg("fill: NULL member name", ai_fill_dyn(&tz, "x") == NULL);

    ai_label twice[] = {{"x", "one"}, {"x", "two"}};
    ai_label nodesc[] = {{"x", "one"}, {"y", NULL}};
    ai_label sp[] = {{"x", "one"}, {"y z", "two"}};
    expect_earg("choose: NULL labels", !ai_choose_dyn(NULL, TICKET).ok);
    expect_earg("choose: one label", !ai_choose_dyn(&(ai_labels){"c", "Q?", tl, 1}, TICKET).ok);
    expect_earg("choose: NULL question", !ai_choose_dyn(&(ai_labels){"c", NULL, tl, 2}, TICKET).ok);
    expect_earg("choose: bad name", !ai_choose_dyn(&(ai_labels){"a[0]", "Q?", tl, 2}, TICKET).ok);
    expect_earg("choose: label twice", !ai_choose_dyn(&(ai_labels){"c", "Q?", twice, 2}, TICKET).ok);
    expect_earg("choose: NULL description", !ai_choose_dyn(&(ai_labels){"c", "Q?", nodesc, 2}, TICKET).ok);
    expect_earg("rate: space in a label", !ai_rate_dyn(&(ai_labels){"c", "Q?", sp, 2}, TICKET).ok);
    expect_earg("choose: NULL state", !ai_choose_dyn(&tc, NULL).ok);

    puts(bad ? "dyn: FAILED" : "dyn: ok");
    return bad;
}

/* oom-ai: allocation-failure sweep over tld_ai.h (see oom.h), against the mock server:
 * run it through tests/ai/run.sh oom, which starts tests/ai/mock_ollama.py and points
 * TLD_AI_HOST at it. Every call uses the cache, whose write is best effort: when only
 * that fails the call succeeds and warns, which is not a finding. */
#include "oom.h"
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"

#define TEAM(X) X(bug, "A software defect") X(billing, "Payments and refunds")
AI_CHOICE(team, "Which team should handle this ticket?", TEAM)
#define URG(X) X(l0, "none") X(l1, "someday") X(l2, "routine") X(l3, "soon") X(l4, "urgent") X(l5, "critical")
AI_SCALE(urg, "How urgent is this ticket?", URG)
#define FIX(X) X(str, title, "One-line summary") X(str, steps, "What to change") X(num, risk, "Risk 0 to 1") X(bool, needs_db_change, "Schema change")
AI_STRUCT(fix, FIX)

static const char *TICKET = "The app crashes on start since the update";

static int sc_text(void)
{
        const char *s = ai_text("Summarise: %s", TICKET);
        int ok = s != NULL && s[0] != 0;
        ai_reset();
        return ok ? 0 : 1;
}
static int sc_text_opts(void)
{
        const char *stop[] = {"END", NULL};
        const char *s = ai_text_with(AI_OPTS(.system = "Be brief.", .temperature = AI_F(0), .seed = AI_I(7), .stop = stop,
                                             .think = "high", .keep_alive = "10m", .extra_options = "{\"repeat_penalty\": 1.1}"),
                                     "Summarise: %s", TICKET);
        int ok = s != NULL;
        ai_reset();
        return ok ? 0 : 1;
}
static int sc_choose(void)
{
        struct team_answer t = ai_choose(team, TICKET);
        ai_reset();
        return t.ok ? 0 : 1;
}
static int sc_rate(void)
{
        struct urg_answer u = ai_rate(urg, TICKET);
        ai_reset();
        return u.ok ? 0 : 1;
}
static int sc_yesno(void)
{
        double p = ai_yesno("Is a refund requested?", "{\"ticket\": \"refund please\", \"n\": 2}");
        ai_reset();
        return p >= 0 ? 0 : 1;
}
static int sc_fill(void)
{
        struct fix f;
        int rc = ai_fill_with(AI_OPTS(.max_retries = AI_I(0)), &f, fix, "Propose a fix for: %s", TICKET);
        int ok = rc == 0 && f.title != NULL;
        ai_reset();
        return ok ? 0 : 1;
}
static int sc_embed(void)
{
        const char *texts[3] = {"one", "two", "three"};
        ai_vecs m = ai_embed_all(texts, 3);
        int ok = m.x != NULL;
        ai_reset();
        return ok ? 0 : 1;
}
static int sc_review(void)
{
        const char *a = ai_review("Which team?", TICKET);
        int ok = a != NULL || ai_status() == AI_OK;
        ai_reset();
        return ok ? 0 : 1;
}

#define CACHE_WARNING "cannot write a cache entry: out of memory"

static const struct oom_scenario scenarios[] = {
        {"text", sc_text, "ai_text", CACHE_WARNING},
        {"text_opts", sc_text_opts, "ai_text_with every option, extra_options", CACHE_WARNING},
        {"choose", sc_choose, "ai_choose (AI_CHOICE)", CACHE_WARNING},
        {"rate", sc_rate, "ai_rate (AI_SCALE)", CACHE_WARNING},
        {"yesno", sc_yesno, "ai_yesno with a JSON state", CACHE_WARNING},
        {"fill", sc_fill, "ai_fill_with (AI_STRUCT)", CACHE_WARNING},
        {"embed", sc_embed, "ai_embed_all", CACHE_WARNING},
        {"review", sc_review, "ai_review", NULL},
};

int main(int argc, char *argv[])
{
        if(!getenv("TLD_AI_HOST")){
                fprintf(stderr, "oom-ai: run through tests/ai/run.sh oom (needs the mock server)\n");
                return 2;
        }
        /* The cache warning is the evidence that a failed allocation was reported; it is a
           tld.h WARNING line, which the quiet test default (TLD_VERBOSE=0) would hide. */
        setenv("TLD_VERBOSE", "1", 0);
        return oom_main(argc, argv, scenarios, (int) (sizeof scenarios / sizeof scenarios[0]), "tld_ai.h");
}

/* tests/ai/defaults.c - ai_defaults(), ai_effective(), ai_setting_source() (PRD 6.10).
 *
 *   defaults set HOST DB REVIEWS SRC   set every default, call each endpoint, check the
 *                                      in-program rules; SRC is the expected source of
 *                                      the string settings ("defaults", or "env" when
 *                                      run.sh sets the AI_* variables)
 *   defaults timeout HOST              a 1 s default timeout against a slow mock
 *   defaults stress HOST               8 threads calling while defaults change (TSan)
 *
 * run.sh checks the logged requests (models per endpoint, options, dimensions). */
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

static int bad;

static void expect(int cond, const char *what) {
    printf("%s  %s", cond ? "ok  " : "BAD ", what);
    if (!cond) printf("  [status %s: %s]", status_name(ai_status()), ai_error());
    printf("\n");
    if (!cond) bad = 1;
}

static void show(void) {
    static const char *names[] = {"host", "llm", "decider", "embedder", "db", "reviews",
                                  "num_ctx", "timeout", "fresh", "guard",
                                  "embed_dimensions", "opts"};
    ai_config c;
    if (ai_effective(&c)) return;
    printf("effective: host=%s llm=%s decider=%s embedder=%s db=%s reviews=%s timeout=%ld "
           "dims=%ld guard=%ld\n",
           c.host, c.llm, c.decider, c.embedder, c.db, c.reviews, c.timeout.v,
           c.embed_dimensions.set ? c.embed_dimensions.v : 0, c.guard.v);
    printf("sources:");
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        printf(" %s=%s", names[i], ai_setting_source(names[i]));
    printf("\n");
}

/* Never let a test call reach a real server: the host in force must be a local mock. */
static int safe_host(void) {
    ai_config c;
    if (ai_effective(&c) || strncmp(c.host, "http://127.0.0.1:", 17) ||
        !strcmp(c.host + 17, "11434")) {
        fprintf(stderr, "defaults: refusing to run against %s\n", c.host);
        return 0;
    }
    return 1;
}

static int mode_set(const char *host, const char *db, const char *reviews, const char *src) {
    const char *stops[] = {"DEFAULT-STOP", NULL};
    int env = !strcmp(src, "env");
    /* the caller's storage may be temporary: ai.h copies. With the environment set,
     * the defaults name paths that must never be used. */
    char tmpdb[1024], revdef[1024];
    snprintf(tmpdb, sizeof tmpdb, "%s%s", db, env ? ".unused" : "");
    snprintf(revdef, sizeof revdef, "%s%s", reviews, env ? ".unused" : "");

    /* cumulative: two calls, the second adds to the first */
    expect(ai_defaults(AI_CONFIG(.host = host, .llm = "alt-llm", .decider = "alt-decider",
                                 .timeout = AI_I(7))) == 0, "ai_defaults (first part)");
    if (bad || !safe_host()) return 1;
    expect(ai_defaults(AI_CONFIG(.embedder = "alt-embedder", .db = tmpdb, .reviews = revdef,
                                 .embed_dimensions = AI_I(64),
                                 .opts = AI_OPTS(.model = "ignored-model",
                                                 .system = "DEFAULT-SYS",
                                                 .temperature = AI_F(0.25), .seed = AI_I(9),
                                                 .keep_alive = "5m", .stop = stops))) == 0,
           "ai_defaults (second part, cumulative)");
    memset(tmpdb, 'x', sizeof tmpdb - 1);
    memset(revdef, 'x', sizeof revdef - 1);
    show();
    ai_config c;
    ai_effective(&c);
    expect(!strcmp(ai_setting_source("llm"), src) && !strcmp(ai_setting_source("host"), src) &&
               !strcmp(ai_setting_source("db"), src), "string settings come from the expected layer");
    expect(!strcmp(ai_setting_source("timeout"), src), "timeout source");
    expect(!strcmp(ai_setting_source("embed_dimensions"), "defaults") && c.embed_dimensions.v == 64,
           "embed_dimensions from defaults");
    expect(!strcmp(ai_setting_source("opts"), "defaults") && c.opts &&
               !strcmp(c.opts->system, "DEFAULT-SYS") && !c.opts->model,
           "opts deep-copied, model dropped");
    expect(!strcmp(ai_setting_source("guard"), "builtin"), "guard is built-in");
    expect(ai_setting_source("nonsense") == NULL, "unknown setting name gives NULL");
    expect(c.timeout.v == (env ? 5 : 7), "timeout value");
    expect(!strcmp(c.llm, env ? "inkling-2-mini:latest" : "alt-llm"), "llm value");

    /* one call per endpoint with the defaults, then with a per-call model */
    expect(ai_text("default text") != NULL, "ai_text uses the default llm");
    expect(ai_yesno("default question?", "state") >= 0, "ai_yesno uses the default decider");
    expect(ai_choose(pick, "state").ok, "ai_choose uses the default decider");
    expect(ai_embed("default embed").x != NULL, "ai_embed uses the default embedder");
    expect(ai_text_with(AI_OPTS(.model = "other-model", .temperature = AI_F(0.75)),
                        "per-call text") != NULL, "per-call .model overrides (text)");
    expect(ai_yesno_with(AI_OPTS(.model = "autotrust-jev"), "per-call question?", "s") >= 0,
           "per-call .model overrides (decider)");
    expect(ai_embed_with(AI_OPTS(.model = "embeddinggemma-2"), "per-call embed").x != NULL,
           "per-call .model overrides (embedder)");
    expect(ai_embed_with(AI_OPTS(.model = "embeddinggemma-2"), "again").x != NULL,
           "second embed with the other embedder");
    expect(ai_review("Pick?", "payload") == NULL && ai_status() == AI_OK, "review opened");

    /* store paths are fixed once in use */
    expect(ai_defaults(AI_CONFIG(.db = "elsewhere.cache", .llm = "zzz")) == -1 &&
               ai_status() == AI_EBUSY, "db change after first use refused (AI_EBUSY)");
    expect(ai_defaults(AI_CONFIG(.reviews = "elsewhere-reviews")) == -1 &&
               ai_status() == AI_EBUSY, "reviews change after first use refused (AI_EBUSY)");
    ai_effective(&c);
    expect(!strcmp(c.db, db) && strcmp(c.llm, "zzz"), "refused call changed nothing");

    /* validation, all or nothing */
    expect(ai_defaults(AI_CONFIG(.host = "https://example.org", .llm = "zzz")) == -1 &&
               ai_status() == AI_EHTTP, "https host rejected (AI_EHTTP)");
    expect(ai_defaults(AI_CONFIG(.timeout = AI_I(0), .llm = "zzz")) == -1 &&
               ai_status() == AI_EARG, "timeout 0 rejected");
    expect(ai_defaults(AI_CONFIG(.llm = "", .decider = "zzz")) == -1 && ai_status() == AI_EARG,
           "empty model name rejected");
    expect(ai_defaults(AI_CONFIG(.guard = AI_I(2))) == -1, "guard 2 rejected");
    ai_effective(&c);
    expect(strcmp(c.llm, "zzz") && strcmp(c.decider, "zzz") && c.timeout.v == (env ? 5 : 7),
           "invalid calls changed nothing");

    /* reset */
    expect(ai_defaults(NULL) == 0, "ai_defaults(NULL)");
    show();
    ai_effective(&c);
    expect(!strcmp(ai_setting_source("llm"), env ? "env" : "builtin") &&
               !strcmp(c.llm, env ? "inkling-2-mini:latest" : "inkling-2-mini:latest"),
           "reset: llm back to the built-in (or env)");
    expect(!strcmp(ai_setting_source("opts"), "builtin") && !c.opts &&
               !c.embed_dimensions.set, "reset: opts and embed_dimensions cleared");
    expect(!strcmp(c.db, db), "reset keeps the cache that is in use");
    return bad;
}

static int mode_timeout(const char *host) {
    struct timespec t0, t1;
    expect(ai_defaults(AI_CONFIG(.host = host, .timeout = AI_I(1), .db = "off")) == 0,
           "ai_defaults timeout 1");
    if (bad || !safe_host()) return 1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    char *s = ai_text("slow");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double dt = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    expect(!s && ai_status() == AI_EHTTP && strstr(ai_error(), "timed out"),
           "slow reply hits the default timeout");
    expect(dt < 2.5, "failed within 2.5 s");
    return bad;
}

static const char *g_host;
static int g_fail;

static void stress_job(int i, void *ctx) {
    (void)ctx;
    if (i % 4 == 0) {
        char sys[32];
        snprintf(sys, sizeof sys, "system %d", i);
        if (ai_defaults(AI_CONFIG(.host = g_host, .llm = i % 8 ? "inkling-2-mini" : "alt-llm",
                                  .opts = AI_OPTS(.system = sys, .temperature = AI_F(i)))))
            g_fail = 1;
    }
    ai_config c;
    if (ai_effective(&c) || !c.llm) g_fail = 1;
    if (!ai_text("job %d", i)) {
        fprintf(stderr, "stress: text %d: %s\n", i, ai_error());
        g_fail = 1;
    }
    if (!ai_choose(pick, "state").ok) {
        fprintf(stderr, "stress: choose %d: %s\n", i, ai_error());
        g_fail = 1;
    }
}

static int mode_stress(const char *host) {
    g_host = host;
    expect(ai_defaults(AI_CONFIG(.host = host)) == 0, "ai_defaults host");
    if (bad || !safe_host()) return 1;
    ai_parallel(64, 8, stress_job, NULL);
    expect(!g_fail, "64 jobs on 8 threads while the defaults change");
    ai_reset();
    return bad;
}

int main(int argc, char **argv) {
    if (argc >= 6 && !strcmp(argv[1], "set")) return mode_set(argv[2], argv[3], argv[4], argv[5]);
    if (argc >= 3 && !strcmp(argv[1], "timeout")) return mode_timeout(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "stress")) return mode_stress(argv[2]);
    fprintf(stderr, "usage: defaults set HOST DB REVIEWS SRC | timeout HOST | stress HOST\n");
    return 2;
}

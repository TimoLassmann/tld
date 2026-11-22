/* tests/ai/defaults.c - ai_defaults() (PRD 6.10).
 *
 *   defaults set HOST DB REVIEWS SRC   set every default, call each endpoint, check
 *                                      validation; SRC is "defaults", or "env" when
 *                                      run.sh sets the AI_* variables (they must win)
 *   defaults reset HOST                a later call replaces the defaults, NULL resets
 *                                      (TLD_AI_HOST must point at the mock)
 *   defaults timeout HOST              a 1 s default timeout against a slow mock
 *   defaults stress HOST               8 threads calling while defaults change (TSan)
 *   defaults afterexit                 a call after tld_ai.h's exit cleanup fails cleanly
 *   defaults longreview                a review directory too long for a path fails with
 *                                      AI_EARG instead of a shortened path
 *
 * run.sh checks the logged requests (models per endpoint, options, dimensions). */
#define _POSIX_C_SOURCE 200809L
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include "test.h"
#include "status_name.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define T2(X) X(alpha, "First") X(beta, "Second")
AI_CHOICE(pick, "Which one?", T2)

static int bad;

static void expect(int cond, const char *what) {
    printf("%s  %s", cond ? "ok  " : "BAD ", what);
    if (!cond) printf("  [status %s: %s]", status_name(ai_status()), ai_error());
    printf("\n");
    if (!cond) bad = 1;
}

/* Never let a test call reach a real server: the host in force (TLD_AI_HOST wins over the
 * defaults) must be a local mock. */
static int safe_host(const char *host) {
    const char *h = getenv("TLD_AI_HOST");
    if (!h || !*h) h = host;
    if (!h || strncmp(h, "http://127.0.0.1:", 17) || !strcmp(h + 17, "11434")) {
        fprintf(stderr, "defaults: refusing to run against %s\n", h ? h : "(none)");
        return 0;
    }
    return 1;
}

static int mode_set(const char *host, const char *db, const char *reviews, const char *src) {
    const char *stops[] = {"DEFAULT-STOP", NULL};
    int env = !strcmp(src, "env");
    if (!safe_host(host)) return 1;
    /* the caller's storage may be temporary: tld_ai.h copies. With the environment set,
     * the defaults name paths that must never be used. */
    char tmpdb[1024], revdef[1024];
    snprintf(tmpdb, sizeof tmpdb, "%s%s", db, env ? ".unused" : "");
    snprintf(revdef, sizeof revdef, "%s%s", reviews, env ? ".unused" : "");
    expect(ai_defaults(AI_CONFIG(.host = host, .llm = "alt-llm", .decider = "alt-decider",
                                 .embedder = "alt-embedder", .db = tmpdb, .reviews = revdef,
                                 .timeout = AI_I(7), .embed_dimensions = AI_I(64),
                                 .opts = AI_OPTS(.model = "ignored-model",
                                                 .system = "DEFAULT-SYS",
                                                 .temperature = AI_F(0.25), .seed = AI_I(9),
                                                 .keep_alive = "5m", .stop = stops))) == 0,
           "ai_defaults");
    memset(tmpdb, 'x', sizeof tmpdb - 1);
    memset(revdef, 'x', sizeof revdef - 1);

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

    /* validation, all or nothing: run.sh checks that the next text call still uses
     * the llm set above */
    expect(ai_defaults(AI_CONFIG(.host = "https://example.org", .llm = "zzz")) == -1 &&
               ai_status() == AI_EHTTP, "https host rejected (AI_EHTTP)");
    expect(ai_defaults(AI_CONFIG(.timeout = AI_I(0), .llm = "zzz")) == -1 &&
               ai_status() == AI_EARG, "timeout 0 rejected");
    expect(ai_defaults(AI_CONFIG(.llm = "", .decider = "zzz")) == -1 && ai_status() == AI_EARG,
           "empty model name rejected");
    expect(ai_defaults(AI_CONFIG(.fresh = AI_I(2))) == -1, "fresh 2 rejected");
    expect(ai_text("after invalid") != NULL, "invalid calls changed nothing");
    return bad;
}

static int mode_reset(const char *host) {
    if (!safe_host(NULL)) return 1; /* TLD_AI_HOST must be the mock: the reset drops .host */
    expect(ai_defaults(AI_CONFIG(.host = host, .llm = "alt-llm", .embed_dimensions = AI_I(64),
                                 .opts = AI_OPTS(.system = "RESET-SYS"))) == 0,
           "ai_defaults llm, embed_dimensions, opts");
    expect(ai_text("r1") != NULL && ai_embed("r1").x != NULL, "calls with the defaults");
    expect(ai_defaults(AI_CONFIG(.decider = "alt-decider")) == 0, "ai_defaults decider only");
    expect(ai_text("r2") != NULL, "call after the second ai_defaults");
    expect(ai_defaults(NULL) == 0, "ai_defaults(NULL)");
    expect(ai_text("r3") != NULL && ai_embed("r3").x != NULL, "calls after the reset");
    return bad;
}

static int mode_timeout(const char *host) {
    struct timespec t0, t1;
    if (!safe_host(host)) return 1;
    expect(ai_defaults(AI_CONFIG(.host = host, .timeout = AI_I(1), .db = "off")) == 0,
           "ai_defaults timeout 1");
    clock_gettime(CLOCK_MONOTONIC, &t0);
    const char *s = ai_text("slow");
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
    if (!safe_host(host)) return 1;
    expect(ai_defaults(AI_CONFIG(.host = host)) == 0, "ai_defaults host");
    ai_parallel(64, 8, stress_job, NULL);
    expect(!g_fail, "64 jobs on 8 threads while the defaults change");
    ai_reset();
    return bad;
}

/* Registered before tld_ai.h's own exit handler, so it runs after it. */
static void after_exit(void) {
    int ok = ai_text("after exit") == NULL && ai_status() == AI_EARG &&
             ai_defaults(AI_CONFIG(.llm = "x")) == -1 && ai_status() == AI_EARG &&
             ai_embed("after exit").x == NULL && !ai_choose(pick, "state").ok;
    printf("%s  calls after the exit cleanup fail with AI_EARG: %s\n", ok ? "ok  " : "BAD ",
           ai_error());
    fflush(stdout);
    _exit(ok ? 0 : 1);
}

static int mode_afterexit(void) {
    atexit(after_exit);
    expect(ai_defaults(AI_CONFIG(.db = "off")) == 0, "ai_defaults before exit");
    return 1; /* after_exit decides the exit code */
}

static int mode_longreview(void) {
    static char dir[1300];
    memset(dir, 'd', sizeof dir - 1);
    expect(ai_defaults(AI_CONFIG(.db = "off", .reviews = dir)) == 0, "ai_defaults with a long reviews");
    expect(ai_review("Pick?", "payload") == NULL && ai_status() == AI_EARG &&
               strstr(ai_error(), "too long") != NULL,
           "review directory too long: AI_EARG");
    return bad;
}

int main(int argc, char **argv) {
    test_init_keep(&argc, argv, "MODE ARGS...",
                   "ai_defaults() checks, driven by tests/ai/run.sh against tests/ai/mock_ollama.py:\n"
                   "  set HOST DB REVIEWS SRC   set every default, call each endpoint, check validation\n"
                   "                            (SRC is \"defaults\", or \"env\" when TLD_AI_* must win)\n"
                   "  reset HOST                a later call replaces the defaults, NULL resets\n"
                   "  timeout HOST              a 1 s default timeout against a slow mock\n"
                   "  stress HOST               8 threads calling while defaults change\n"
                   "  afterexit                 a call after the exit cleanup fails cleanly\n"
                   "  longreview                a too-long review directory fails with AI_EARG\n"
                   "HOST must be a local mock (TLD_AI_HOST wins over it).\n"
                   "Example: ai-defaults afterexit");
    if (argc >= 2) SECTION_MSG("ai_defaults: %s", argv[1]);
    if (argc >= 6 && !strcmp(argv[1], "set")) return mode_set(argv[2], argv[3], argv[4], argv[5]);
    if (argc >= 3 && !strcmp(argv[1], "reset")) return mode_reset(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "timeout")) return mode_timeout(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "stress")) return mode_stress(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "afterexit")) return mode_afterexit();
    if (argc >= 2 && !strcmp(argv[1], "longreview")) return mode_longreview();
    fprintf(stderr, "usage: defaults set HOST DB REVIEWS SRC | reset HOST | timeout HOST | "
                    "stress HOST | afterexit | longreview\n");
    return 2;
}

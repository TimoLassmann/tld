/* examples/triage.c - route a support ticket, draft a fix for bugs, ask a person when
 * unsure.
 *   cc -std=c11 -I. examples/triage.c -o triage -lpthread -lm
 *   ./triage tests/ai/ticket.txt
 *   ./triage -vv tests/ai/ticket.txt     # also each request (endpoint, model, host)
 * The results go to stdout; each step (answer, confidence, model, time, tokens) goes to
 * stderr. Exit codes: 0 done, 1 error, 2 usage, 3 waiting for a review (rerun once
 * answered). */
#define _POSIX_C_SOURCE 200809L /* setenv */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#define NAME(label, description) #label,

#define TEAM(X)                                   \
    X(bug,     "A software defect or crash")      \
    X(billing, "Payments, invoices and refunds")  \
    X(account, "Login, passwords and access")
AI_CHOICE(team, "Which team should handle this ticket?", TEAM)
static const char *team_name[] = {TEAM(NAME)};

/* Decision models score on exactly 6 levels, lowest first. */
#define URGENCY(X)                                      \
    X(none,     "Needs no action")                      \
    X(someday,  "Can wait a month or more")             \
    X(routine,  "Can wait a week")                      \
    X(soon,     "Should be handled this week")          \
    X(urgent,   "Blocks the customer today")            \
    X(critical, "Many customers are affected right now")
AI_SCALE(urgency, "How urgent is this ticket?", URGENCY)
static const char *urgency_name[] = {URGENCY(NAME)};

#define FIX(X)                                                    \
    X(str,  title,  "One-line summary of the fix")                \
    X(str,  steps,  "What the engineer should change")            \
    X(num,  risk,   "Risk that the fix breaks something, 0 to 1") \
    X(bool, needs_db_change, "Whether the database schema changes")
AI_STRUCT(fix, FIX)

/* After each call: which model and host answered, and how (stderr, TLD_VERBOSE >= 1). */
static void show_call(void) {
    const ai_info *in = ai_last();
    if (in->model) KV_MSG("model", "%s", in->model);
    if (in->host) KV_MSG("host", "%s", in->host);
    if (in->from_cache)
        OK_MSG("answered from the cache");
    else
        OK_MSG("answered in %.1f s · %ld prompt + %ld output tokens", in->seconds,
               in->prompt_tokens, in->output_tokens);
}

static int handle(const char *ticket) {
    SECTION_MSG("ai_choose: which team?");
    struct team_answer t = ai_choose(team, ticket);
    if (!t.ok) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
    show_call();
    KV_MSG("answer", "%s (confidence %.2f)", team_name[t.value], t.confidence);
    for (int i = 0; i < team__count; i++) KV_MSG("p", "%-8s %.2f", team_name[i], t.p[i]);

    SECTION_MSG("ai_rate: how urgent?");
    struct urgency_answer u = ai_rate(urgency, ticket);
    if (!u.ok) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
    show_call();
    KV_MSG("answer", "%s (score %.2f of 0..%d, confidence %.2f)", urgency_name[u.level], u.score,
           urgency__count - 1, u.confidence);

    SECTION_MSG("ai_yesno: asking for a refund?");
    double refund = ai_yesno("Is the customer asking for a refund?", ticket);
    if (refund < 0) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
    show_call();
    KV_MSG("answer", "P(yes) %.2f", refund);

    printf("team %s (confidence %.2f), urgency %s, P(refund) %.2f\n", team_name[t.value],
           t.confidence, urgency_name[u.level], refund);

    if (t.confidence < 0.8) {                         /* unsure: a person decides */
        SECTION_MSG("ai_review: confidence %.2f < 0.80, a person decides", t.confidence);
        const char *who = ai_review("Which team should handle this ticket?", ticket);
        if (!who) {
            if (ai_status() != AI_OK) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
            /* ai_error() names the review file */
            fprintf(stderr, "triage: %s; to answer, set \"answer\" to bug, billing or account and "
                            "\"status\" to \"done\" in that file, then run again\n", ai_error());
            return 3;                                 /* 3: rerun once answered */
        }
        printf("reviewer chose %s\n", who);
        return 0;
    }

    switch (t.value) {
    case bug: {
        SECTION_MSG("ai_fill: propose a fix (up to 3 tries, stop when risk < 0.3)");
        struct fix f;
        int ok = 0;
        for (int i = 0; i < 3; i++) {                 /* each attempt is a fresh call */
            if (ai_fill_with(AI_OPTS(.temperature = AI_F(0.3), .num_ctx = AI_I(16384)),
                             &f, fix, "Propose a fix for this ticket:\n%s", ticket) == 0) {
                ok = 1;
                show_call();
                KV_MSG("try", "%d: %s (risk %.2f)", i + 1, f.title, f.risk);
                if (f.risk < 0.3) break;
            } else {
                KV_MSG("try", "%d failed: %s", i + 1, ai_error());
            }
        }
        if (!ok) { fprintf(stderr, "triage: no fix: %s\n", ai_error()); return 1; }
        printf("fix: %s (risk %.2f, schema change %s)\n%s\n",
               f.title, f.risk, f.needs_db_change ? "yes" : "no", f.steps);
        break;
    }
    case billing: {
        SECTION_MSG("ai_text: reply to the customer");
        const char *reply = ai_text("Write a two-sentence reply to this customer:\n%s", ticket);
        if (!reply) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
        show_call();
        puts(reply);
        break;
    }
    case account:
        puts("forwarded to the access team");
        break;
    }
    return 0;
}

static void usage(FILE *f, const char *prog) {
    fprintf(f, "Usage: %s [-h] [-v] TICKET.txt\n\n"
               "Routes a support ticket: picks a team, rates urgency, asks whether it wants a\n"
               "refund, then drafts a fix (bug), a reply (billing) or forwards it (account).\n"
               "Below 0.80 confidence a person decides through a review file (exit 3).\n"
               "Example: %s tests/ai/ticket.txt\n\n"
               "Options:\n"
               "  -h, --help      show this help and exit\n"
               "  -v, --verbose   -vv also shows each request (TLD_VERBOSE=2)\n\n"
               "TLD_VERBOSE=0 leaves only the results and errors.\n",
            prog, prog);
}

int main(int argc, char **argv) {
    static struct option opts[] = {{"help", no_argument, 0, 'h'},
                                   {"verbose", no_argument, 0, 'v'},
                                   {0, 0, 0, 0}};
    int c, verbose = 0;
    while ((c = getopt_long(argc, argv, "hv", opts, NULL)) != -1) {
        switch (c) {
        case 'h': usage(stdout, argv[0]); return 0;
        case 'v': verbose++; break;
        default: usage(stderr, argv[0]); return 2;
        }
    }
    if (optind != argc - 1) { usage(stderr, argv[0]); return 2; }
    if (verbose) setenv("TLD_VERBOSE", verbose >= 2 ? "2" : "1", 1);

    tld_strbuf *ticket = NULL;
    if (tld_path_read(argv[optind], &ticket) != OK) return 1;   /* prints why */
    SECTION_MSG("ticket");
    KV_MSG("file", "%s", argv[optind]);
    int n = ticket->len;
    if (n > 0 && TLD_STR(ticket)[n - 1] == '\n') n--;        /* KV_MSG adds its own */
    KV_MSG("text", "%.*s", n, TLD_STR(ticket));
    int rc = handle(TLD_STR(ticket));
    tld_strbuf_free(ticket);
    return rc;
}

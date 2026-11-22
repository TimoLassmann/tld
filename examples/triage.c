/* examples/triage.c - route a support ticket, draft a fix for bugs, ask a person when
 * unsure.
 *   cc -std=c11 -I. examples/triage.c -o triage -lpthread -lm
 *   ./triage tests/ai/ticket.txt
 * Exit codes: 0 done, 1 error, 2 usage, 3 waiting for a review (rerun once answered). */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include <stdio.h>
#include <stdlib.h>

#define TEAM(X)                                   \
    X(bug,     "A software defect or crash")      \
    X(billing, "Payments, invoices and refunds")  \
    X(account, "Login, passwords and access")
AI_CHOICE(team, "Which team should handle this ticket?", TEAM)

/* Decision models score on exactly 6 levels, lowest first. */
#define URGENCY(X)                                      \
    X(none,     "Needs no action")                      \
    X(someday,  "Can wait a month or more")             \
    X(routine,  "Can wait a week")                      \
    X(soon,     "Should be handled this week")          \
    X(urgent,   "Blocks the customer today")            \
    X(critical, "Many customers are affected right now")
AI_SCALE(urgency, "How urgent is this ticket?", URGENCY)

#define FIX(X)                                                    \
    X(str,  title,  "One-line summary of the fix")                \
    X(str,  steps,  "What the engineer should change")            \
    X(num,  risk,   "Risk that the fix breaks something, 0 to 1") \
    X(bool, needs_db_change, "Whether the database schema changes")
AI_STRUCT(fix, FIX)

static int handle(const char *ticket) {
    struct team_answer t = ai_choose(team, ticket);
    if (!t.ok) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }

    struct urgency_answer u = ai_rate(urgency, ticket);
    if (!u.ok) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
    double refund = ai_yesno("Is the customer asking for a refund?", ticket);
    if (refund < 0) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
    printf("team %d (confidence %.2f), urgency %d, P(refund) %.2f\n",
           t.value, t.confidence, u.level, refund);

    if (t.confidence < 0.8) {                         /* unsure: a person decides */
        const char *who = ai_review("Which team should handle this ticket?", ticket);
        if (!who) return ai_status() == AI_OK ? 3 : 1; /* 3: rerun once answered */
        printf("reviewer chose %s\n", who);
        return 0;
    }

    switch (t.value) {
    case bug: {
        struct fix f;
        int ok = 0;
        for (int i = 0; i < 3; i++) {                 /* each attempt is a fresh call */
            if (ai_fill_with(AI_OPTS(.temperature = AI_F(0.3), .num_ctx = AI_I(16384)),
                             &f, fix, "Propose a fix for this ticket:\n%s", ticket) == 0) {
                ok = 1;
                if (f.risk < 0.3) break;
            }
        }
        if (!ok) { fprintf(stderr, "triage: no fix: %s\n", ai_error()); return 1; }
        printf("fix: %s (risk %.2f, schema change %s)\n%s\n",
               f.title, f.risk, f.needs_db_change ? "yes" : "no", f.steps);
        break;
    }
    case billing: {
        const char *reply = ai_text("Write a two-sentence reply to this customer:\n%s", ticket);
        if (!reply) { fprintf(stderr, "triage: %s\n", ai_error()); return 1; }
        puts(reply);
        break;
    }
    case account:
        puts("forwarded to the access team");
        break;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: triage ticket.txt\n"); return 2; }
    tld_strbuf *ticket = NULL;
    if (tld_path_read(argv[1], &ticket) != OK) return 1;   /* prints why */
    int rc = handle(TLD_STR(ticket));
    tld_strbuf_free(ticket);
    return rc;
}

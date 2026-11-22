/* tests/ai/special.c OUT - prompts with special characters must reach the server
 * intact. Writes the exact prompt and state to OUT.prompt / OUT.state (run.sh compares
 * them with the logged requests) and checks that the echoed reply decodes back. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include "test.h"
#include "show.h"
#include <stdio.h>
#include <string.h>

static const char prompt[] =
    "Quotes \"x\" and 'y', backslash \\ and \\n literal, slash /\n"
    "newline above, tab\there, CR\rhere, bell\a, unit\x1f sep, del\x7f\n"
    "JSON look-alikes: \"42\" \"true\" null {\"k\": [1, 2]} \\u00e9\n"
    "UTF-8: caf\xc3\xa9, na\xc3\xafve, \xe2\x82\xac 100, emoji \xf0\x9f\x98\x80 \xf0\x9f\xa7\xac,"
    " CJK \xe4\xb8\xad\xe6\x96\x87";
static const char state[] = "{\"ticket\": \"r\xc3\xa9" "fund \\\"now\\\"\", \"n\": 42, \"ok\": true}";
static const char plain_state[] = "Plain state with \"quotes\" and caf\xc3\xa9\n";

static int put(const char *path, const char *suffix, const char *s) {
    char p[1024];
    snprintf(p, sizeof p, "%s.%s", path, suffix);
    FILE *f = fopen(p, "wb");
    if (!f || fwrite(s, 1, strlen(s), f) != strlen(s) || fclose(f)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    test_init_keep(&argc, argv, "OUT",
                   "Sends a prompt full of quotes, control characters and UTF-8 and checks that the\n"
                   "mock echoes it back intact; writes the exact prompt and state to OUT.prompt and\n"
                   "OUT.state for run.sh to compare with the logged requests. Needs TLD_AI_HOST at the mock.\n"
                   "Example: TLD_AI_HOST=http://127.0.0.1:PORT ai-special /tmp/sp");
    if (argc < 2 || put(argv[1], "prompt", prompt) || put(argv[1], "state", plain_state)) {
        fprintf(stderr, "usage: special OUT\n");
        return 2;
    }
    int bad = 0;
    SECTION_MSG("special characters round trip");
    char *s = ai_text_with(AI_OPTS(.system = prompt), "%s", prompt);
    show_call("ai_text_with (prompt and system full of special characters)", s != NULL);
    if (!s || strncmp(s, "Mock reply to: ", 15) || strcmp(s + 15, prompt)) {
        fprintf(stderr, "special: reply did not round-trip: %s\n", s ? s : ai_error());
        bad = 1;
    }
    double y1 = ai_yesno(prompt, state);
    show_call("ai_yesno (JSON state)", y1 >= 0);
    double y2 = ai_yesno(prompt, plain_state);
    show_call("ai_yesno (plain state)", y2 >= 0);
    if (y1 < 0 || y2 < 0) {
        fprintf(stderr, "special: yesno: %s\n", ai_error());
        bad = 1;
    }
    puts(bad ? "special: FAILED" : "special: ok");
    return bad;
}

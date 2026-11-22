/* tests/ai/show.h - -v output for the tld_ai test programs: what was asked, which model
 * and host answered, time and tokens. All on stderr (stdout is checked by run.sh). */
#ifndef AI_TEST_SHOW_H
#define AI_TEST_SHOW_H

static inline void show_call(const char *what, int ok) {
    const ai_info *in = ai_last();
    KV_MSG("call", "%s", what);
    if (in->model) KV_MSG("model", "%s", in->model);
    if (in->host) KV_MSG("host", "%s", in->host);
    if (!ok)
        KV_MSG("failed", "%s", ai_error());
    else if (in->from_cache)
        OK_MSG("answered from the cache");
    else
        OK_MSG("answered in %.1f s · %ld prompt + %ld output tokens", in->seconds,
               in->prompt_tokens, in->output_tokens);
}

#endif

/* tests/ai/status_name.h - status code names for the tld_ai.h test programs. */
static const char *status_name(int s) {
    static const char *n[] = {"AI_OK",      "AI_ENOSERVER", "AI_EHTTP", "AI_EPARSE",
                              "AI_ESCHEMA", "AI_ENOMODEL",  "AI_EIO",   "AI_ENOMEM",
                              "AI_EARG"};
    return s >= 0 && s < (int)(sizeof n / sizeof n[0]) ? n[s] : "unknown";
}

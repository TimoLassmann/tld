/* Text as the format of a prompt: a hard compile error. */
#include "tld.h"
#include "tld_ai.h"
void f(const char *prompt);
void f(const char *prompt) { (void)ai_text(prompt); }

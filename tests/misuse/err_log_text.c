/* Text as the format of a message macro: a hard compile error. */
#include "tld.h"
void f(const char *text);
void f(const char *text) { LOG_MSG(text); }

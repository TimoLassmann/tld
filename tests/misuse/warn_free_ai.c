/* free() on library memory from tld_ai.h: rejected with -Wall -Werror (const). */
#include <stdlib.h>
#include "tld.h"
#include "tld_ai.h"
void f(void);
void f(void) { free(ai_text("hello")); }

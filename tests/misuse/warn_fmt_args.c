/* Arguments that do not match the format: rejected with -Wall -Werror. */
#include "tld.h"
void f(int n);
void f(int n) { LOG_MSG("%s items", n); }

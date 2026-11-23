/* timing: one global wall-clock stopwatch that logs the elapsed seconds on stderr. */
#ifndef TLD_TIMING_H
#define TLD_TIMING_H
#include "../core/tld-core.h"

/* end 0 starts the stopwatch; 1 logs "exec time: 1.23s" (a LOG line on stderr). One
 * global timer (static state, not thread-safe). */
TLD_API void tld__timer(int end);

/* Complete statements (they include the semicolon): TLD_START_TIMER starts the
 * stopwatch, TLD_END_TIMER prints the elapsed time (tld__timer(0) / (1)). */
#define TLD_START_TIMER tld__timer(0);
#define TLD_END_TIMER tld__timer(1);

#endif

/* timing: one global wall-clock stopwatch that prints elapsed seconds to stdout. */
#ifndef TLD_TIMING_H
#define TLD_TIMING_H
#include "../core/tld-core.h"

/* end 0 starts the stopwatch; 1 prints "\nexec time: 1.23s\n" and 2 prints "1.23s" to
 * stdout. One global timer (static state, not thread-safe). */
TLD_API void tld__timer(int end);

/* Complete statements (they include the semicolon): TLD_START_TIMER starts the
 * stopwatch, TLD_END_TIMER prints the elapsed time (tld__timer(0) / (1)). */
#define TLD_START_TIMER tld__timer(0);
#define TLD_END_TIMER tld__timer(1);

#endif

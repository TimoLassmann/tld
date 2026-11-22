/* logsum: arithmetic on probabilities stored as logarithms (log-sum-exp via a lookup
 * table) and tolerant floating-point equality. */
#ifndef TLD_LOGSUM_H
#define TLD_LOGSUM_H

#include "../core/tld-core.h"

/* 1 if a and b are equal up to a relative difference of FLT_EPSILON / DBL_EPSILON (an
 * absolute one when either is 0 or both are tiny), else 0. */
tld_external int nearly_equal_float(float a, float b);
tld_external int nearly_equal_double(double a, double b);

/* TLSAFE_EQ(X, Y): nearly_equal_float or nearly_equal_double, chosen by the type of X
 * (float or double only). */
#define TLSAFE_EQ(X,Y) _Generic((X),                        \
                                float: nearly_equal_float,  \
                                double: nearly_equal_double \
                )(X,Y)


/* Fill logsum's lookup table. Optional (logsum does it on first use); thread-safe. */
tld_external void init_logsum(void);

/* log(exp(a) + exp(b)) for log-space values, from a table with steps of 0.001. NaN if
 * either is NaN; -INFINITY acts as log(0); returns max(a, b) when they differ by 15.7 or
 * more. */
tld_external float logsum(const float a,const float b);

/* log(p), with -INFINITY for p == 0. */
tld_external float prob2scaledprob(float p);

/* exp(p), with 0 for p == -INFINITY. */
tld_external float scaledprob2prob(float p);

#endif

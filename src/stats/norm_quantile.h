/* norm_quantile: quantile normalisation of the columns of a matrix. */
#ifndef TLD_STATS_NORM_QUANTILE_H
#define TLD_STATS_NORM_QUANTILE_H

#include "../core/tld-core.h"

/* Quantile-normalise, in place, the m columns of the n x m matrix data (data[i][j] is
 * row i, column j; e.g. from galloc(&data, n, m)). Afterwards every column has the same
 * distribution: the mean over columns of the sorted values. Tied values in a column
 * share the mean of their positions. NaN is an error. */
TLD_API int norm_quantile(double** data, int n, int m);

#endif

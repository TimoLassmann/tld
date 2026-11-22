/* stats: descriptive statistics of double arrays (mean, variance, covariance, Pearson
 * correlation, Shannon entropy, standardising), the sigmoid, the normal density and
 * Gaussian kernel density estimates. Results come back through the last argument. */
#ifndef TLD_STATS_BASIC_H
#define TLD_STATS_BASIC_H

/* Shannon entropy in bits of the distribution vec[i] / sum(vec) (n >= 1, every
 * vec[i] >= 0); 0 when all values are 0. */
TLD_API int tld_shannon(double *vec, int n, double *entropy);

/* Arithmetic mean of n >= 1 values. */
TLD_API int tld_mean(double *vec, int n, double *mean);

/* Variance dividing by n (population, n >= 1) or by n - 1 (sample, n >= 2). */
TLD_API int tld_pop_variance(double *vec, int n, double *variance);
TLD_API int tld_sample_variance(double *vec, int n, double *variance);

/* Standardise vec in place to (x - mean) / sd, with the sample standard deviation;
 * the mean and sd used come back in *mean and *stdev. FAIL when sd is 0 or n < 2. */
TLD_API int tld_standardise(double *vec, int n, double *mean, double *stdev);

/* Covariance of two arrays of n values, dividing by n - 1 (sample, n >= 2) or by n
 * (population). */
TLD_API int tld_sample_covariance(double *vec_a, double *vec_b, int n, double *cov);
TLD_API int tld_pop_covariance(double *vec_a, double *vec_b, int n, double *cov);

/* Pearson correlation of two arrays of n values, in [-1, 1]; 0 when either array is
 * constant. */
TLD_API int tld_pearson_corr(double *vec_a, double *vec_b, int n, double *corr);

/* 1 / (1 + exp(-x)), computed without overflow. tld_sigmoid fails (FAIL) for an
 * infinite or NaN x; tld_sigmoid_safe never fails: NaN gives NaN, +inf 1, -inf 0. */
TLD_API int tld_sigmoid(double x, double *out);
TLD_API int tld_sigmoid_safe(double x, double *out);

/* Density at x of the normal distribution with mean mu and standard deviation
 * sigma (> 0). */
TLD_API int tld_normal_pdf(double x, double mu, double sigma, double* p);

/* Gaussian kernel density estimate of data[0..data_len-1], bandwidth sd * (3n/4)^(-1/5),
 * evaluated at the centres of n_steps equal bins spanning [min, max] of the data. *out
 * is a new n_steps x 2 galloc matrix, out[i][0] = x and out[i][1] = density; free with
 * gfree. */
TLD_API int tld_kde_pdf(double *data, int data_len, int n_steps, double ***out);

/* Weighted kernel density estimate: value data[i] seen cnt[i] (>= 0) times. The density
 * is evaluated at each data[i]: *out is a new data_len x 2 galloc matrix, out[i][0] =
 * data[i] and out[i][1] = density; free with gfree. */
TLD_API int tld_kde_count_pdf(double *data,double* cnt, int data_len, double*** out);

#endif

#ifndef BASIC_H
#define BASIC_H

#ifdef BASIC_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN int tld_shannon(double *vec, int n, double *entropy);

EXTERN int tld_mean(double *vec, int n, double *mean);
EXTERN int tld_pop_variance(double *vec, int n, double *variance);
EXTERN int tld_sample_variance(double *vec, int n, double *variance);


EXTERN int tld_normal_pdf(double x, double mu, double sigma, double* p);

#undef BASIC_IMPORT
#undef EXTERN


#endif

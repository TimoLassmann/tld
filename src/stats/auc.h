#ifndef AUC_H
#define AUC_H

#include "../core/tld-core.h"


tld_external int tld_auc(double *Y, double *Y_hat, int n, double *t, double *ret);


tld_external int tld_auc_calculate(double *Y, double *Y_hat, int n, double *ret);
tld_external int tld_auc_best_threshold(double *Y, double *Y_hat, int n, double *best_threshold);
#endif

/* auc: area under the ROC curve, and the best score threshold, for 0/1 labels and
 * real-valued scores. */
#ifndef AUC_H
#define AUC_H

#include "../core/tld-core.h"


/* ROC analysis of n samples: Y[i] is the label (1.0 positive; any other value in [0, 1]
 * negative), Y_hat[i] the score (higher means more likely positive; no NaN). Both
 * classes must occur. *ret is the AUC (trapezoids, tied scores form one step) and *t
 * the score nearest the (0, 1) corner of the curve: call Y_hat >= *t positive. */
tld_external int tld_auc(double *Y, double *Y_hat, int n, double *t, double *ret);


/* tld_auc giving only the AUC (tld_auc_calculate) or only the threshold
 * (tld_auc_best_threshold). */
tld_external int tld_auc_calculate(double *Y, double *Y_hat, int n, double *ret);
tld_external int tld_auc_best_threshold(double *Y, double *Y_hat, int n, double *best_threshold);
#endif

/* auc: area under the ROC curve, and the best score threshold, for 0/1 labels and
 * real-valued scores. */
#ifndef TLD_AUC_H
#define TLD_AUC_H

#include "../core/tld-core.h"

/* ROC analysis of n samples: Y[i] is the label (1.0 positive; any other value in [0, 1]
 * negative), Y_hat[i] the score (higher means more likely positive; no NaN). Both
 * classes must occur. *auc is the AUC (trapezoids, tied scores form one step) and *t
 * the score nearest the (0, 1) corner of the curve: call Y_hat >= *t positive. Either
 * output may be NULL when it is not wanted:
 *
 *   RUN(tld_auc(y, score, n, NULL, &auc));
 */
TLD_API int tld_auc(double *Y, double *Y_hat, int n, double *t, double *auc);
#endif

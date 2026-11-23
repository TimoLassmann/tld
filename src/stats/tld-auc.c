#include "tld-auc.h"

#include "../alloc/tld-alloc.h"

#include <float.h>
#include <math.h>

struct tld__auc_pt {
        double Y;
        double Y_hat;
};

static int tld__auc_sweep(double *Y, double *Y_hat, int n, double *auc_out, double *thres_out);

/* qsort comparator: ascending Y_hat */
static int tld__auc_pt_sort(const void *a, const void *b) {
        const struct tld__auc_pt *one = a;
        const struct tld__auc_pt *two = b;

        if (one->Y_hat < two->Y_hat) {
                return -1;
        } else if (one->Y_hat > two->Y_hat) {
                return 1;
        } else {
                return 0;
        }
}

/* Sorts the scores, sweeps the ROC curve from the highest score down,
   accumulates the area with the trapezoidal rule (tied scores form one
   step) and records the score nearest to the (0,1) corner. */
int tld__auc_sweep(double *Y, double *Y_hat, int n, double *auc_out, double *thres_out)
{
        struct tld__auc_pt* l = NULL;
        double thres = 0.0;
        double auc = 0.0;
        double tpr = 0.0;
        double fpr = 0.0;
        double last_tpr = 0.0;
        double last_fpr = 0.0;
        double best_distance = INFINITY;
        int tp = 0;
        int fp = 0;
        int total_positive = 0;
        int total_negative = 0;

        ASSERT(n > 0, "TLD auc needs more than 0 datapoints");
        ASSERT(Y != NULL, "No labels");
        ASSERT(Y_hat != NULL, "No predictions");

        MMALLOC(l, sizeof(struct tld__auc_pt) * (size_t) n);

        for(int i = 0; i < n; i++) {
                if(!(Y[i] >= 0.0 && Y[i] <= 1.0)) {
                        ERROR_MSG("label %d out of range: %f", i, Y[i]);
                }
                if(isnan(Y_hat[i])) {
                        ERROR_MSG("prediction %d is NaN", i);
                }
                l[i].Y = Y[i];
                l[i].Y_hat = Y_hat[i];
                if (Y[i] == 1.0) {
                        total_positive++;
                } else {
                        total_negative++;
                }
        }

        if(total_positive == 0 || total_negative == 0) {
                ERROR_MSG("AUC needs both classes (%d positive, %d negative)", total_positive, total_negative);
        }

        qsort(l, (size_t) n, sizeof(struct tld__auc_pt), tld__auc_pt_sort);

        for (int i = n - 1; i >= 0; i--) {
                if(l[i].Y == 1.0){
                        tp++;
                } else {
                        fp++;
                }

                if(i == 0 || l[i].Y_hat != l[i-1].Y_hat){
                        tpr = (double)tp / (double)total_positive;
                        fpr = (double)fp / (double)total_negative;

                        auc += (fpr - last_fpr) * (tpr + last_tpr) / 2.0;

                        double distance = sqrt(fpr * fpr + (1.0 - tpr) * (1.0 - tpr));
                        if (distance < best_distance) {
                                best_distance = distance;
                                thres = l[i].Y_hat;
                        }

                        last_tpr = tpr;
                        last_fpr = fpr;
                }
        }

        *auc_out = auc;
        *thres_out = thres;

        MFREE(l);
        return OK;
ERROR:
        if(l){
                MFREE(l);
        }
        return FAIL;
}

/* The best threshold is the score closest to the (0,1) corner of the ROC curve;
   samples with Y_hat >= threshold are called positive. */
int tld_auc(double *Y, double *Y_hat, int n, double *t, double *auc)
{
        double thres = 0.0;
        double area = 0.0;
        RUN(tld__auc_sweep(Y, Y_hat, n, &area, &thres));
        if(t){
                *t = thres;
        }
        if(auc){
                *auc = area;
        }
        return OK;
ERROR:
        return FAIL;
}

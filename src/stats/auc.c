#include "auc.h"


#include "../alloc/tld-alloc.h"

#include <stdio.h>
#include <math.h>

/* Unified Struct Definition */
struct auc_pt {
        double Y;
        double Y_hat;
};

/* Comparator function for qsort to sort in ascending order */
int auc_pt_sort(const void *a, const void *b) {
        struct auc_pt * const *one = a;
        struct auc_pt * const *two = b;

        if ((*one)->Y_hat < (*two)->Y_hat) {
                return -1;
        } else if ((*one)->Y_hat > (*two)->Y_hat) {
                return 1;
        } else {
                return 0;
        }
}

/* Function to compute AUC and determine the best threshold */
int tld_auc(double *Y, double *Y_hat, int n, double *t, double *ret)
{
        struct auc_pt** l = NULL;
        double thres = 0.0;
        double auc;

        if (n <= 0) {
                ERROR_MSG("TLD auc needs more than 0 datapoints");
        }

        /* Allocate memory for auc_pt pointers */
        MMALLOC(l, sizeof(struct auc_pt*) * n);

        /* Populate the auc_pt list */
        for(int i = 0; i < n; i++) {
                l[i] = NULL;
                if(Y[i] < 0.0 || Y[i] > 1.0) {
                        fprintf(stderr, "label %d out of range: %f\n", i, Y[i]);
                        goto ERROR;
                }
                MMALLOC(l[i], sizeof(struct auc_pt));
                l[i]->Y = Y[i];
                l[i]->Y_hat = Y_hat[i];
        }

        /* Sort the list in descending order of Y_hat */
        qsort(l, n, sizeof(struct auc_pt*), auc_pt_sort);

        /* Calculate AUC using the trapezoidal rule */
        double tpr = 0.0, last_tpr = 0.0, last_fpr = 0.0;
        int tp = 0, fp = 0;// last_tp = 0, last_fp = 0;
        int total_positive = 0, total_negative = 0;

        for (int i = 0; i < n; i++) {
                if (l[i]->Y == 1.0){
                        total_positive++;
                }else{
                        total_negative++;
                }
        }

        auc = 0.0;
        double best_distance = INFINITY;
        /* double best_tpr = 0.0, best_fpr = 0.0; */

        for (int i = n - 1; i >= 0; i--) {
                if(l[i]->Y == 1.0){
                        tp++;
                }else{
                        fp++;
                }

                if(i == 0 || l[i]->Y_hat != l[i-1]->Y_hat){
                        tpr = (double)tp / total_positive;
                        double fpr = (double)fp / total_negative;

                        if (i != n - 1) {
                                auc += (fpr - last_fpr) * (tpr + last_tpr) / 2.0;
                        }

                        double distance = sqrt(pow(0.0 - fpr, 2.0) + pow(1.0 - tpr, 2.0));
                        if (distance < best_distance) {
                                best_distance = distance;
                                thres = l[i]->Y_hat;
                                /* best_tpr = tpr; */
                                /* best_fpr = fpr; */
                        }

                        last_tpr = tpr;
                        last_fpr = fpr;
                        /* last_tp = tp; */
                        /* last_fp = fp; */
                }
        }

        /* Assign the results */
        *t = thres;
        *ret = auc;

        /* printf("Best Threshold: %f\n", thres); */
        /* printf("At Best Threshold: TPR=%f, FPR=%f, Distance=%f\n", best_tpr, best_fpr, best_distance); */

        /* Free allocated memory */
        for(int i = 0; i < n; i++) {
                if(l[i]){
                        MFREE(l[i]);
                }
        }
        MFREE(l);

        return OK;
ERROR:
        if(l){
                for(int i = 0; i < n; i++) {
                        if(l[i]){
                                MFREE(l[i]);
                        }
                }
                MFREE(l);
        }
        return FAIL;
}

#include "auc.h"


#include "../alloc/tld-alloc.h"

#include <stdio.h>
struct auc_pt {
        double Y;
        double Y_hat;
};

static int auc_pt_sort(const void *a, const void *b);

int tld_auc(double*Y , double* Y_hat, int n, double* ret)
{
        struct auc_pt**l = NULL;

        double* tpr = NULL;     /*  true positive rate: TP / (TP + FN) */
        double* fpr = NULL;     /* false positive rate: FP / (FP + TN)  */
        double tp;
        double tn;
        double fp;
        double fn;

        double auc;
        ASSERT(n > 0, "TLD auc needs more than 0 datapoints");

        MMALLOC(l, sizeof(struct auc_pt*) * n);

        galloc(&tpr,  n+1);
        galloc(&fpr,  n+1);

        for(int i = 0; i < n; i++){
                if(Y[i] < 0.0 || Y[i] > 1.0){
                        ERROR_MSG("label %d out of range: %f",i,Y[i]);
                }
                l[i] = NULL;
                MMALLOC(l[i], sizeof(struct auc_pt));
                l[i]->Y = Y[i];
                l[i]->Y_hat = Y_hat[i];
        }

        qsort(l, n, sizeof(struct auc_pt*),auc_pt_sort);


        tp = 0.0;
        fp = 0.0;
        tn = 0.0;
        fn = 0.0;
        for(int i = 0; i < n; i++){
                if(Y[i] == 1.0){
                        tp += 1.0;
                }else{
                        fp += 1.0;
                }
        }
        tpr[n] = tp / ( tp + fn);
        fpr[n] = fp / ( fp + tn);

        for(int i = 0; i < n; i++){
                tp = 0.0;
                fp = 0.0;
                tn = 0.0;
                fn = 0.0;

                for(int j = 0; j < i; j++){
                        if(l[j]->Y == 1.0){
                                tp++;
                        }else{
                                fp++;
                        }
                }

                for(int j = i; j < n; j++){
                        if(l[j]->Y == 1.0){
                                fn++;
                        }else{
                                tn++;
                        }
                }
                tpr[i] = tp / ( tp + fn);
                fpr[i] = fp / ( fp + tn);
        }
        auc = 0.0;
        for(int i = 1; i <= n;i++){
                double dx = fpr[i] - fpr[i-1];
                double dy = tpr[i] - tpr[i-1];
                auc += (dx* tpr[i-1]) + (0.5 * dx * dy);
        }
        /* LOG_MSG("auc: %f", auc); */

        /* fprintf(stdout,"target <- c("); */
        /* for(int i = 0; i < n;i++){ */
        /*         if(i){ */
        /*                 fprintf(stdout,",%f", Y[i]); */
        /*         }else{ */
        /*                 fprintf(stdout,"%f", Y[i]); */
        /*         } */
        /* } */
        /* fprintf(stdout,");\n"); */

        /* fprintf(stdout,"pred <- c("); */
        /* for(int i = 0; i < n;i++){ */
        /*         if(i){ */
        /*                 fprintf(stdout,",%f", Y_hat[i]); */
        /*         }else{ */
        /*                 fprintf(stdout,"%f", Y_hat[i]); */
        /*         } */
        /* } */
        /* fprintf(stdout,");\n"); */
        /* fprintf(stdout,"roc<- prediction(predictions= pred , labels = target) \n"); */
        /* fprintf(stdout,"auc =  performance(roc, measure = \"auc\") \n"); */
        /* fprintf(stdout,"auc@y.values\n"); */
        *ret = auc;
        for(int i = 0; i < n; i++){
                MFREE(l[i]);
        }

        MFREE(l);


        gfree(fpr);
        gfree(tpr);

        return OK;
ERROR:
        if(fpr){
                gfree(fpr);
        }
        if(tpr){
                gfree(tpr);
        }
        if(l){
                for(int i = 0; i < n; i++){
                        if(l[i]){
                                MFREE(l[i]);
                        }
                }
                MFREE(l);
        }
        return FAIL;
}

int auc_pt_sort(const void *a, const void *b)
{
        struct auc_pt * const *one = a;
        struct auc_pt * const *two = b;

        if((*one)->Y_hat > (*two)->Y_hat){
                return -1;
        }else{
                return 1;
        }
}

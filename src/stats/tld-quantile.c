#include "tld-quantile.h"
#include "../alloc/tld-alloc.h"

#include <math.h>

typedef struct {
        double v;
        int idx;
} tld__qnorm_item;

static int tld__qnorm_item_cmp (const void *a, const void *b);

/* Quantile normalisation of the m columns of an n x m matrix (in place).
   The reference distribution is the mean over columns of the sorted
   values. Each group of tied values in a column covers a run of sorted
   positions and gets the mean of the reference values over that run. */
int tld_quantile_normalise(double **data, int n, int m)
{
        double** data_rank = NULL;
        tld__qnorm_item* l = NULL;

        double sum = 0.0;

        ASSERT(data != NULL, "No data");
        ASSERT(n > 0, "Need at least one row");
        ASSERT(m > 0, "Need at least one column");

        for(int i = 0; i < n; i++){
                for(int j = 0; j < m; j++){
                        ASSERT(!isnan(data[i][j]), "NaN at row %d column %d", i, j);
                }
        }

        galloc(&data_rank,n,m);
        MMALLOC(l, sizeof(tld__qnorm_item) * (size_t) n);

        /* Sorted values of each column */
        for(int column = 0; column < m; column++){
                for(int i = 0; i < n; i++){
                        l[i].idx = i;
                        l[i].v = data[i][column];
                }
                qsort(l, (size_t) n, sizeof(tld__qnorm_item), tld__qnorm_item_cmp);

                for(int i = 0;i < n;i++){
                        data_rank[i][column] = l[i].v;
                }
        }

        /* Reference quantiles: mean across columns at each sorted position */
        for(int i = 0; i < n;i++){
                sum = 0.0;
                for(int j = 0; j < m;j++){
                        sum += data_rank[i][j];
                }
                sum /= (double) m;
                data_rank[i][0] = sum;
        }

        for(int column = 0; column < m; column++){
                for(int i = 0; i < n; i++){
                        l[i].idx = i;
                        l[i].v = data[i][column];
                }
                qsort(l, (size_t) n, sizeof(tld__qnorm_item), tld__qnorm_item_cmp);

                int start = 0;
                while(start < n){
                        int end = start + 1;
                        while(end < n && l[end].v == l[start].v){
                                end++;
                        }
                        sum = 0.0;
                        for(int k = start; k < end; k++){
                                sum += data_rank[k][0];
                        }
                        sum /= (double) (end - start);
                        for(int k = start; k < end; k++){
                                data[l[k].idx][column] = sum;
                        }
                        start = end;
                }
        }

        gfree(data_rank);
        MFREE(l);
        return OK;
ERROR:
        if(data_rank){
                gfree(data_rank);
        }
        if(l){
                MFREE(l);
        }
        return FAIL;
}

int tld__qnorm_item_cmp (const void *a, const void *b)
{
        const tld__qnorm_item *one = a;
        const tld__qnorm_item *two = b;
        if(one->v > two->v){
                return 1;
        }else if(one->v < two->v){
                return -1;
        }
        /* Equal values: order by original index so the sort is deterministic */
        if(one->idx > two->idx){
                return 1;
        }else if(one->idx < two->idx){
                return -1;
        }
        return 0;
}

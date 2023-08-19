#include "norm_quantile.h"
#include "../alloc/tld-alloc.h"

typedef struct {
    double v;
    int idx;
} n_itm;

static int cmp_n_itm (const void *a, const void *b);

int norm_quantile(double **data, int n, int m)
{
        n_itm** l = NULL;
        double** data_rank = NULL;


        galloc(&data_rank,n,m);
        MMALLOC(l, sizeof(n_itm*) * n );
        for(int i = 0; i < n;i++){
                l[i] = NULL;
                MMALLOC(l[i], sizeof(n_itm));
                l[i]->v = 0.0;
                l[i]->idx = 0;
        }

        /* main bit; */
        for(int column = 0; column < m; column++){
                for(int row = 0; row < n; row++){
                        l[row]->idx = row;
                        l[row]->v = data[row][column];
                }
                qsort(l, sizeof(n_itm), n, cmp_n_itm);

                for(int i = 0;i < 10;i++){
                        LOG_MSG("%d %d %f", column,i, l[i]->v);
                }
        }


        gfree(data_rank);
        for(int i = 0; i < n;i++){
                MFREE(l[i]);
        }
        MFREE(l);
        return OK;
ERROR:
        if(data_rank){
                gfree(data_rank);
        }
        if(l){
                for(int i = 0; i < n;i++){
                        if(l[i]){
                                MFREE(l[i]);
                        }
                }
                MFREE(l);
        }
        return FAIL;
}


int cmp_n_itm (const void *a, const void *b)
{
        n_itm * const *one = a;
        n_itm * const *two = b;
        if((*one)->v > (*two)->v){
                return 1;
        }else{
                return -1;
        }
}

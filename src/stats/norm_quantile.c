#include "norm_quantile.h"
#include "../alloc/tld-alloc.h"

#include "stdio.h"

typedef struct {
        double v;
        int idx;
        int rank;
} n_itm;

static int cmp_n_itm (const void *a, const void *b);
static int cmp_n_itm_idx(const void *a, const void *b);

int norm_quantile(double **data, int n, int m)
{
        double** data_rank = NULL;
        n_itm** l = NULL;

        double sum = 0.0;

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
                for(int i = 0; i < n; i++){
                        l[i]->idx = i;
                        l[i]->v = data[i][column];
                        /* LOG_MSG("%d %d", i, column); */
                }
                qsort(l,n,  sizeof(n_itm*), cmp_n_itm);

                for(int i = 0;i < n;i++){
                        data_rank[i][column] = l[i]->v;
                }
        }
        /* LOG_MSG("DATA org"); */
        /* for(int i = 0; i < n;i++){ */
        /*         for(int j = 0; j < m;j++){ */
        /*                 fprintf(stdout,"%f ",data[i][j]); */
        /*         } */
        /*         fprintf(stdout,"\n"); */
        /* } */
        /* fprintf(stdout,"\n"); */

        for(int i = 0; i < n;i++){
                sum = 0.0;
                for(int j = 0; j < m;j++){
                        /* fprintf(stdout,"%f ",data_rank[i][j]); */
                        sum += data_rank[i][j];
                }
                sum /= (double) m;
                data_rank[i][0] = sum;
                /* fprintf(stdout,": %f\n",sum); */
        }
        /* fprintf(stdout,"\n\n"); */
        for(int column = 0; column < m; column++){
                for(int i = 0; i < n; i++){
                        l[i]->idx = i;
                        l[i]->v = data[i][column];
                        l[i]->rank = -1;
                        /* LOG_MSG("%d %d", i, column); */
                }
                qsort(l,n,  sizeof(n_itm*), cmp_n_itm);
                l[0]->rank = 0;
                for(int i = 1; i < n; i++){
                        if(l[i]->v != l[i-1]->v){
                                l[i]->rank = l[i-1]->rank + 1;
                        }else{
                                l[i]->rank = l[i-1]->rank;
                        }
                }

                qsort(l,n,  sizeof(n_itm*), cmp_n_itm_idx);

                for(int i = 0;i < n;i++){
                        /* fprintf(stdout,"%d %d (%f -> %f) \n", l[i]->idx, l[i]->rank,l[i]->v,data_rank[l[i]->idx][0]); */
                        data[i][column] = data_rank[l[i]->rank][0];
                        /* data_rank[i][column] = l[i]->v; */
                        /* LOG_MSG("%d %d %f", column,i, l[i]->v); */
                }
                /* fprintf(stdout," IDX\n"); */
        }
        /* LOG_MSG("DATA norm"); */
        /* for(int i = 0; i < n;i++){ */
        /*         for(int j = 0; j < m;j++){ */
        /*                 fprintf(stdout,"%f ",data[i][j]); */
        /*         } */
        /*         fprintf(stdout,"\n"); */
        /* } */
        /* fprintf(stdout,"\n"); */
        for(int j = 0; j < m;j++){
                sum = 0.0;
                for(int i = 0; i < n;i++){
                        sum += data[i][j];
                        /* fprintf(stdout,"%f ",data[i][j]); */
                }
                /* fprintf(stdout,"%d %f\n",j, sum / (double) n); */
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

int cmp_n_itm_idx (const void *a, const void *b)
{
        n_itm * const *one = a;
        n_itm * const *two = b;
        if((*one)->idx > (*two)->idx ){
                return 1;
        }else{
                return -1;
        }
}

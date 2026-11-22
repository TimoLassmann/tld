#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "basic.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
/*
  adopted from:

  https://metacpan.org/release/JANERT/Statistics-KernelEstimation-0.05/source/lib/Statistics/KernelEstimation.pm
*/
static int copy_steps(double *data, int data_len, double ***ret);
static int create_steps(double *data, int data_len, int n_steps, double band, double*** ret);
static int default_bandwidth(double *x,double* cnt, int len, double* band);


int tld_kde_pdf(double *data, int data_len, int n_steps, double ***out)
{
        double total_cnt = 0.0;
        double band = 0.1;

        double** ladder = NULL;

        ASSERT(data != NULL, "No data");
        ASSERT(data_len > 0,"No data len?");
        ASSERT(n_steps > 0,"Number of steps must be positive");

        RUN(default_bandwidth(data,NULL,data_len,&band));

        RUN(create_steps(data,data_len, n_steps, band, &ladder));

        total_cnt = data_len;

        for(int i = 0; i < n_steps;i++){
                double d = 0.0;

                for(int j = 0; j < data_len; j++){
                        double add_d = 0.0;
                        RUN(tld_normal_pdf(ladder[i][0], data[j], band,&add_d));
                        d+= add_d;
                }
                d  = d / total_cnt;
                ladder[i][1] = d;
        }

        *out = ladder;

        return OK;
ERROR:
        if(ladder){
                gfree(ladder);
        }
        return FAIL;
}

int tld_kde_count_pdf(double *data,double* cnt, int data_len, double*** out)
{
        double total_cnt = 0.0;
        double band = 0.1;

        double** ladder = NULL;

        ASSERT(data != NULL, "No data");
        ASSERT(cnt != NULL, "No counts");
        ASSERT(data_len > 0,"No data len?");

        total_cnt = 0.0;
        for(int i = 0; i < data_len; i++){
                ASSERT(cnt[i] >= 0.0, "Count %d is negative or NaN", i);
                total_cnt += cnt[i];
        }
        ASSERT(total_cnt > 0.0, "Total count is zero");

        RUN(copy_steps(data,data_len, &ladder ));

        RUN(default_bandwidth(data,cnt,data_len,&band));

        for(int i = 0; i < data_len;i++){
                double d = 0.0;

                for(int j = 0; j < data_len; j++){
                        double add_d = 0.0;
                        RUN(tld_normal_pdf(ladder[i][0], data[j], band,&add_d));
                        add_d = add_d * cnt[j];
                        d+= add_d;
                }
                d  = d / total_cnt;
                ladder[i][1] = d;
        }

        *out = ladder;

        return OK;
ERROR:
        if(ladder){
                gfree(ladder);
        }
        return FAIL;
}

int copy_steps(double *data, int data_len, double ***ret)
{
        double** x = NULL;
        galloc(&x, data_len,2);

        for(int i = 0; i < data_len;i++){
                x[i][0] = data[i];
                x[i][1] = 0.0;
        }
        *ret = x;
        return OK;
ERROR:
        if(x){
                gfree(x);
        }
        return FAIL;
}

/* Evaluation points: n_steps bin centres spanning [min, max]. When all
   data are equal the range is empty, so span mean +/- 4 bandwidths. */
int create_steps(double *data, int data_len, int n_steps, double band, double*** ret)
{
        double** x = NULL;
        double min = DBL_MAX;
        double max = -DBL_MAX;
        double step = 0.0;
        for(int i = 0;i < data_len;i++){
                if(max < data[i]){
                        max = data[i];
                }
                if(min > data[i]){
                        min = data[i];
                }
        }
        if(!(max > min)){
                min = min - 4.0 * band;
                max = max + 4.0 * band;
        }
        step = (max- min)  / (double)(n_steps);
        galloc(&x, n_steps,2);

        for(int i = 0; i < n_steps;i++){
                x[i][0] = min + (double) i * step + step / 2.0;
                x[i][1] = 0.0;
        }
        *ret = x;
        return OK;
ERROR:
        if(x){
                gfree(x);
        }
        return FAIL;

}

/* Silverman-style bandwidth sigma * (3n/4)^(-1/5). The variance is
   computed in two passes (mean first, then squared deviations) to avoid
   the cancellation of E[x^2] - E[x]^2 for data with a large offset. For
   constant data the bandwidth falls back to 1e-3 * |mean|, or 1.0 when
   the mean is zero. */
int default_bandwidth(double *x,double* cnt, int len, double* band)
{
        double sum_x = 0.0;
        double sum_dd = 0.0;
        double mean;
        double var;
        double sigma;
        double sum = 0.0;
        double b;

        for(int i = 0; i < len; i++){
                double w = cnt ? cnt[i] : 1.0;
                sum_x += w * x[i];
                sum += w;
        }
        ASSERT(sum > 0.0, "Total weight is zero");
        mean = sum_x / sum;
        ASSERT(isfinite(mean), "Data contain NaN or infinite values");

        for(int i = 0; i < len; i++){
                double w = cnt ? cnt[i] : 1.0;
                double d = x[i] - mean;
                sum_dd += w * d * d;
        }
        var = sum_dd / sum;
        if(!(var > 0.0)){
                var = 0.0;
        }
        sigma = sqrt(var);

        b = sigma * pow(3.0 * sum / 4.0 , -1.0 / 5.0 );
        if(!(b > 0.0)){
                b = fabs(mean) * 1e-3;
                if(!(b > DBL_MIN)){
                        b = 1.0;
                }
        }
        ASSERT(isfinite(b), "Bandwidth is not finite");
        *band = b;
        return OK;
ERROR:
        return FAIL;
}

#include "tld-logsum.h"
#include <pthread.h>

#include <math.h>
#include <float.h>

#define TLD__LOGSUM_SCALE 1000.0F
#define TLD__LOGSUM_SIZE 16000

static float tld__logsum_lookup[TLD__LOGSUM_SIZE];
static pthread_once_t tld__logsum_once = PTHREAD_ONCE_INIT;

static void tld__logsum_fill_table(void)
{
        for(int i = 0; i < TLD__LOGSUM_SIZE;i++){
                tld__logsum_lookup[i] = logf(1.0F + expf((float) -i / TLD__LOGSUM_SCALE));
        }
}

/* Thread-safe; calling it is optional, logsum() initialises the table on first use. */
void init_logsum(void)
{
        pthread_once(&tld__logsum_once, tld__logsum_fill_table);
}

float logsum(const float a,const float b)
{
        if(isnan(a) || isnan(b)){
                return NAN;
        }
        const float max = MACRO_MAX(a, b);
        const float min = MACRO_MIN(a, b);

        if(  TLSAFE_EQ(min, -INFINITY)){
                return max;
        }
        /* !(d < 15.7) also catches +inf - +inf = NaN */
        if( !((max-min) < 15.7F)){
                return max;
        }
        init_logsum();
        return  max+ tld__logsum_lookup[(int)((max-min)*TLD__LOGSUM_SCALE)];
}

int nearly_equal_float(float a, float b)
{
        const float min = FLT_MIN;
        float absa = fabsf(a);
        float absb = fabsf(b);
        float d = fabsf(a-b);
        int ret = 0;
        if(a == b){
                ret = 1;
        }else if (a == 0.0F || b == 0 || (absa + absb < min)){
                ret =  d < (FLT_EPSILON * min);
        }else{
                ret =  d / MACRO_MIN((absa+absb), FLT_MAX) < FLT_EPSILON;
        }
        return ret;
}

int nearly_equal_double(double a, double b)
{
        const double min = DBL_MIN;
        double absa = fabs(a);
        double absb = fabs(b);
        double d = fabs(a-b);
        int ret = 0;
        if(a == b){
                ret = 1;
        }else if (a == 0.0 || b == 0 || (absa + absb < min)){
                ret = d < (DBL_EPSILON * min);
        }else{
                ret = d / MACRO_MIN((absa+absb), DBL_MAX) < DBL_EPSILON;
        }
        return ret;
}

#undef TLD__LOGSUM_SCALE
#undef TLD__LOGSUM_SIZE

#include <time.h>
#include <stdio.h>

#include "tld-timing.h"

void tld__timer(int end)
{
        static double time_start;
        struct timespec now = {0};
        double t = 0.0;

        if(timespec_get(&now, TIME_UTC) != TIME_UTC){
                now.tv_sec = time(NULL);
        }
        t = (double) now.tv_sec + (double) now.tv_nsec / 1e9;
        if(end){
                LOG_MSG("exec time: %1.2fs", t - time_start);
                return;
        }
        time_start = t;
}

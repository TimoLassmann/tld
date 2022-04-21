#ifndef TLD_LOGSUM_H
#define TLD_LOGSUM_H

#include "../core/tld-core.h"

tld_external void init_logsum(void);
tld_external float logsum(const float a,const float b);
tld_external float prob2scaledprob(float p);
tld_external float scaledprob2prob(float p);

#endif

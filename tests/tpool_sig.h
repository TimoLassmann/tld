#ifndef TPOOL_SIG_H
#define TPOOL_SIG_H

#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>

#ifdef TPOOL_SIG_IMPORT
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN atomic_int global_stop_run;

EXTERN pthread_mutex_t global_cond_mutex;
EXTERN pthread_cond_t global_cond;
EXTERN int setup_signals_and_thread_comms(void);
EXTERN void catch_signal(int sig);


#undef TPOOL_SIG_IMPORT
#undef EXTERN


#endif

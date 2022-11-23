#ifndef __HERETICWEB__
#define __HERETICWEB__
#include <pthread.h>
extern pthread_cond_t webcond;
extern pthread_mutex_t webmutex;
void I_HereticWebInit(void);
#endif

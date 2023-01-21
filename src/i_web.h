#ifndef __IWEB__
#define __IWEB__

#include <stdbool.h>
#include <semaphore.h>
#include <ulfius.h>

extern struct _u_instance web;
extern pthread_cond_t webcond;
extern pthread_mutex_t webmutex;

struct jrra_info_s {
    bool valid;
    int gamemission;
    int episode;
    int map;
    int totalkills;
    int killcount;
    int totalsecret;
    int secretcount;
    char *prettygamename;
    char *prettymapnum;
    char *mapname;
};
int I_WebInit(void);
void I_WebDestroy(void);

void I_WebNewLevel(
    int gamemission,
    int episode,
    int map,
    int totalkills,
    int totalsecret);

void I_WebUpdateSecretcount(int secretcount);

void I_WebUpdateKillcount(int killcount);

#endif

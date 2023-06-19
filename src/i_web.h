#ifndef __IWEB__
#define __IWEB__

#include <stdbool.h>
#include <semaphore.h>
#include <ulfius.h>

extern struct _u_instance web;
extern pthread_mutex_t corn_mutex;
extern int corns;

#define under(mtx) for(bool _x=true;_x && !pthread_mutex_lock(mtx);_x=false,pthread_mutex_unlock(mtx))

struct jrra_info_s {
    bool valid;
    bool hasSignaledDemo;
    int demoplayback;
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

void I_WebIsDemo(int demoplayback);

#endif

#ifndef _JRRA_REPORT_H_
#define _JRRA_REPORT_H_

struct jrra_info_s {
    int valid;
	int mission;
	int episode;
	int map;
    char *prettygamename;
    char *prettymapnum;
    char *mapname;
};

extern struct jrra_info_s jrra_info;
void jrra_report(int, int, int);
#endif

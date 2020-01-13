#define _GNU_SOURCE
#include <iso646.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i_system.h"
#include "d_mode.h"
#include "jrra_report.h"

sqlite3 *db;
bool jrra_did_register_atexit = false;
const char *jrra_db_path = "/home/jason/d/doom/maps.db";
const char *jrra_report_txt_path = "/tmp/jrra_report.txt";

/*
 *  some wordwrap stuff
 *  stolen from https://stackoverflow.com/questions/22582989/word-wrap-program-c
 */
inline int wordlen(const char * str){
   int tempindex=0;
   while(str[tempindex]!=' ' && str[tempindex]!=0 && str[tempindex]!='\n'){
      ++tempindex;
   }
   return(tempindex);
}
void wrap(char * s, const int wrapline){

   int index=0;
   int curlinelen = 0;
   while(s[index] != '\0'){

      if(s[index] == '\n'){
         curlinelen=0;
      }
      else if(s[index] == ' '){

         if(curlinelen+wordlen(&s[index+1]) >= wrapline){
            s[index] = '\n';
            curlinelen = 0;
         }

      }

      curlinelen++;
      index++;
   }

}

/* end stolen wordwrap stuff */


bool DB_Open(const char *filename)
{
	__label__ out_return;
	int rc;
	char *zErr = NULL;

	rc = sqlite3_open(filename, &db);
	if (rc != SQLITE_OK) {
		zErr = "in sqlite3_open";
		goto out_return;
	}

out_return:
	if (zErr) {
		fprintf(stderr, "error in DB_Open: %s\n", zErr);
		return false;
	} else {
		return true;
	}
}

bool DB_Close()
{
	__label__ out_return;
	int rc;
	char *zErr = NULL;

	rc = sqlite3_close(db);
	if (rc != SQLITE_OK) {
		zErr = "in sqlite3_close";
		goto out_return;
	}

out_return:
	if (zErr) {
		fprintf(stderr, "error in DB_Close: %s\n", zErr);
		return false;
	} else {
		return true;
	}
}

const char *jrra_get_gamename(int logical_gamemission)
{
	const char *gamename = NULL;

	switch(logical_gamemission) {
	case doom:
		gamename = "doom";
		break;
	case doom2:
		gamename = "doom2";
		break;
	case pack_plut:
		gamename = "plutonia";
		break;
	case pack_tnt:
		gamename = "tnt";
		break;
	case heretic:
		gamename = "heretic";
		break;
	case hexen:
		gamename = "hexen";
		break;
	default:
		gamename = "unknown";
		break;
	}

	return gamename;
}

char *jrra_get_mapname(int logical_gamemission, int episode, int map)
{
	__label__ out_return, out_finalize;
	int rc;
	sqlite3_stmt *stmt;
	char *zErr = NULL;
	const char *gamename = NULL;
	char *mapname = NULL;

	gamename = jrra_get_gamename(logical_gamemission);

	rc = sqlite3_prepare_v2(
		db,
		"select name from maps where game=? and map=? order by episode!=?;",
		-1,
		&stmt,
		NULL
	);
	if (rc != SQLITE_OK) {
		zErr = "in prepare";
		goto out_return;
	}

	rc = sqlite3_bind_text(stmt, 1, gamename, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		zErr = "in bind gamename";
		goto out_finalize;
	}

	rc = sqlite3_bind_int(stmt, 3, episode);
	if (rc != SQLITE_OK) {
		zErr = "in bind episode";
		goto out_finalize;
	}

	rc = sqlite3_bind_int(stmt, 2, map);
	if (rc != SQLITE_OK) {
		zErr = "in bind map";
		goto out_finalize;
	}

	rc = sqlite3_step(stmt);
	switch (rc) {
	case SQLITE_ROW:
		mapname = strdup((char *)sqlite3_column_text(stmt, 0));
		break;
	case SQLITE_DONE:
		mapname = NULL;
		break;
	default:
		zErr = "in sqlite3_step";
		break;
	}

out_finalize:
	rc = sqlite3_finalize(stmt);
out_return:
	if (zErr) {
		fprintf(stderr, "error in jrra_get_mapname: %s\n", zErr);
		return NULL;
	}
	return mapname;
}

char *jrra_get_prettygamename(int logicalgamemission)
{
	__label__ out_return, out_finalize;
	int rc;
	char *zErr = NULL;
	const char *gamename = NULL;
	sqlite3_stmt *stmt;
	char *prettygamename = NULL;

	gamename = jrra_get_gamename(logicalgamemission);

	rc = sqlite3_prepare_v2(
		db,
		"select name from games where game=?;",
		-1,
		&stmt,
		NULL
	);
	if (rc != SQLITE_OK) {
		zErr = "in prepare";
		goto out_return;
	}

	rc = sqlite3_bind_text(stmt, 1, gamename, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		zErr = "in bind";
		goto out_finalize;
	}

	rc = sqlite3_step(stmt);
	switch (rc) {
	case SQLITE_ROW:
		prettygamename = strdup((char *)sqlite3_column_text(stmt, 0));
		break;
	case SQLITE_DONE:
		prettygamename = NULL;
		break;
	default:
		prettygamename = NULL;
		zErr = "in step";
		break;
	}

out_finalize:
	rc = sqlite3_finalize(stmt);
out_return:
	if (zErr) {
		fprintf(stderr, "error in jrra_get_prettygamename: %s\n", zErr);
		return NULL;
	} else {
		return prettygamename;
	}
}

char *jrra_get_prettymapnum(int logical_gamemission, int episode, int map)
{
	char *prettymapnum = NULL;

	switch (logical_gamemission) {
	case doom:
	case heretic:
		asprintf(
			&prettymapnum,
			"E%dM%d",
			episode,
			map
		);
		break;
	case doom2:
	case pack_tnt:
	case pack_plut:
	case hexen:
		asprintf(
			&prettymapnum,
			"MAP%02d",
			map
		);
		break;
	default:
		prettymapnum = NULL;
		break;
	}

	return prettymapnum;
}

void jrra_atexit()
{
	FILE *f = fopen(jrra_report_txt_path, "w");
	if (f) {
		fprintf(f, "\n\n\n ");
		fclose(f);
	}
}

void jrra_report(int logical_gamemission, int episode, int map)
{
	__label__ out_return, out_free, out_fclose;
	int rc;
	char *zErr = NULL;

	char *mapname = NULL;
	char *prettygamename = NULL;
	char *prettymapnum = NULL;
	FILE *f = NULL;
	
	if (!jrra_did_register_atexit) {
		I_AtExit(jrra_atexit, true);
	}

	rc = DB_Open(jrra_db_path);
	if (!rc) {
		zErr = "in DB_Open";
		goto out_return;
	}

	mapname = jrra_get_mapname(logical_gamemission, episode, map);
	prettygamename = jrra_get_prettygamename(logical_gamemission);
	prettymapnum = jrra_get_prettymapnum(logical_gamemission, episode, map);

	if (mapname) wrap(mapname, 15);

	f = fopen(jrra_report_txt_path, "w");
	if (!f) {
		zErr = "in fopen";
		goto out_free;
	}

	rc = fprintf(f, "%s \n%s%s \n%s \n ",
		prettygamename?:"",
		prettymapnum?:"",
		mapname?":":"",
		mapname?:""
	);
	if (rc < 0) {
		zErr = "in fprintf";
		goto out_fclose;
	}

out_fclose:
	fclose(f);
out_free:
	free(mapname);
	free(prettygamename);
	free(prettymapnum);
	rc = DB_Close();
	if (!rc) {
		zErr = "in DB_Close";
		goto out_return;
	}
out_return:
	if (zErr) {
		fprintf(stderr, "error in jrra_report: %s\n", zErr);
	}
	return;
}

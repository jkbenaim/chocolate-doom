#include <ulfius.h>
#include "i_web.h"
#include "hereticweb.h"
#include "jrra_report.h"
#include <sys/stat.h>
#include "doomdef.h"
#include <stdbool.h>
#include <semaphore.h>
#include <err.h>
#include "i_system.h"
#include "p_local.h"
#include "s_sound.h"

pthread_cond_t webcond;
pthread_mutex_t webmutex;
bool webexit = false;

json_t *doominfo()
{
	json_t *j = json_object();
	if (jrra_info.valid == 0) {
		return j;
	}
	json_object_set(j, "mission", json_integer(jrra_info.mission));
	json_object_set(j, "episode", json_integer(jrra_info.episode));
	json_object_set(j, "map", json_integer(jrra_info.map));
	json_object_set(j, "gamename", json_string(jrra_info.prettygamename));
	json_object_set(j, "prettymapnum", json_string(jrra_info.prettymapnum));
	json_object_set(j, "mapname", json_string(jrra_info.mapname));
	json_object_set(j, "totalkills", json_integer(totalkills));
	json_object_set(j, "killcount", json_integer(players[0].killcount));
	json_object_set(j, "totalsecret", json_integer(totalsecret));
	json_object_set(j, "secretcount", json_integer(players[0].secretcount));
	return j;
}

void manager_cb(const struct _u_request *request, struct _websocket_manager *manager, void *user_data)
{
	int rc;

	printf("manager\n");
	P_SetMessage(&players[consoleplayer], "CONNECTED", true);
	S_StartSound(NULL, sfx_chat);

	if (pthread_mutex_lock(&webmutex) != 0) {
		err(1, "pthread_mutex_lock");
	}

	while (1) {
		json_t *j = doominfo();
		rc = ulfius_websocket_send_json_message(
			manager,
			j
		);
		//if (rc != U_OK) printf("error in uwsjm\n");
		json_decref(j);
		rc = pthread_cond_wait(&webcond, &webmutex);
		if (rc != 0) err(1, "pthread_cond_wait");
		if (webexit) return;
	}

}

void message_cb(const struct _u_request *request, struct _websocket_manager *manager, const struct _websocket_message *message, void *user_data)
{
	printf("message, length: %lu\n", message->data_len);
}

void onclose_cb(const struct _u_request *request, struct _websocket_manager *manager, void *user_data)
{
	printf("onclose\n");
}

void killall_managers()
{
	webexit = true;
	pthread_cond_broadcast(&webcond);
}

int callback_ws(const struct _u_request *request, struct _u_response *response, void *user_data)
{
	int rc;
	printf("header: %s\n", u_map_get(request->map_header, "Upgrade")?:"(none)");

	rc = ulfius_set_websocket_response(
		response,
		NULL,
		NULL,
		manager_cb,
		NULL,
		message_cb,
		NULL,
		onclose_cb,
		NULL
	);
	if (rc != U_OK) {
		printf("oopsie\n");
		ulfius_set_empty_body_response(response, 500);
		return U_CALLBACK_CONTINUE;
	}

	printf("all good\n");
	return U_CALLBACK_CONTINUE;
}

int callback_wad(const struct _u_request *request, struct _u_response *response, void *user_data)
{
	const char *name;
	void *buf;
	lumpindex_t idx;

	name = u_map_get(request->map_url, "lumpname");
	if (!name) {
		ulfius_set_empty_body_response(response, 400);
		return U_CALLBACK_CONTINUE;
	}

	idx = W_CheckNumForName(name);
	if (idx == -1) {
		ulfius_set_empty_body_response(response, 404);
		return U_CALLBACK_CONTINUE;
	}

	buf = malloc(W_LumpLength(idx));
	W_ReadLump(idx, buf);
	ulfius_set_binary_body_response(response, 200, buf, W_LumpLength(idx));

	return U_CALLBACK_CONTINUE;
}

void I_HereticWebInit(void)
{
	int rc;
	rc = pthread_mutex_init(&webmutex, NULL);
	if (rc != 0) err(1, "couldn't create pthread mutex");
	rc = pthread_cond_init(&webcond, NULL);
	if (rc != 0) err(1, "couldn't create pthread condition");

	I_AtExit(killall_managers, 1);

	I_WebInit();
	ulfius_add_endpoint_by_val(
		&web,
		"GET",
		NULL,
		"/ws",
		0,
		&callback_ws,
		NULL
	);
	ulfius_add_endpoint_by_val(
		&web,
		"GET",
		NULL,
		"/wad/:lumpname",
		0,
		&callback_wad,
		NULL
	);
}


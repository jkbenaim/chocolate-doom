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

pthread_cond_t twitchcond;
pthread_mutex_t twitchmutex;
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
	S_StartSound(NULL, sfx_telept);

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
	should_redeem = 1;
}

void onclose_cb(const struct _u_request *request, struct _websocket_manager *manager, void *user_data)
{
	printf("onclose\n");
}

void killall_managers()
{
	webexit = true;
	pthread_cond_broadcast(&webcond);
	pthread_cond_broadcast(&twitchcond);
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

void twitch_manager_cb(
	const struct _u_request *request,
	struct _websocket_manager *websocket_manager,
	void *user_data
) {
	int rc;

	printf("twitch manager\n");
	if (pthread_mutex_lock(&twitchmutex) != 0) {
		err(1, "twitch pthread_mutex_lock");
	}

	while (1) {
		rc = pthread_cond_wait(&twitchcond, &twitchmutex);
		if (rc != 0) err(1, "twitch pthread_cond_wait");
		if (webexit) return;
	}

}

void twitch_message_cb(
	const struct _u_request *request,
	struct _websocket_manager *websocket_manager,
	const struct _websocket_message *message,
	void *user_data
) {
	char *type = NULL;
	int rc;
	char *msg = NULL;
	json_t *jmsg = NULL;
	json_error_t jerr;

	//printf("message is: %.*s\n", (int)message->data_len, message->data);

	msg = calloc(1, message->data_len + 1);
	if (!msg) err(1, "in calloc");
	memcpy(msg, message->data, message->data_len);

	jmsg = json_loads(msg, 0, NULL);
	rc = json_unpack_ex(jmsg, &jerr, 0, "{s{ss}}", "metadata", "message_type", &type);
	if (rc == -1) {
		printf("error in unpack: %s\n", jerr.text);
		printf("\t%s\n", jerr.source);
		return;
	}
	if (!strcmp(type, "session_welcome")) {
		char *session_id = NULL;
		struct _u_request req;
		struct _u_response resp;
		json_t *subbody = NULL;
		json_t *transport = NULL;

		printf("twitch welcome\n");
		
		rc = json_unpack_ex(jmsg, &jerr, 0, "{s{s{ss}}}", "payload", "session", "id", &session_id);
		if (rc == -1) {
			printf("error in welcome unpack: %s\n", jerr.text);
			printf("\t%s\n", jerr.source);
			return;
		}

		if (!getenv("CHOCO_AUTH")) errx(1, "CHOCO_AUTH not set");
		if (!getenv("CHOCO_CID")) errx(1, "CHOCO_CID not set");

		rc = ulfius_init_request(&req);
		if (rc != U_OK) errx(1, "couldn't init sub request");

		rc = ulfius_set_request_properties(
			&req,
			U_OPT_HTTP_VERB, "POST",
			U_OPT_HTTP_URL, "https://api.twitch.tv/helix/eventsub/subscriptions",
			U_OPT_HEADER_PARAMETER, "Content-Type", "application/json",
			U_OPT_HEADER_PARAMETER, "Authorization", getenv("CHOCO_AUTH"),	// FIXME
			U_OPT_HEADER_PARAMETER, "Client-Id", getenv("CHOCO_CID"),	// FIXME
			U_OPT_NONE
		);
		if (rc != U_OK) errx(1, "couldn't set sub request properties");

		subbody = json_loads("{\"type\":\"channel.channel_points_custom_reward_redemption.add\",\"version\":\"1\",\"condition\":{\"broadcaster_user_id\":\"44201757\"},\"transport\":{\"method\":\"websocket\"}}", 0, NULL);
		if (!subbody) errx(1, "couldn't json_loads");

		rc = json_unpack(subbody, "{so}", "transport", &transport);
		if (rc == -1) errx(1, "couldn't json_unpack for transport");
		rc = json_object_set(transport, "session_id", json_string(session_id));
		if (rc == -1) errx(1, "couldn't set session_id");

		rc = ulfius_set_json_body_request(&req, subbody);
		if (rc != U_OK) errx(1, "couldn't set sub request json body");

		rc = ulfius_init_response(&resp);
		if (rc != U_OK) errx(1, "couldn't init sub response");
		rc = ulfius_send_http_request(&req, &resp);
		if (rc != U_OK) errx(1, "couldn't send sub request");
	} else if (!strcmp(type, "session_keepalive")) {
		//printf("twitch keepalive\n");
	} else if (!strcmp(type, "notification")) {
		should_redeem = 1;
		printf("redemption received\n");
	} else {
		printf("unknown twitch message type: %s\n", type);
	}
}

void twitch_close_cb(
	const struct _u_request *request,
	struct _websocket_manager *websocket_manager,
	void *user_data
) {
	printf("twitch close\n");
}


void I_HereticWebInit(void)
{
	int rc;
	struct _u_request request;
	struct _u_response response;
	struct _websocket_client_handler twitch_client_handler = {NULL, NULL};

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

	ulfius_init_request(&request);
	ulfius_init_response(&response);
	rc = ulfius_set_websocket_request(
		&request,
		"wss://eventsub-beta.wss.twitch.tv/ws",
		NULL,
		NULL
	);
	if (rc != U_OK) errx(1, "couldn't set ws request");

	printf("opening ws client\n");
	rc = ulfius_open_websocket_client_connection(
		&request,
		&twitch_manager_cb, NULL,
		&twitch_message_cb, NULL,
		&twitch_close_cb, NULL,
		&twitch_client_handler,
		&response
	);
	if (rc != U_OK) errx(1, "couldn't open ws client connection");
	printf("opened\n");

}


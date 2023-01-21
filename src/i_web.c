#include <ulfius.h>
#include "i_web.h"
#include <sys/stat.h>
#include <err.h>
#include <semaphore.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <ulfius.h>
#include <string.h>
#include "i_system.h"
#include "jrra_db.h"

struct _u_instance web;
pthread_cond_t webcond;
pthread_mutex_t webmutex;
pthread_cond_t twitchcond;
pthread_mutex_t twitchmutex;
pthread_mutex_t infomutex;
bool webexit = false;

struct jrra_info_s jrra_info = {.valid=false};

#define under(mtx) for(bool _x=true;_x && !pthread_mutex_lock(mtx);_x=false,pthread_mutex_unlock(mtx))

void I_WebNewLevel(
    int gamemission,
    int episode,
    int map,
    int totalkills,
    int totalsecret)
{
    under(&infomutex) {
        jrra_info.valid = true;
        jrra_info.gamemission = gamemission;
        jrra_info.episode = episode;
        jrra_info.map = map;
        jrra_info.totalkills = totalkills;
        jrra_info.totalsecret = totalsecret;
        jrra_info.secretcount = 0;
        jrra_info.killcount = 0;
        jrra_info.prettygamename = NULL;
        jrra_info.prettymapnum = NULL;
        jrra_info.mapname = NULL;
        jrra_report(&jrra_info);
    }
    pthread_cond_broadcast(&webcond);
}

void I_WebUpdateSecretcount(int secretcount)
{
    under(&infomutex) {
        jrra_info.secretcount = secretcount;
    }
    pthread_cond_broadcast(&webcond);
}

void I_WebUpdateKillcount(int killcount)
{
    under(&infomutex) {
        jrra_info.killcount = killcount;
    }
    pthread_cond_broadcast(&webcond);
}

json_t *doominfo()
{
    json_t *j = json_object();
    json_object_set(j,"hello",json_null());
    if (jrra_info.valid == false) {
        return j;
    }
    json_object_set(j, "gamemission", json_integer(jrra_info.gamemission));
    json_object_set(j, "episode", json_integer(jrra_info.episode));
    json_object_set(j, "map", json_integer(jrra_info.map));
    json_object_set(j, "gamename", json_string(jrra_info.prettygamename));
    json_object_set(j, "prettymapnum", json_string(jrra_info.prettymapnum));
    json_object_set(j, "mapname", json_string(jrra_info.mapname));
    json_object_set(j, "totalkills", json_integer(jrra_info.totalkills));
    json_object_set(j, "killcount", json_integer(jrra_info.killcount));
    json_object_set(j, "totalsecret", json_integer(jrra_info.totalsecret));
    json_object_set(j, "secretcount", json_integer(jrra_info.secretcount));
    return j;
}

void manager_cb(
    const struct _u_request *request,
    struct _websocket_manager *manager,
    void *user_data)
{
    int rc;

    if (pthread_mutex_lock(&webmutex) != 0) {
        err(1, "pthread_mutex_lock");
    }

    while (1) {
        json_t *j = doominfo();
        rc = ulfius_websocket_send_json_message(
            manager,
            j
        );
        json_decref(j);
        rc = pthread_cond_wait(&webcond, &webmutex);
        if (rc != 0) err(1, "pthread_cond_wait");
        if (webexit) return;
    }
}

void message_cb(
    const struct _u_request *request,
    struct _websocket_manager *manager,
    const struct _websocket_message *message,
    void *user_data)
{
    printf("message, length: %lu\n", message->data_len);
    // should_redeem = 1;
}

void onclose_cb(
    const struct _u_request *request,
    struct _websocket_manager *manager,
    void *user_data)
{
    printf("onclose\n");
}

void killall_managers()
{
    webexit = true;
    pthread_cond_broadcast(&webcond);
    pthread_cond_broadcast(&twitchcond);
}

int callback_ws(
    const struct _u_request *request,
    struct _u_response *response,
    void *user_data)
{
    int rc;

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

    return U_CALLBACK_CONTINUE;
}

void twitch_manager_cb(
    const struct _u_request *request,
    struct _websocket_manager *websocket_manager,
    void *user_data)
{
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
    void *user_data)
{
    char *type = NULL;
    int rc;
    char *msg = NULL;
    json_t *jmsg = NULL;
    json_error_t jerr;

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

        rc = ulfius_init_request(&req);
        if (rc != U_OK) errx(1, "couldn't init sub request");

        rc = ulfius_set_request_properties(
            &req,
            U_OPT_HTTP_VERB, "POST",
            U_OPT_HTTP_URL, "https://api.twitch.tv/helix/eventsub/subscriptions",
            U_OPT_HEADER_PARAMETER, "Content-Type", "application/json",
            U_OPT_HEADER_PARAMETER, "Authorization", getenv("CHOCO_AUTH"),
            U_OPT_HEADER_PARAMETER, "Client-Id", getenv("CHOCO_CID"),
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
        // should_redeem = 1;
        printf("redemption received\n");
    } else {
        printf("unknown twitch message type: %s\n", type);
    }
}

void twitch_close_cb(
    const struct _u_request *request,
    struct _websocket_manager *websocket_manager,
    void *user_data)
{
    printf("twitch ws close\n");
}

int callback_application(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    int rc;
    size_t sz = 0;
    char *buf;
    const char *appfilename = "/home/jason/d/doom/application.html";
    struct stat sb;
    FILE *f = NULL;
    rc = stat(appfilename, &sb);
    if (rc == -1) {
        ulfius_set_string_body_response(response, 500, "in stat");
        return U_CALLBACK_CONTINUE;
    }
    f = fopen(appfilename, "r");
    if (!f) {
        ulfius_set_string_body_response(response, 500, "in fopen");
        return U_CALLBACK_CONTINUE;
    }
    buf = (char *)malloc(sb.st_size);
    if (!buf) {
        fclose(f);
        ulfius_set_string_body_response(response, 500, "in malloc");
        return U_CALLBACK_CONTINUE;
    }
    sz = fread(buf, sb.st_size, 1, f);
    if (sz != 1) {
        free(buf);
        fclose(f);
        ulfius_set_string_body_response(response, 500, "in fread");
        return U_CALLBACK_CONTINUE;
    }

    ulfius_add_header_to_response(response, "Content-Type", "text/html");
    ulfius_set_binary_body_response(response, 200, buf, sb.st_size);
    return U_CALLBACK_CONTINUE;
}

void I_WebDestroy(void)
{
    // clean up web server
    ulfius_stop_framework(&web);
    ulfius_clean_instance(&web);
}

int I_WebInit(void)
{
    int port = 8080;
    int rc;
    struct _u_request request;
    struct _u_response response;
    struct _websocket_client_handler twitch_client_handler = {NULL, NULL};

    if (ulfius_init_instance(&web, port, NULL, NULL) != U_OK) {
        printf("failure in ulfius_init_instance\n");
        return -1;
    }

    ulfius_add_endpoint_by_val(
        &web,
        "GET",
        "/",
        NULL,
        0,
        &callback_application,
        NULL
    );

    for (port = 8080; port < 8090; port++) {
        web.port = port;
        if (ulfius_start_framework(&web) == U_OK) {
            break;
        }
    }
    if (port == 8090) {
        printf("couldn't start ulfius\n");
        return -1;
    }
    printf("web ok: http://localhost:%d\n", port);

    rc = pthread_mutex_init(&webmutex, NULL);
    if (rc != 0) err(1, "couldn't create pthread mutex");
    rc = pthread_cond_init(&webcond, NULL);
    if (rc != 0) err(1, "couldn't create pthread condition");

    I_AtExit(killall_managers, 1);

    ulfius_add_endpoint_by_val(
        &web,
        "GET",
        NULL,
        "/ws",
        0,
        &callback_ws,
        NULL
    );

    if (!getenv("CHOCO_AUTH") || !getenv("CHOCO_CID")) {
        printf("CHOCO_AUTH or CHOCO_CID not set, so skipping twitch api init\n");
    } else {
        ulfius_init_request(&request);
        ulfius_init_response(&response);
        rc = ulfius_set_websocket_request(
            &request,
            "wss://eventsub-beta.wss.twitch.tv/ws",
            NULL,
            NULL
        );
        if (rc != U_OK) errx(1, "couldn't set twitch ws request");

        rc = ulfius_open_websocket_client_connection(
            &request,
            &twitch_manager_cb, NULL,
            &twitch_message_cb, NULL,
            &twitch_close_cb, NULL,
            &twitch_client_handler,
            &response
        );
        if (rc != U_OK) errx(1, "couldn't open twitch ws client connection");
        printf("opened twitch ws\n");
    }
    return 0;
}


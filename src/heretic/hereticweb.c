#include <ulfius.h>
#include "i_web.h"
#include "hereticweb.h"
#include "jrra_report.h"
#include <sys/stat.h>
#include "doomdef.h"

int callback_doominfo(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    json_t *j;
    if (jrra_info.valid == 0) {
        ulfius_set_empty_body_response(response, 200);
        return U_CALLBACK_CONTINUE;
    }
    j = json_object();
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
    ulfius_add_header_to_response(response, "Content-Type", "application/json");
    ulfius_set_json_body_response(response, 200, j);
    json_decref(j);
    j = NULL;
	return U_CALLBACK_CONTINUE;
}

void I_HereticWebInit(void)
{
	I_WebInit();
	ulfius_add_endpoint_by_val(
		&web,
		"GET",
		"/doominfo",
		NULL,
		0,
		&callback_doominfo,
		NULL
	);
}


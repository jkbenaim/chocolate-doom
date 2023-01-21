#include <ulfius.h>
#include "i_web.h"
#include <sys/stat.h>

struct _u_instance web;

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

int I_WebInit(void)
{
    int port = 8080;
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
    return 0;
}

void I_WebDestroy(void)
{
    // clean up web server
    ulfius_stop_framework(&web);
    ulfius_clean_instance(&web);
}

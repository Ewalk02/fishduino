#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"

#include "argtable3/argtable3.h"

#include "chihiros_heater_client.h"

static const char *TAG = "chihiros_console";

static chihiros_heater_client_t *s_client;

static struct {
    struct arg_dbl *f;
    struct arg_end *end;
} set_args;

static int cmd_connect(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!s_client) return 1;
    chihiros_heater_client_request_connect(s_client);
    printf("connect requested\n");
    return 0;
}

static int cmd_disconnect(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!s_client) return 1;
    chihiros_heater_client_request_disconnect(s_client);
    printf("disconnect requested\n");
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!s_client) return 1;
    chihiros_heater_client_state_t st = chihiros_heater_client_get_state(s_client);
    printf("connected=%s subscribed=%s stale=%s\n",
           st.connected ? "yes" : "no",
           st.subscribed ? "yes" : "no",
           st.stale ? "yes" : "no");

    if (st.last_status.valid) {
        printf("temp=%.2fC %.2fF watts=%" PRIu16 " heating=%s\n",
               (double)st.last_status.current_temp_c,
               (double)st.last_status.current_temp_f,
               st.last_status.watts,
               st.last_status.heating ? "yes" : "no");
    } else {
        printf("temp=invalid\n");
    }

    if (st.has_last_setpoint) {
        printf("last_setpoint_f=%.1f\n", (double)st.last_setpoint_f);
    } else {
        printf("last_setpoint_f=unset\n");
    }

    return 0;
}

static int cmd_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_args.end, argv[0]);
        return 1;
    }

    double f = *set_args.f->dval;
    if (!s_client) return 1;
    bool ok = chihiros_heater_client_set_setpoint_f(s_client, (float)f);
    printf("set %.1f => %s\n", f, ok ? "ok" : "failed");
    return ok ? 0 : 1;
}

void chihiros_heater_console_register(chihiros_heater_client_t *client)
{
    s_client = client;

    const esp_console_cmd_t connect_cmd = {
        .command = "connect",
        .help = "Scan DYH1*, connect, subscribe, init",
        .hint = NULL,
        .func = &cmd_connect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&connect_cmd));

    const esp_console_cmd_t disconnect_cmd = {
        .command = "disconnect",
        .help = "Disconnect from heater",
        .hint = NULL,
        .func = &cmd_disconnect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&disconnect_cmd));

    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Show latest decoded status",
        .hint = NULL,
        .func = &cmd_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    set_args.f = arg_dbl1(NULL, NULL, "<F>", "Setpoint in Fahrenheit (e.g. 77.5)");
    set_args.end = arg_end(2);

    const esp_console_cmd_t set_cmd = {
        .command = "set",
        .help = "Send setpoint (F) to heater",
        .hint = NULL,
        .func = &cmd_set,
        .argtable = &set_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));

    ESP_LOGI(TAG, "Commands: connect, status, set <F>, disconnect");
}


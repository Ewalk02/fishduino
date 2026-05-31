#include <stdio.h>
#include <stdlib.h>

#include "esp_console.h"
#include "heater/heater_manager.h"

static int cmd_heater_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    heater_status_t st;
    heater_manager_get_status(&st);
    printf("enabled=%s state=%s temp=%.1fF target=%.1fF heating=%s online=%s stale=%s\n",
           st.enabled ? "yes" : "no", heater_manager_state_text(st.state), (double)st.reported_temp_f,
           (double)st.target_temp_f, st.heating ? "yes" : "no", st.online ? "yes" : "no",
           st.stale ? "yes" : "no");
    if (st.alarm != HEATER_ALARM_NONE) {
        printf("alarm: %s (%s)\n", heater_manager_alarm_text(st.alarm), st.error_text);
    }
    return 0;
}

static int cmd_heater_set(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: heater_set <temp_F>\n");
        return 1;
    }
    float f = (float)atof(argv[1]);
    esp_err_t err = heater_manager_set_target_temp_f(f);
    printf("heater_set %.1f => %s\n", (double)f, err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

static int cmd_heater_enable(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: heater_enable 0|1\n");
        return 1;
    }
    bool en = argv[1][0] == '1';
    esp_err_t err = heater_manager_set_enabled(en);
    printf("heater_enable %d => %s\n", (int)en, err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

void fishduino_heater_console_register(void)
{
    const esp_console_cmd_t status = {
        .command = "heater_status",
        .help = "Chihiros heater status",
        .func = &cmd_heater_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status));

    const esp_console_cmd_t set = {
        .command = "heater_set",
        .help = "Set heater target temp (F)",
        .func = &cmd_heater_set,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set));

    const esp_console_cmd_t enable = {
        .command = "heater_enable",
        .help = "Enable/disable heater BLE",
        .func = &cmd_heater_enable,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&enable));
}

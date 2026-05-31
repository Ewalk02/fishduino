#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "ota/ota_manager.h"

static int cmd_ota_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("version: %s\n", fishduino_ota_get_version_string());
    printf("state: %s\n", fishduino_ota_manager_state_text(fishduino_ota_manager_get_state()));
    return 0;
}

static int cmd_ota_update(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: ota_update <https_url>\n");
        return 1;
    }
    esp_err_t err = fishduino_ota_start_url(argv[1]);
    printf("ota_update => %s\n", err == ESP_OK ? "started" : "fail");
    return err == ESP_OK ? 0 : 1;
}

static int cmd_ota_confirm_good(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = fishduino_ota_confirm_good();
    printf("ota_confirm_good => %s\n", err == ESP_OK ? "ok" : "fail");
    return err == ESP_OK ? 0 : 1;
}

void fishduino_ota_console_register(void)
{
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota_status",
        .help = "OTA / firmware version status",
        .func = &cmd_ota_status,
    }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota_update",
        .help = "HTTPS OTA update from URL",
        .func = &cmd_ota_update,
    }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota_confirm_good",
        .help = "Mark OTA image valid (cancel rollback)",
        .func = &cmd_ota_confirm_good,
    }));
}

#include <stdio.h>
#include <stdlib.h>

#include "esp_console.h"
#include "maintenance/maintenance_mode.h"

static int cmd_maintenance_start(int argc, char **argv)
{
    uint32_t min = 30;
    if (argc >= 2) {
        min = (uint32_t)atoi(argv[1]);
    }
    fishduino_maintenance_mode_start(min);
    printf("Maintenance started (%u min)\n", (unsigned)min);
    return 0;
}

static int cmd_maintenance_end(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fishduino_maintenance_mode_end();
    printf("Maintenance ended\n");
    return 0;
}

static int cmd_maintenance_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("active=%s remaining_ms=%lld\n", fishduino_maintenance_mode_is_active() ? "yes" : "no",
           (long long)fishduino_maintenance_mode_remaining_ms());
    return 0;
}

void fishduino_maintenance_console_register(void)
{
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "maintenance_start",
        .help = "Start maintenance mode [minutes]",
        .func = &cmd_maintenance_start,
    }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "maintenance_end",
        .help = "End maintenance mode",
        .func = &cmd_maintenance_end,
    }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "maintenance_status",
        .help = "Maintenance mode status",
        .func = &cmd_maintenance_status,
    }));
}

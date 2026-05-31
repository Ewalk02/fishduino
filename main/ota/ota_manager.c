#include "ota_manager.h"
#include "fishduino_version.h"

#include <string.h>

#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shelly/shelly_manager.h"

static const char *TAG = "ota_mgr";

static fishduino_ota_state_t s_state = FISHDUINO_OTA_IDLE;
static char s_last_error[96];
static bool s_health_confirmed;

static const char *s_version_str = FISHDUINO_BUILD_VERSION;

const char *fishduino_ota_get_version_string(void)
{
    return s_version_str != NULL ? s_version_str : "unknown";
}

const char *fishduino_ota_manager_state_text(fishduino_ota_state_t st)
{
    switch (st) {
    case FISHDUINO_OTA_DOWNLOADING:
        return "downloading";
    case FISHDUINO_OTA_VALIDATING:
        return "validating";
    case FISHDUINO_OTA_PENDING_REBOOT:
        return "pending reboot";
    case FISHDUINO_OTA_SUCCESS:
        return "success";
    case FISHDUINO_OTA_FAILED:
        return "failed";
    default:
        return "idle";
    }
}

fishduino_ota_state_t fishduino_ota_manager_get_state(void)
{
    return s_state;
}

void fishduino_ota_manager_init(void)
{
    s_state = FISHDUINO_OTA_IDLE;
    s_last_error[0] = '\0';

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (running != NULL &&
        esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "First boot after OTA — awaiting health confirm");
    }
}

static void ota_task(void *arg)
{
    const char *url = (const char *)arg;
    s_state = FISHDUINO_OTA_DOWNLOADING;

    fishduino_shelly_co2_command_now(false);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    s_state = FISHDUINO_OTA_VALIDATING;
    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err != ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "%s", esp_err_to_name(err));
        s_state = FISHDUINO_OTA_FAILED;
        ESP_LOGE(TAG, "OTA failed: %s", s_last_error);
        vTaskDelete(NULL);
        return;
    }

    s_state = FISHDUINO_OTA_PENDING_REBOOT;
    ESP_LOGI(TAG, "OTA complete; rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

esp_err_t fishduino_ota_start_url(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_state == FISHDUINO_OTA_DOWNLOADING || s_state == FISHDUINO_OTA_VALIDATING) {
        return ESP_ERR_INVALID_STATE;
    }

    static char url_copy[256];
    strncpy(url_copy, url, sizeof(url_copy) - 1);
    url_copy[sizeof(url_copy) - 1] = '\0';

    if (xTaskCreate(ota_task, "ota_task", 8192, url_copy, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t fishduino_ota_confirm_good(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (running == NULL) {
        return ESP_FAIL;
    }

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) {
            return err;
        }
    }

    s_health_confirmed = true;
    ESP_LOGI(TAG, "App marked valid (rollback cancelled)");
    return ESP_OK;
}

void fishduino_ota_manager_tick(void)
{
    if (s_health_confirmed) {
        return;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (running == NULL) {
        return;
    }

    if (esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
        return;
    }

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        static uint32_t boot_ms;
        if (boot_ms == 0) {
            boot_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        }
        if ((xTaskGetTickCount() * portTICK_PERIOD_MS - boot_ms) > 45000) {
            fishduino_ota_confirm_good();
        }
    }
}

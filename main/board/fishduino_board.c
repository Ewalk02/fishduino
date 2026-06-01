#include "fishduino_board.h"

#include "sdkconfig.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#if CONFIG_FISHDUINO_BOARD_7B
#include "board_7b.h"
#else
#include "board_4in.h"
#endif

static const char *TAG = "fishduino_board";

esp_err_t fishduino_board_init(void)
{
    ESP_LOGI(TAG, "Fishduino hardware target: %s", fishduino_board_name());
    return ESP_OK;
}

lv_display_t *fishduino_display_start(void)
{
#if CONFIG_FISHDUINO_BOARD_7B
    return board_7b_display_start();
#else
    return board_4in_display_start();
#endif
}

bool fishduino_display_lock(uint32_t timeout_ms)
{
#if CONFIG_FISHDUINO_BOARD_7B
    return board_7b_display_lock(timeout_ms);
#else
    return board_4in_display_lock(timeout_ms);
#endif
}

void fishduino_display_unlock(void)
{
#if CONFIG_FISHDUINO_BOARD_7B
    board_7b_display_unlock();
#else
    board_4in_display_unlock();
#endif
}

void fishduino_display_backlight_on(void)
{
#if CONFIG_FISHDUINO_BOARD_7B
    board_7b_display_backlight_on();
#else
    board_4in_display_backlight_on();
#endif
}

int fishduino_display_width(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return 0;
    }
    return (int)lv_display_get_horizontal_resolution(disp);
}

int fishduino_display_height(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return 0;
    }
    return (int)lv_display_get_vertical_resolution(disp);
}

bool fishduino_board_is_4in(void)
{
#if CONFIG_FISHDUINO_BOARD_4IN
    return true;
#else
    return false;
#endif
}

bool fishduino_board_is_7b(void)
{
#if CONFIG_FISHDUINO_BOARD_7B
    return true;
#else
    return false;
#endif
}

const char *fishduino_board_name(void)
{
#if CONFIG_FISHDUINO_BOARD_7B
    return "7B (ESP32-P4-WIFI6-7inch-Touch-LCD)";
#else
    return "4IN (ESP32-P4-WIFI6 + 4-DSI-TOUCH-A)";
#endif
}

void fishduino_board_log_status(void)
{
    lv_display_t *disp = lv_display_get_default();
    const int w = fishduino_display_width();
    const int h = fishduino_display_height();

    ESP_LOGI(TAG, "Display: %dx%d (LVGL default display %s)",
             w, h, disp ? "ok" : "missing");

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB565
    ESP_LOGI(TAG, "LCD color format: RGB565");
#elif CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    ESP_LOGI(TAG, "LCD color format: RGB888");
#else
    ESP_LOGI(TAG, "LCD color format: (see BSP menuconfig)");
#endif

    const size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_total > 0) {
        ESP_LOGI(TAG, "PSRAM: enabled, total=%u free=%u bytes",
                 (unsigned)psram_total, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        ESP_LOGW(TAG, "PSRAM: not available");
    }

    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev != NULL) {
        ESP_LOGI(TAG, "Touch: LVGL input device registered");
    } else {
        ESP_LOGW(TAG, "Touch: no LVGL input device");
    }
}

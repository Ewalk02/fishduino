#include "scheduler.h"

#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "scheduler";

void fishduino_time_snapshot_now(fishduino_time_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    time_t t = time(NULL);
    out->epoch_seconds = (uint32_t)(t > 0 ? t : 0);

    struct tm local_tm;
    if (t > 100000 && localtime_r(&t, &local_tm) != NULL) {
        out->valid_time = true;
        out->minutes_since_midnight = (uint16_t)(local_tm.tm_hour * 60 + local_tm.tm_min);
    }
}

typedef struct {
    fishduino_scheduler_tick_fn cb;
    void *ctx;
} scheduler_state_t;

static void scheduler_task(void *arg)
{
    scheduler_state_t *st = (scheduler_state_t *)arg;

    while (true) {
        fishduino_time_snapshot_t now;
        fishduino_time_snapshot_now(&now);

        st->cb(&now, st->ctx);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void fishduino_scheduler_start(fishduino_scheduler_tick_fn tick_cb, void *tick_ctx)
{
    static scheduler_state_t st;
    st.cb = tick_cb;
    st.ctx = tick_ctx;

    if (xTaskCreate(scheduler_task, "fish_sched", 4096, &st, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scheduler task");
    } else {
        ESP_LOGI(TAG, "Scheduler started");
    }
}


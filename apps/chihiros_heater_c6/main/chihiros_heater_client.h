#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "chihiros_heater_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool connected;
    bool subscribed;

    float last_setpoint_f;
    bool has_last_setpoint;

    chihiros_status_t last_status;
    uint32_t last_status_ms;  // uptime ms
    bool stale;
} chihiros_heater_client_state_t;

typedef struct chihiros_heater_client chihiros_heater_client_t;

typedef void (*chihiros_heater_client_state_cb_t)(const chihiros_heater_client_state_t *state, void *ctx);

typedef struct {
    // Scan/connect behavior
    const char *name_prefix;      // "DYH1"
    uint32_t stale_timeout_ms;    // default 10000
    bool keepalive_enabled;       // default false
    uint32_t keepalive_period_ms; // unused unless enabled

    // Setpoint safety clamp
    float min_setpoint_f;  // default 50
    float max_setpoint_f;  // default 95
} chihiros_heater_client_config_t;

bool chihiros_heater_client_init(chihiros_heater_client_t **out_client,
                                 const chihiros_heater_client_config_t *cfg,
                                 chihiros_heater_client_state_cb_t cb,
                                 void *cb_ctx);

void chihiros_heater_client_start_task(chihiros_heater_client_t *client);

void chihiros_heater_client_request_connect(chihiros_heater_client_t *client);
void chihiros_heater_client_request_disconnect(chihiros_heater_client_t *client);
bool chihiros_heater_client_set_setpoint_f(chihiros_heater_client_t *client, float target_f);

chihiros_heater_client_state_t chihiros_heater_client_get_state(chihiros_heater_client_t *client);

#ifdef __cplusplus
}
#endif


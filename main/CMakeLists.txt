set(fishduino_srcs
  "main.c"
  "storage/settings_nvs.c"
  "storage/settings_runtime.c"
  "storage/wifi_creds_nvs.c"
  "scheduler/scheduler.c"
  "co2/co2_schedule.c"
  "co2/co2_gpio.c"
  "feeder/feeder_schedule.c"
  "feeder/feeder_actuator.c"
  "net/wifi_manager.c"
  "net/time_sync.c"
  "net/shelly_client.c"
  "net/shelly_address.c"
  "net/shelly_console.c"
  "net/safety_console.c"
  "net/status_console.c"
  "net/dashboard_console.c"
  "net/heater_console.c"
  "net/maintenance_console.c"
  "net/ota_console.c"
  "net/water_console.c"
  "net/maint_tracker_console.c"
  "water/water_storage.c"
  "water/water_metrics.c"
  "water/water_alerts.c"
  "maint_tracker/maint_tracker.c"
  "shelly/shelly_manager.c"
  "safety/co2_safety.c"
  "maintenance/maintenance_mode.c"
  "ota/ota_manager.c"
  "ble/ble_central_manager.c"
  "heater/chihiros_ble_client.c"
  "heater/heater_manager.c"
  "heater/heater_safety.c"
  "fluval/fluval_protocol.c"
  "fluval/fluval_transport.c"
  "fluval/fluval_stub_transport.c"
  "fluval/fluval_uart_transport.c"
  "fluval/fluval_light.c"
  "net/fluval_console.c"
  "ui/ui.c"
  "ui/dashboard_data.c"
  "ui/cockpit_gauge.c"
  "ui/screen_dashboard_cockpit.c"
  "ui/screen_options.c"
  "ui/screen_wifi_settings.c"
  "ui/screen_shelly_settings.c"
  "ui/screen_co2_schedule.c"
  "ui/screen_commissioning.c"
  "ui/screen_diagnostics.c"
  "ui/screen_fluval_settings.c"
  "ui/screen_heater_settings.c"
  "ui/screen_maintenance.c"
  "ui/screen_water_tests.c"
  "ui/screen_water_entry.c"
  "ui/screen_water_history.c"
  "ui/screen_maint_tracker.c"
  "board/fishduino_board.c"
  "ui/fishduino_ui_layout.c"
  "ui/screen_dashboard_cockpit_wide.c"
)

if(CONFIG_FISHDUINO_BOARD_7B)
  list(APPEND fishduino_srcs "board/board_7b.c")
else()
  list(APPEND fishduino_srcs "board/board_4in.c")
endif()

set(fishduino_requires
  esp_wifi
  esp_netif
  esp_event
  esp_timer
  esp_http_client
  esp_https_ota
  app_update
  json
  lwip
  nvs_flash
  esp_partition
  console
  driver
  freertos
  chihiros_heater_protocol
  mbedtls
)

set(fishduino_priv_requires "")
if(CONFIG_FISHDUINO_FLUVAL_HOSTED_BLE)
  list(APPEND fishduino_srcs
    "fluval/fluval_ble_client.c"
    "fluval/fluval_ble_transport.c"
  )
endif()

idf_component_register(SRCS ${fishduino_srcs}
  INCLUDE_DIRS "." "ui" "board"
  REQUIRES ${fishduino_requires} bt
  PRIV_REQUIRES ${fishduino_priv_requires}
)

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
  "shelly/shelly_manager.c"
  "fluval/fluval_protocol.c"
  "fluval/fluval_transport.c"
  "fluval/fluval_stub_transport.c"
  "fluval/fluval_uart_transport.c"
  "fluval/fluval_light.c"
  "net/fluval_console.c"
  "ui/ui.c"
  "ui/screen_dashboard.c"
  "ui/screen_options.c"
  "ui/screen_wifi_settings.c"
  "ui/screen_shelly_settings.c"
  "ui/screen_co2_schedule.c"
  "ui/screen_commissioning.c"
  "ui/screen_diagnostics.c"
  "ui/screen_fluval_settings.c"
)

set(fishduino_requires
  esp_wifi
  esp_netif
  esp_event
  esp_timer
  esp_http_client
  json
  lwip
  nvs_flash
  console
  driver
  freertos
)

set(fishduino_priv_requires "")
if(CONFIG_FISHDUINO_FLUVAL_HOSTED_BLE)
  list(APPEND fishduino_srcs
    "fluval/fluval_ble_client.c"
    "fluval/fluval_ble_transport.c"
  )
endif()

idf_component_register(SRCS ${fishduino_srcs}
  INCLUDE_DIRS "."
  REQUIRES ${fishduino_requires} bt
)

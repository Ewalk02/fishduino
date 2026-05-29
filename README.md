# Fishduino (ESP32-P4-WIFI6 + 4-DSI-TOUCH-A)

This project targets the Waveshare **ESP32-P4-WIFI6** development board with a Waveshare **4-DSI-TOUCH-A** (480×800, MIPI-DSI 2-lane, GT911 touch).

## Prerequisites (Ubuntu)

- Install **ESP-IDF** with `esp32p4` support (recommended: ESP-IDF **v5.5+**).
  - Espressif guide: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html`
- Optional (recommended): VS Code + ESP-IDF extension.
- USB serial permissions (typical):

```bash
sudo usermod -aG dialout "$USER"
```

Log out/in after changing groups.

## Build / flash (CLI)

From an ESP-IDF **5.5.4** environment shell:

```bash
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
cd "/home/surfacepro/fishduino"
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

In `menuconfig`, set:
`Component config → Board Support Package(ESP32-P4) → Display → Select LCD type → Waveshare 4-DSI-TOUCH-A Display`

If the board is not detected for flashing, hold **BOOT** while tapping **RST** to enter download mode.

## Display / touch

We use Waveshare’s ESP32-P4 BSP component `waveshare/esp32_p4_platform` (see `idf_component.yml`).

In `menuconfig`:

- `Component config → Board Support Package(ESP32-P4) → Display → Select LCD type → Waveshare 4-DSI-TOUCH-A Display`
- Color format: start with RGB565.

## Actuators (CO2 + feeder)

GPIO assignments are centralized in `main/hardware_pins.h`.

Before connecting real loads (solenoid/relay/servo), use the in-app “GPIO test” first.

## Notes: Chihiros inline heater (future)

Chihiros inline heaters (e.g. “Heater Pro”) are typically controlled via **Bluetooth LE** using the vendor app, not UART.

Planned integration approach (phase 2):

- Use the onboard ESP32-C6 BLE stack to connect to the heater.
- Reverse engineer the GATT services/characteristics and command frames (often vendor-specific “UART over BLE” patterns).
- Only after commands are reliable, add UI for setpoint and (later) closed-loop control.

Until then, Fishduino will focus on CO2 + feeder and can optionally read temperature from a dedicated aquarium temp sensor (DS18B20, etc.) in a later phase.

## Wi-Fi + Shelly smart plugs

Fishduino uses **ESP-Hosted** (onboard ESP32-C6) for Wi-Fi. Configure credentials once per machine:

```bash
idf.py menuconfig
```

- **Fishduino Configuration** → **WiFi SSID** and **WiFi Password** (first-time / fallback defaults)
- **Component config** → **Wi-Fi Remote** → slave target **esp32c6**

Or press `/` in menuconfig and search for `WiFi SSID`.

**On the touchscreen:** **OPTIONS** → **Wi-Fi Settings** — edit SSID and password, tap **SAVE & CONNECT**. Credentials are stored in NVS (on-device) and used on every boot. Menuconfig values apply only until you save from the UI once.

`sdkconfig` is gitignored; do not commit it.

Set Shelly plug IPs in [`main/shelly/shelly_config.local.h`](main/shelly/shelly_config.local.h) (copy from [`shelly_config.local.h.example`](main/shelly/shelly_config.local.h.example); this file is gitignored), then rebuild. Wi-Fi SSID/password belong in `menuconfig` only — `sdkconfig` is gitignored.

```c
#define FISHDUINO_CO2_SHELLY_IP_DEFAULT    "192.168.1.100"
#define FISHDUINO_FILTER_SHELLY_IP_DEFAULT "192.168.1.101"
```

Assign **static DHCP** leases to both plugs on your router.

**CO2 schedule** (default 09:00–17:00 local time): `co2.on_min` / `co2.off_min` in NVS. Enable with **OPTIONS → CO2 schedule ON**, serial `co2_schedule on`, or after first boot defaults. Dashboard shows **CO2: SCHEDULE DISABLED** when off.

**Timezone** defaults to **US Central** (DST via POSIX TZ). Change under **OPTIONS** (Eastern / Central / Mountain / Pacific) or serial `co2_schedule status` shows current zone.

**Filter monitoring** is read-only: Fishduino never sends `Switch.Set` to the filter plug. When the filter monitor is offline, the UI shows last-known watts and age.

### Shelly authentication

Fishduino uses **unauthenticated** local HTTP GET RPC (`/rpc/Switch.GetStatus`, `/rpc/Switch.Set`). **Disable LAN authentication** on both Shelly plugs (Shelly app → device settings → Authentication). If auth is enabled, requests return HTTP 401 and Fishduino logs a warning; digest auth is not supported.

### Serial commands (`idf.py monitor`)

| Command | Description |
|---------|-------------|
| `shelly` | Print CO2 and filter Shelly status (+ filter last-known) |
| `shelly_co2_on` / `shelly_co2_off` | Test CO2 plug switch |
| `shelly_filter` | Poll filter plug (read-only) |
| `shelly_alarm` | Filter alert state |
| `co2_schedule on` / `off` / `status` | Enable/disable CO2 schedule (saved to NVS) |

Touch **CO2 ON / OFF / AUTO** on the dashboard for manual override (clears at next schedule ON or OFF time). **OPTIONS** opens timezone and schedule toggles.


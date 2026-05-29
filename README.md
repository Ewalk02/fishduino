# Fishduino (ESP32-P4-WIFI6 + 4-DSI-TOUCH-A)

This project targets the Waveshare **ESP32-P4-WIFI6** development board with a Waveshare **4-DSI-TOUCH-A** (480×800, MIPI-DSI 2-lane, GT911 touch).

## Prerequisites (Ubuntu)

- Install **ESP-IDF** with `esp32p4` support (recommended: ESP-IDF **v5.5.4**).
  - Espressif guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html
- Optional (recommended): VS Code + ESP-IDF extension.
- USB serial permissions:

```bash
sudo usermod -aG dialout "$USER"
```

Log out/in after changing groups.

## Activate ESP-IDF

From the project directory:

```bash
cd ~/fishduino
source scripts/fishduino-env.sh
```

Or use the full path:

```bash
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
```

**Troubleshooting:** If you see `idf.py: command not found`, ESP-IDF has not been sourced in this terminal. Run `source scripts/fishduino-env.sh` (or the export script above) and try again.

## menuconfig (display + Wi-Fi defaults)

```bash
source scripts/fishduino-env.sh
idf.py menuconfig
```

Set:

- `Component config → Board Support Package(ESP32-P4) → Display → Select LCD type → Waveshare 4-DSI-TOUCH-A Display`
- **Fishduino Configuration** → **WiFi SSID** and **WiFi Password** (first-time / fallback before on-screen save)
- `Component config → Wi-Fi Remote` → slave target **esp32c6**

Or press `/` in menuconfig and search for `WiFi SSID`.

`sdkconfig` is gitignored; do not commit it.

## Build

```bash
source scripts/fishduino-env.sh
./scripts/build.sh
```

Or manually:

```bash
source scripts/fishduino-env.sh
idf.py set-target esp32p4
idf.py build
```

## Flash and monitor

```bash
source scripts/fishduino-env.sh
idf.py -p /dev/ttyACM0 flash monitor
```

If the board is not detected, hold **BOOT** while tapping **RST** to enter download mode.

## Wi-Fi credentials

| Method | When to use |
|--------|-------------|
| **OPTIONS → Wi-Fi Settings → SAVE & CONNECT** | Preferred on device; stored in NVS, used every boot |
| **menuconfig → Fishduino Configuration** | First flash or dev machine fallback until you save from the UI |

## Shelly plug IPs

| Method | When to use |
|--------|-------------|
| **OPTIONS → Shelly Settings → SAVE** | Preferred; stored in NVS |
| **`main/shelly/shelly_config.local.h`** | Optional compile-time defaults for **fresh NVS only** (copy from `shelly_config.local.h.example`; gitignored) |

Also set **CO2/filter enabled**, **switch id** (default 0), and assign static DHCP leases on your router.

**Shelly authentication:** disable LAN authentication on both plugs. Fishduino uses unauthenticated HTTP GET RPC only.

## First test with lamps, not aquarium equipment

Before connecting a CO2 solenoid or filter pump:

1. Plug **test lamps** into the CO2 and filter Shelly outlets (same wattage class as your loads is ideal).
2. Configure Wi-Fi and Shelly IPs (above).
3. Run **OPTIONS → Safety Test** on the touchscreen, or serial `safety_test` in the monitor. Use **OPTIONS → Diagnostics** or serial `status` to verify active settings.
4. Complete all 7 steps:
   - CO2 lamp ON / OFF
   - CO2 AUTO schedule behavior
   - Filter watts visible on dashboard
   - Filter plug `output=false` → **FILTER IS OFF**
   - Filter plug offline → **FILTER MONITOR OFFLINE**
   - Confirm Fishduino never sends `Switch.Set` to the filter plug (step 7 / serial note)
5. Use **FILTER CALIBRATE** (or `filter_calibrate`) with the filter lamp **on** for 30 seconds to set power baseline.

Only after lamps behave correctly, connect real aquarium equipment.

## CO2 schedule

Default window 09:00–17:00 (local). Edit under **OPTIONS → CO2 Schedule**. Enable schedule under **OPTIONS → CO2 schedule ON** or serial `co2_schedule on`. Schedules that cross midnight (`on_min > off_min`) are supported.

**Timezone** defaults to US Central. Change under **OPTIONS**.

## Filter monitoring

Read-only: Fishduino never sends `Switch.Set` to the filter plug. Low-power alarm uses calibrated baseline (50% of baseline, minimum 5 W).

## Serial commands (`idf.py monitor`)

| Command | Description |
|---------|-------------|
| `shelly` | CO2/filter status + active IPs |
| `shelly_co2_on` / `shelly_co2_off` | Test CO2 plug |
| `shelly_filter` | Poll filter (read-only) |
| `shelly_alarm` | Filter alert state |
| `co2_schedule on` / `off` / `status` | CO2 schedule enable |
| `co2_schedule times 09:00 17:00` | Set ON/OFF times |
| `filter_calibrate` | 30 s watt baseline (filter lamp on) |
| `status` | Active Wi-Fi, device IP, Shelly, CO2 schedule, filter calibration |
| `safety_test` | Commissioning checklist |

Touch **CO2 ON / OFF / AUTO** on the dashboard for manual override.

## Display / touch

Waveshare BSP `waveshare/esp32_p4_platform` (see `idf_component.yml`). Color format: RGB565.

## Actuators (CO2 + feeder)

GPIO assignments in `main/hardware_pins.h`.

## Chihiros heater (future)

Planned BLE integration via ESP32-C6; not in this firmware yet.

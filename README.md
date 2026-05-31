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

## Headless mode (no DSI panel)

Use when the MIPI DSI display is **not connected** and you want to test serial console / backend features (`water_add`, `maint_list`, Shelly, etc.) without the display init hang.

**Enable** (either method):

1. `idf.py menuconfig` → **Fishduino Configuration** → **Headless mode (skip display/touch/LVGL)** → enable
2. Uncomment in `sdkconfig.defaults`:
   ```text
   CONFIG_FISHDUINO_HEADLESS=y
   ```

Then regenerate config and build:

```bash
rm -f sdkconfig
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

On boot you should see: `Headless mode enabled: skipping display/touch/LVGL init` and the `fishduino>` prompt.

**Disable:** turn off in menuconfig or re-comment the line in `sdkconfig.defaults`, then `rm -f sdkconfig`, `idf.py set-target esp32p4`, and rebuild. Normal builds initialize the touchscreen UI when headless is off (default).

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
| `dashboard_status` | Print cockpit `dashboard_data` snapshot (headless UI validation) |
| `shelly_co2_on` / `shelly_co2_off` | Test CO2 plug |
| `shelly_filter` | Poll filter (read-only) |
| `shelly_alarm` | Filter alert state |
| `co2_schedule on` / `off` / `status` | CO2 schedule enable |
| `co2_schedule times 09:00 17:00` | Set ON/OFF times |
| `filter_calibrate` | 30 s watt baseline (filter lamp on) |
| `status` | Active Wi-Fi, device IP, Shelly, CO2 schedule, filter calibration |
| `fluval` | Fluval Plant 4.0 state |
| `fluval_read` | Request Fluval status |
| `fluval_manual` / `fluval_auto` | Fluval mode |
| `fluval_setall <0-100>` | Set all Fluval channels |
| `fluval_set <p> <b> <cw> <w> <ww>` | Set Fluval channels |
| `safety_test` | Commissioning checklist |
| `heater_status` / `heater_set` / `heater_enable` | Chihiros heater |
| `maintenance_start` / `maintenance_end` / `maintenance_status` | Maintenance Mode |
| `ota_status` / `ota_update` / `ota_confirm_good` | OTA |
| `co2_override_on` | Dangerous CO2 override (expires) |
| `water_add` / `water_latest` / `water_list` / `water_clear_confirm` | Water test log |
| `maint_list` / `maint_due` / `maint_done` / `maint_snooze` | Maintenance reminders |

Touch **CO2 ON / OFF / AUTO** on the dashboard for manual override. If CO2 is blocked by the interlock, the dashboard shows the reason.

## Display / touch

Waveshare BSP `waveshare/esp32_p4_platform` (see `idf_component.yml`). Color format: RGB565.

## Actuators (CO2 + feeder)

GPIO assignments in `main/hardware_pins.h`.

## Architecture (single P4 firmware)

Production ships **one** image on the ESP32-P4. The onboard ESP32-C6 runs **ESP-Hosted slave** (Wi-Fi remote + NimBLE HCI only). Policy, UI, Shelly HTTP, CO2 interlock, maintenance, OTA, Fluval, and Chihiros all live under `main/`.

```text
┌─────────────────────────────────────────────────────────────┐
│ ESP32-P4 Fishduino                                          │
│  LVGL UI → policy (CO2, maintenance, heater, OTA)           │
│         → Shelly HTTP (Wi-Fi via hosted)                    │
│         → ble_central_manager → Fluval / Chihiros drivers   │
└───────────────────────────┬─────────────────────────────────┘
                            │ ESP-Hosted (SDIO)
┌───────────────────────────▼─────────────────────────────────┐
│ ESP32-C6 coprocessor — Wi-Fi + NimBLE VHCI controller       │
└─────────────────────────────────────────────────────────────┘
```

| Layer | Modules | Must not |
|-------|---------|----------|
| UI | `main/ui/` | Direct BLE/HTTP |
| Policy | `co2_safety`, `maintenance_mode`, `heater_manager`, `ota_manager` | GATT/scan |
| BLE | `ble_central_manager`, `fluval_ble_client`, `chihiros_ble_client` | Duplicate `nimble_port_init` |
| Protocol | `chihiros_heater_protocol`, Fluval hex commands | Safety policy |

**Future fallback only:** UART to an external C6 running `apps/chihiros_heater_c6/` or `tools/fluval_ble_helper/` — not used on the onboard coprocessor.

Details: [`docs/chihiros_ble.md`](docs/chihiros_ble.md)

## CO2 safety interlock

CO2 solenoid ON is gated by `main/safety/co2_safety.c` everywhere: Shelly tick, manual UI, GPIO fallback, schedule, and boot restore.

CO2 stays **OFF** when:

- Filter baseline not calibrated (`filter_baseline_watts == 0`)
- Filter monitor offline or stale
- Filter plug output off or below running threshold (with hysteresis)
- **Maintenance Mode** active

The dashboard shows `CO2 blocked: <reason>` when blocked. Filter plug remains **read-only** (no `Switch.Set` to filter IP).

Dangerous override (auto-expires): serial `co2_override_on <minutes>`.

## Maintenance Mode

**OPTIONS → Maintenance** (15 / 30 / 60 min) or serial `maintenance_start <min>` / `maintenance_end` / `maintenance_status`.

- Persists end time in NVS; turns CO2 off on start
- Suppresses non-critical filter OFF / LOW_POWER alarms
- **Does not** suppress heater over-temp

## Water test logging and reminders

Manual entry of pH, ammonia, nitrite, nitrate (and optional notes). **Not** auto-sensed.

- **OPTIONS → Water Tests** — add entry, view history/chart, latest on dashboard
- **OPTIONS → Reminders** — recurring tasks (filter rinse, water tests, etc.)
- Storage: flash ring buffer (`waterlog` partition), 200 entries, oldest dropped when full
- Saving a water test marks **Check water parameters** done (+14 days)

Distinct from **Maintenance Mode** above (temporary CO2-off).

Details: [`docs/water_logging.md`](docs/water_logging.md)

## OTA with rollback

Custom partition table [`partitions.csv`](partitions.csv): `factory` + `ota_0` + `ota_1` (**3 MB** app slots each), `otadata`, and a **64 KB** `waterlog` data partition for the water-test ring buffer. Requires **16 MB** flash (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB` in `sdkconfig.defaults`). After changing defaults, delete local `sdkconfig` and run `idf.py set-target esp32p4` so flash size is not stuck at 2 MB.

- `ota_status`, `ota_update <https_url>`, `ota_confirm_good` on serial console
- Pre-reboot: CO2 forced off
- Pending firmware is confirmed with `esp_ota_mark_app_valid_cancel_rollback()` after health window (~45 s)

Build version string: `FISHDUINO_BUILD_VERSION` in `main/fishduino_version.h`.

## Chihiros aquarium heater

Integrated on P4 via hosted NimBLE (NUS, `DYH1*` name prefix). Enable under **OPTIONS → Heater**. Policy fail-safe uses minimum setpoint when unsafe (no dedicated OFF packet in protocol).

| Command | Description |
|---------|-------------|
| `heater_status` | BLE link, temp, target, alarms |
| `heater_set <temp_F>` | Set target °F |
| `heater_enable 0\|1` | Enable integration |

Lab/reference firmware (not production): [`apps/chihiros_heater_c6/`](apps/chihiros_heater_c6/)

## Fluval Plant 4.0 light

Primary path: P4 NimBLE central → onboard C6 → Fluval BLE (no UART required).

Optional UART fallback: [`tools/fluval_ble_helper/README.md`](tools/fluval_ble_helper/README.md)

```text
Fishduino ESP32-P4  --ESP-Hosted BLE-->  Fluval Plant 4.0
Fishduino ESP32-P4  --UART (fallback)-->  BLE helper  --BLE-->  Fluval
```

### BLE protocol (helper-side)

| Item | Value |
|------|-------|
| Device name | `Plant4.0_450467` (configurable) |
| Service | `FFF0` (`0000fff0-0000-1000-8000-00805f9b34fb`) |
| Notify | `FFF1` — responses |
| Write | `FFF2` — commands |

Confirmed commands (hex written to FFF2):

| Action | TX |
|--------|-----|
| Read status | `d0ff` |
| Set Manual | `d1a10100` |
| Set Auto | `d1a10101` |
| Set channels | `d1a60318<pink>0418<blue>0518<cold_white>0618<white>0718<warm_white>0e00` |

Status responses start with `d2b0000e01<mode>02f5...` where mode `00` = Manual, `01` = Auto. LED channels after marker `02 f5`: `03` Pink, `04` Blue, `05` Cold White, `06` White, `07` Warm White.

### Supported in Fishduino

- Read mode (Manual / Auto) and five channel percentages
- Set Manual / Auto mode
- Set manual channel percentages
- Periodic status polling (default 10 s)
- Dashboard display and basic controls
- Serial console commands

### Fluval BLE path

Fluval shares `ble_central_manager` with Chihiros (one connection, heater priority). See **Architecture** above.

### Not yet supported

- Editing the full Auto schedule (timepoints) stored in the light

### Transport modes (Kconfig + NVS)

| Mode | Description |
|------|-------------|
| Disabled | No Fluval I/O (default until enabled in UI) |
| Hosted BLE | Onboard ESP32-C6 via ESP-Hosted (primary) |
| UART helper | External ESP32-C3/S3/C6 running `tools/fluval_ble_helper/` |

Default build compiles hosted BLE (`CONFIG_FISHDUINO_FLUVAL_DEFAULT_TRANSPORT_HOSTED_BLE`). Integration stays off until **OPTIONS → Fluval Settings → Enable**.

**C6 note:** The coprocessor must run ESP-Hosted slave firmware with Bluetooth enabled (do not flash the standalone UART helper to the onboard C6).

Required `sdkconfig` options (in `sdkconfig.defaults` when hosted BLE is enabled):

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_CONTROLLER_DISABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y
CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=y
CONFIG_ESP_HOSTED_NIMBLE_HCI_VHCI=y
```

### UART helper protocol (fallback)

```text
FLUVAL READ
FLUVAL MODE MANUAL
FLUVAL MODE AUTO
FLUVAL SET <pink> <blue> <cold_white> <white> <warm_white>
FLUVAL SETALL <percent>
```

Helper → P4:

```text
FLUVAL STATE <mode> <pink> <blue> <cold_white> <white> <warm_white> AVG <avg> RSSI <rssi>
FLUVAL OK
FLUVAL ERROR <reason>
```

### Configuration

Fluval is **disabled by default**. Enable under **OPTIONS → Fluval Settings**. Default transport is **hosted BLE** on the onboard ESP32-C6; UART helper pins in `main/hardware_pins.h` remain placeholders (`-1`) unless using fallback mode. Defaults: target name `Plant4.0_450467`, poll 10 s, stale timeout 30 s.

Close FluvalConnect and turn off phone Bluetooth while Fishduino controls the light.

### Serial commands

| Command | Description |
|---------|-------------|
| `fluval` | Fluval state (mode, channels, link status) |
| `fluval_read` | Queue status read |
| `fluval_manual` / `fluval_auto` | Set Manual / Auto mode |
| `fluval_setall <0-100>` | Set all channels to same percent |
| `fluval_set <p> <b> <cw> <w> <ww>` | Set individual channels |

Dashboard buttons **LT AUTO**, **LT MAN**, and **LT 50%** queue the same actions (50% = set-all preset).

# Fluval BLE Helper

Standalone ESP-IDF firmware for an ESP32 (C3/S3/WROOM) that controls a **Fluval Plant 4.0** aquarium light over BLE and exposes a simple **UART line protocol** for AquaPilot or USB serial testing.

```text
AquaPilot ESP32-P4  --UART-->  Fluval BLE Helper  --BLE-->  Fluval Plant 4.0
```

## Purpose

The ESP32-P4 on the AquaPilot board does not run a native BLE client in the main firmware. This helper:

- Scans for and connects to `Plant4.0_450467`
- Sends confirmed Fluval GATT commands on FFF2
- Parses FFF1 notifications
- Responds to text commands (`FLUVAL READ`, `FLUVAL MODE AUTO`, etc.)

## Hardware

### First test: USB serial

Flash an ESP32-S3 (or C3) dev board and use `idf.py monitor`. Type commands at the serial prompt (115200 baud).

### Later: wire to AquaPilot P4

Connect a **second UART** between the helper and AquaPilot (not the AquaPilot console UART):

| Helper | AquaPilot P4 |
|--------|----------------|
| TX | RX (`FISHDUINO_FLUVAL_UART_RX`) |
| RX | TX (`FISHDUINO_FLUVAL_UART_TX`) |
| GND | GND |

Set pins in AquaPilot [`main/hardware_pins.h`](../main/hardware_pins.h) and enable Fluval in **OPTIONS → Fluval Settings**.

## Build and flash

```bash
cd tools/fluval_ble_helper
source ../../scripts/fishduino-env.sh   # or your ESP-IDF export
idf.py set-target esp32s3
idf.py build flash monitor
```

Retarget to ESP32-C3:

```bash
idf.py set-target esp32c3
idf.py build
```

## Serial commands

| Command | Description |
|---------|-------------|
| `HELP` | List commands |
| `FLUVAL READ` | Query light status over BLE |
| `FLUVAL STATUS` | Print last cached state (no BLE I/O) |
| `FLUVAL MODE MANUAL` | Set Manual mode |
| `FLUVAL MODE AUTO` | Set Auto mode (saved schedule in light) |
| `FLUVAL SETALL <0-100>` | Set all channels to same percent |
| `FLUVAL SET <p> <b> <cw> <w> <ww>` | Set individual channels |

### Example session

```text
FLUVAL READ
FLUVAL STATE MANUAL 40 20 60 70 50 AVG 48 RSSI -58

FLUVAL MODE AUTO
FLUVAL OK
FLUVAL STATE AUTO 40 20 60 70 50 AVG 48 RSSI -58

FLUVAL SET 40 20 60 70 50
FLUVAL OK
FLUVAL STATE MANUAL 40 20 60 70 50 AVG 48 RSSI -58
```

### Error responses

```text
FLUVAL ERROR DISCONNECTED
FLUVAL ERROR BAD_ARGS
FLUVAL ERROR TIMEOUT
FLUVAL ERROR UNKNOWN_CMD
```

## Fluval BLE protocol (confirmed)

| Item | UUID / value |
|------|----------------|
| Device name | `Plant4.0_450467` |
| Service FFF0 | `0000fff0-0000-1000-8000-00805f9b34fb` |
| Notify FFF1 | `0000fff1-0000-1000-8000-00805f9b34fb` |
| Write FFF2 | `0000fff2-0000-1000-8000-00805f9b34fb` |

| Action | TX (FFF2) |
|--------|-----------|
| Read status | `d0ff` |
| Set Manual | `d1a10100` |
| Set Auto | `d1a10101` |
| Set channels | `d1a60318<pink>0418<blue>0518<cold_white>0618<white>0718<warm_white>0e00` |

Status responses on FFF1 start with `d2b0000e01<mode>02f5...` where mode `00` = Manual, `01` = Auto.

Channels after marker `02 f5`: `03` Pink, `04` Blue, `05` Cold White, `06` White, `07` Warm White.

## Behavior

- Persistent BLE connection with auto-reconnect
- Status poll every 10 seconds while connected
- State marked stale after 30 seconds without a parsed status
- No Wi-Fi, no BLE peripheral mode, no bonding UI

## Limitations

- Does **not** edit the full Auto schedule/timepoints stored in the light
- `FLUVAL MODE AUTO` switches to the schedule already saved by the Fluval app
- Close FluvalConnect and turn off phone Bluetooth while this helper controls the light

## AquaPilot integration

See the main project [README Fluval section](../README.md). Enable Fluval on the P4, wire UART, and use the same command/response format documented here.

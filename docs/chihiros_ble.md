# Chihiros aquarium heater — BLE integration (AquaPilot P4)

Production path: **one** ESP32-P4 AquaPilot firmware talks to the heater over **ESP-Hosted NimBLE** on the onboard ESP32-C6 (same stack as Fluval Plant 4.0). A standalone C6 app under `apps/chihiros_heater_c6/` is **reference/lab only** — not shipped on the coprocessor.

## Device discovery

| Item | Default |
|------|---------|
| Advertised name prefix | `DYH1` (configurable in NVS / **OPTIONS → Heater**) |
| Role | BLE central (NimBLE host on P4, HCI via ESP-Hosted VHCI) |

Scan/connect is **not** owned by the heater driver directly. All GAP/GATT is serialized through `main/ble/ble_central_manager.c`.

## Nordic UART Service (NUS)

| UUID | Role |
|------|------|
| `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Service |
| `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | RX (host → heater, write) |
| `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | TX (heater → host, notify + CCCD) |

Packet framing and checksum rules live in `components/chihiros_heater_protocol/`. Setpoint encoding and status decode are unit-tested there; do not invent new opcodes in application code.

## Connection sequence (after GATT connect)

1. Discover NUS service and characteristics.
2. Subscribe to TX notifications (CCCD).
3. Write init sequence on RX (fixed bytes from reverse engineering):

| Step | Payload (hex) |
|------|----------------|
| INIT_1 | `5a 01 06 00 02 04 01 00` |
| INIT_2 | `5a 01 0b 00 03 09 1a 05 02 13 25 0c 27` |
| INIT_3 | `5a 01 06 00 04 04 01 06` |

4. Status arrives on TX notify; decode with `chihiros_decode_status_packet()`.
5. Setpoint commands: `chihiros_make_setpoint_packet_f()` → 11-byte write on RX.

There is **no** dedicated “heater OFF” BLE packet in this repo. Policy disables heating by stopping writes and sending the **minimum** setpoint (default 50 °F) when connected and unsafe.

## Policy layer (`heater_manager`)

| Module | Responsibility |
|--------|----------------|
| `main/heater/chihiros_ble_client.c` | Driver: scan filter, connect, NUS, init, setpoint writes |
| `main/heater/heater_manager.c` | Enable/target, alarms, NVS settings, fail-safe gating |
| `main/heater/heater_safety.c` | Offline/stale/over-temp vs target |

Heater commands are blocked when: disabled, BLE offline, status stale, invalid temperature, or safety trip. **Heater over-temp is not suppressed during Maintenance Mode.**

## Central BLE manager / arbitration

`ble_central_manager` owns `nimble_port_init()` and multiplexes GAP events.

| Rule | Behavior |
|------|----------|
| Connections | At most **one** GATT connection (Fluval **or** Chihiros) |
| Priority | Chihiros (20) > Fluval (10) |
| Scanning | Time-sliced (~10 s windows); never parallel scans |
| Fluval while heater connected | Fluval polls defer until heater releases the link |

Expect Fluval status latency when both devices are enabled and the heater holds the connection.

## NVS settings (`fishduino_heater_settings_t`)

- `enabled`, `name_prefix` (default `DYH1`)
- `target_temp_f`, `min_setpoint_f`, `max_setpoint_f`
- `stale_timeout_s`, `max_over_target_f`

Stored in settings blob v6; see `main/storage/settings_nvs.h`.

## Serial commands

| Command | Description |
|---------|-------------|
| `heater_status` | Link, temp, setpoint, alarm |
| `heater_set <temp_F>` | Set target (if policy allows) |
| `heater_enable 0\|1` | Enable/disable integration |

## Future fallback (not implemented)

If hosted BLE on the onboard C6 is insufficient, an **external** ESP32-C6 running `apps/chihiros_heater_c6/` could be wired via UART to the P4. That path is documented only as a contingency; production uses the integrated driver above.

## Manual test checklist

1. **Heater only:** enable in UI, connect to `DYH1*`, verify status temp and setpoint.
2. **Fluval only:** unchanged behavior vs pre-scheduler firmware.
3. **Both enabled:** no concurrent connections; heater reconnect preempts Fluval; Fluval resumes between heater slots.
4. **Fail-safe:** disable heater or force stale → min setpoint / no heat commands.
5. **Maintenance:** CO2 blocked; heater over-temp still alarms.

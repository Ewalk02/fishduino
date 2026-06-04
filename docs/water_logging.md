# Water test logging and maintenance reminders

AquaPilot records **manually entered** aquarium water parameters and tracks recurring maintenance tasks. Readings are **not** sensed automatically — you enter test kit results from the touchscreen or serial console.

## Water parameters

| Parameter | Unit |
|-----------|------|
| pH | pH |
| Ammonia | ppm |
| Nitrite | ppm |
| Nitrate | ppm |
| Notes | optional text (up to 95 chars; use quotes for multi-word notes) |

Each entry is timestamped when system time is available (SNTP). If time is not synced, the entry is saved with an unknown-time flag.

## Storage

- **Water tests:** dedicated flash partition `waterlog` (64 KB), fixed-slot ring buffer
- **Capacity:** 200 entries (~1 year of biweekly tests)
- **Wear:** one record write per test — not stored in NVS (avoids flash wear from repeated NVS rewrites)
- **When full:** oldest entries are discarded first

## Alert thresholds (display only)

| Condition | Level |
|-----------|-------|
| Ammonia > 0 ppm | warning |
| Nitrite > 0 ppm | warning |
| Nitrate > 40 ppm | warning |
| pH outside 6.0–8.0 | notice/warning |

Alerts appear on the dashboard and Water Tests screen. **No automatic CO2/heater/filter actions** are taken from water readings.

## Maintenance reminders

Distinct from **Maintenance Mode** (temporary CO2-off during filter service).

Default tasks (NVS `maint_tasks_v1`):

| Task | Interval |
|------|----------|
| Check water parameters | 14 days |
| Rinse filter media | 30 days |
| Change drop checker fluid | 30 days |
| Trim plants | 14 days |
| Clean glass | 7 days |
| Inspect CO2 tubing/check valve | 30 days |
| Check filter flow | 30 days |

Saving a water test automatically marks **Check water parameters** done and sets the next due date 14 days later (when system time is available). If time is not synced, the task shows **done/time-unknown** and is excluded from `maint_due` until SNTP provides a clock; due dates are then backfilled from the sync time.

## UI

- **OPTIONS → Water Tests** — latest values, Add Test, History, Reminders link
- **OPTIONS → Reminders** — mark done, snooze (1/3/7 days)
- Dashboard shows compact latest water line and due/overdue reminder summary

## Serial commands

```text
water_add <ph> <nh3_ppm> <no2_ppm> <no3_ppm> [note...]
```

Multi-word notes are supported (`after water change`) or use quotes: `water_add 7.6 0 0 5 "after water change"`.

```text
water_latest
water_list
water_clear_confirm confirm

maint_list
maint_due
maint_done <id|alias>
maint_snooze <id|alias> <days>
```

Aliases: `water_test`, `filter_rinse`, `drop_checker`, `trim_plants`, `clean_glass`, `co2_inspection`, `filter_flow`

Example:

```text
water_add 7.8 1.0 0.0 0.0
maint_done filter_rinse
```

#include "ui.h"

#include "dashboard_data.h"
#include "screen_commissioning.h"
#include "screen_diagnostics.h"
#include "screen_heater_settings.h"
#include "screen_maintenance.h"
#include "screen_maint_tracker.h"
#include "screen_options.h"
#include "screen_water_entry.h"
#include "screen_water_history.h"
#include "screen_water_tests.h"

void fishduino_ui_init(fishduino_ui_t *ui)
{
    ui->root = lv_obj_create(NULL);
    lv_obj_remove_style_all(ui->root);
    lv_obj_set_size(ui->root, LV_PCT(100), LV_PCT(100));

    ui->cockpit = fishduino_cockpit_dashboard_build(ui->root);

    fishduino_screen_options_build(ui->root);
    fishduino_screen_heater_build(ui->root);
    fishduino_screen_maintenance_build(ui->root);
    fishduino_screen_water_tests_build(ui->root);
    fishduino_screen_water_entry_build(ui->root);
    fishduino_screen_water_history_build(ui->root);
    fishduino_screen_maint_tracker_build(ui->root);

    lv_screen_load(ui->root);
}

void fishduino_ui_update(fishduino_ui_t *ui, const fishduino_co2_t *co2,
                         const fishduino_feeder_t *feeder, const fishduino_settings_t *settings)
{
    fishduino_screen_commissioning_tick();
    fishduino_screen_diagnostics_refresh();

    dashboard_snapshot_t snap;
    dashboard_data_refresh(&snap, co2, feeder, settings);
    fishduino_cockpit_dashboard_update(&ui->cockpit, &snap);
}

#include "screen_dashboard.h"

#include "screen_options.h"
#include "shelly/shelly_manager.h"

static void btn_options_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_options_show();
}

static void btn_co2_on_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_co2_manual(true);
}

static void btn_co2_off_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_co2_manual(false);
}

static void btn_co2_auto_cb(lv_event_t *e)
{
    (void)e;
    fishduino_shelly_co2_auto();
}

fishduino_dashboard_handles_t fishduino_screen_dashboard_build(lv_obj_t *parent)
{
    fishduino_dashboard_handles_t h = {0};

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Fishduino");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *status = lv_label_create(parent);
    lv_label_set_text(status, "Aquarium dashboard");
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 36);

    h.label_filter_alarm = lv_label_create(parent);
    lv_label_set_text(h.label_filter_alarm, "");
    lv_obj_set_style_text_color(h.label_filter_alarm, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_text_font(h.label_filter_alarm, &lv_font_montserrat_14, 0);
    lv_obj_set_width(h.label_filter_alarm, 440);
    lv_obj_set_style_text_align(h.label_filter_alarm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(h.label_filter_alarm, LV_ALIGN_TOP_MID, 0, 58);

    h.label_co2 = lv_label_create(parent);
    lv_label_set_text(h.label_co2, "CO2: --");
    lv_obj_align(h.label_co2, LV_ALIGN_TOP_LEFT, 12, 100);

    h.label_co2_detail = lv_label_create(parent);
    lv_label_set_text(h.label_co2_detail, "Sched: --");
    lv_obj_align(h.label_co2_detail, LV_ALIGN_TOP_LEFT, 12, 122);

    h.label_filter = lv_label_create(parent);
    lv_label_set_text(h.label_filter, "Filter: --");
    lv_obj_align(h.label_filter, LV_ALIGN_TOP_LEFT, 12, 154);

    h.label_filter_energy = lv_label_create(parent);
    lv_label_set_text(h.label_filter_energy, "Filter kWh: --");
    lv_obj_align(h.label_filter_energy, LV_ALIGN_TOP_LEFT, 12, 176);

    h.label_feeder = lv_label_create(parent);
    lv_label_set_text(h.label_feeder, "Feeder: --");
    lv_obj_align(h.label_feeder, LV_ALIGN_TOP_LEFT, 12, 198);

    h.btn_co2_on = lv_btn_create(parent);
    lv_obj_set_size(h.btn_co2_on, 90, 36);
    lv_obj_align(h.btn_co2_on, LV_ALIGN_BOTTOM_LEFT, 12, -56);
    lv_obj_t *lon = lv_label_create(h.btn_co2_on);
    lv_label_set_text(lon, "CO2 ON");
    lv_obj_center(lon);
    lv_obj_add_event_cb(h.btn_co2_on, btn_co2_on_cb, LV_EVENT_CLICKED, NULL);

    h.btn_co2_off = lv_btn_create(parent);
    lv_obj_set_size(h.btn_co2_off, 90, 36);
    lv_obj_align(h.btn_co2_off, LV_ALIGN_BOTTOM_LEFT, 110, -56);
    lv_obj_t *loff = lv_label_create(h.btn_co2_off);
    lv_label_set_text(loff, "CO2 OFF");
    lv_obj_center(loff);
    lv_obj_add_event_cb(h.btn_co2_off, btn_co2_off_cb, LV_EVENT_CLICKED, NULL);

    h.btn_co2_auto = lv_btn_create(parent);
    lv_obj_set_size(h.btn_co2_auto, 90, 36);
    lv_obj_align(h.btn_co2_auto, LV_ALIGN_BOTTOM_LEFT, 208, -56);
    lv_obj_t *lauto = lv_label_create(h.btn_co2_auto);
    lv_label_set_text(lauto, "AUTO");
    lv_obj_center(lauto);
    lv_obj_add_event_cb(h.btn_co2_auto, btn_co2_auto_cb, LV_EVENT_CLICKED, NULL);

    h.btn_options = lv_btn_create(parent);
    lv_obj_set_size(h.btn_options, 90, 36);
    lv_obj_align(h.btn_options, LV_ALIGN_BOTTOM_LEFT, 306, -56);
    lv_obj_t *lopt = lv_label_create(h.btn_options);
    lv_label_set_text(lopt, "OPTIONS");
    lv_obj_center(lopt);
    lv_obj_add_event_cb(h.btn_options, btn_options_cb, LV_EVENT_CLICKED, NULL);

    return h;
}

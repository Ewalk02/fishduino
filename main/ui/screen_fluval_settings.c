#include "screen_fluval_settings.h"

#include <stdio.h>
#include <string.h>

#include "fluval/fluval_light.h"
#include "storage/settings_runtime.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_label_status;
static lv_obj_t *s_slider_pink;
static lv_obj_t *s_slider_blue;
static lv_obj_t *s_slider_cold_white;
static lv_obj_t *s_slider_white;
static lv_obj_t *s_slider_warm_white;

typedef struct {
    bool enabled;
} fluval_enable_ctx_t;

typedef struct {
    fishduino_fluval_recipe_t recipe;
} fluval_recipe_ctx_t;

static bool mutator_fluval_enabled(fishduino_settings_t *st, void *ctx)
{
    fluval_enable_ctx_t *c = (fluval_enable_ctx_t *)ctx;
    st->fluval.enabled = c->enabled;
    return true;
}

static bool mutator_fluval_recipe(fishduino_settings_t *st, void *ctx)
{
    fluval_recipe_ctx_t *c = (fluval_recipe_ctx_t *)ctx;
    st->fluval.manual_recipe = c->recipe;
    return true;
}

static uint8_t slider_percent(lv_obj_t *slider)
{
    return (uint8_t)lv_slider_get_value(slider);
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    fishduino_screen_fluval_hide();
}

static void btn_enable_on_cb(lv_event_t *e)
{
    (void)e;
    fluval_enable_ctx_t ctx = {.enabled = true};
    if (fishduino_settings_update(mutator_fluval_enabled, &ctx, true)) {
        lv_label_set_text(s_label_status, "Fluval enabled");
    }
}

static void btn_enable_off_cb(lv_event_t *e)
{
    (void)e;
    fluval_enable_ctx_t ctx = {.enabled = false};
    if (fishduino_settings_update(mutator_fluval_enabled, &ctx, true)) {
        lv_label_set_text(s_label_status, "Fluval disabled");
    }
}

static void btn_apply_cb(lv_event_t *e)
{
    (void)e;
    esp_err_t err = fishduino_fluval_set_channels(slider_percent(s_slider_pink), slider_percent(s_slider_blue),
                                                  slider_percent(s_slider_cold_white), slider_percent(s_slider_white),
                                                  slider_percent(s_slider_warm_white));
    if (err != ESP_OK) {
        lv_label_set_text(s_label_status, "Apply failed (disabled?)");
        return;
    }
    lv_label_set_text(s_label_status, "Channels queued");
}

static void btn_save_recipe_cb(lv_event_t *e)
{
    (void)e;
    fluval_recipe_ctx_t ctx = {
        .recipe =
            {
                .pink = slider_percent(s_slider_pink),
                .blue = slider_percent(s_slider_blue),
                .cold_white = slider_percent(s_slider_cold_white),
                .white = slider_percent(s_slider_white),
                .warm_white = slider_percent(s_slider_warm_white),
            },
    };
    if (fishduino_settings_update(mutator_fluval_recipe, &ctx, true)) {
        lv_label_set_text(s_label_status, "Recipe saved to NVS");
    } else {
        lv_label_set_text(s_label_status, "Save failed");
    }
}

static lv_obj_t *make_channel_slider(lv_obj_t *parent, int y, const char *name, uint8_t initial,
                                     lv_obj_t **slider_out)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, name);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, initial, LV_ANIM_OFF);
    lv_obj_set_width(slider, 280);
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 80, y);
    *slider_out = slider;
    return slider;
}

static void load_recipe_sliders(void)
{
    fishduino_settings_t st;
    if (!fishduino_settings_get_snapshot(&st)) {
        return;
    }
    lv_slider_set_value(s_slider_pink, st.fluval.manual_recipe.pink, LV_ANIM_OFF);
    lv_slider_set_value(s_slider_blue, st.fluval.manual_recipe.blue, LV_ANIM_OFF);
    lv_slider_set_value(s_slider_cold_white, st.fluval.manual_recipe.cold_white, LV_ANIM_OFF);
    lv_slider_set_value(s_slider_white, st.fluval.manual_recipe.white, LV_ANIM_OFF);
    lv_slider_set_value(s_slider_warm_white, st.fluval.manual_recipe.warm_white, LV_ANIM_OFF);
}

void fishduino_screen_fluval_build(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_set_size(s_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Fluval Plant 4.0");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    s_label_status = lv_label_create(s_screen);
    lv_label_set_text(s_label_status, "Manual channel recipe");
    lv_obj_set_width(s_label_status, 440);
    lv_obj_set_style_text_align(s_label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_label_status, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *btn_on = lv_btn_create(s_screen);
    lv_obj_set_size(btn_on, 88, 28);
    lv_obj_align(btn_on, LV_ALIGN_TOP_LEFT, 8, 52);
    lv_label_set_text(lv_label_create(btn_on), "Enable");
    lv_obj_add_event_cb(btn_on, btn_enable_on_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_off = lv_btn_create(s_screen);
    lv_obj_set_size(btn_off, 88, 28);
    lv_obj_align(btn_off, LV_ALIGN_TOP_LEFT, 100, 52);
    lv_label_set_text(lv_label_create(btn_off), "Disable");
    lv_obj_add_event_cb(btn_off, btn_enable_off_cb, LV_EVENT_CLICKED, NULL);

    make_channel_slider(s_screen, 92, "Pink", 40, &s_slider_pink);
    make_channel_slider(s_screen, 118, "Blue", 20, &s_slider_blue);
    make_channel_slider(s_screen, 144, "Cold W", 60, &s_slider_cold_white);
    make_channel_slider(s_screen, 170, "White", 70, &s_slider_white);
    make_channel_slider(s_screen, 196, "Warm W", 50, &s_slider_warm_white);
    load_recipe_sliders();

    lv_obj_t *btn_apply = lv_btn_create(s_screen);
    lv_obj_set_size(btn_apply, 120, 32);
    lv_obj_align(btn_apply, LV_ALIGN_TOP_LEFT, 8, 230);
    lv_label_set_text(lv_label_create(btn_apply), "Apply Now");
    lv_obj_add_event_cb(btn_apply, btn_apply_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_save = lv_btn_create(s_screen);
    lv_obj_set_size(btn_save, 120, 32);
    lv_obj_align(btn_save, LV_ALIGN_TOP_LEFT, 140, 230);
    lv_label_set_text(lv_label_create(btn_save), "Save Recipe");
    lv_obj_add_event_cb(btn_save, btn_save_recipe_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(s_screen);
    lv_obj_set_size(btn_back, 100, 36);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(lv_label_create(btn_back), "BACK");
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
}

lv_obj_t *fishduino_screen_fluval_root(void)
{
    return s_screen;
}

void fishduino_screen_fluval_show(void)
{
    if (s_screen != NULL) {
        load_recipe_sliders();
        lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_screen);
    }
}

void fishduino_screen_fluval_hide(void)
{
    if (s_screen != NULL) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

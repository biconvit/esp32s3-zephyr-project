/**
 * @file screen_settings.c
 * @brief Settings Screen - Threshold adjustment visualization
 *
 * Layout (160x80 pixels):
 * ┌────────────────────────────┐
 * │  Temp Threshold:    30.0C  │  Row 1: Title (left) + Pending value (right)
 * │  [═════════●═════════════] │  Row 2: Full-width threshold bar (150px)
 * │  20C                  40C  │  Row 3: Min and Max range (at ends of bar)
 * │  Saved (Active)            │  Row 4: Status / Confirm action hint
 * └────────────────────────────┘
 */

#include "ui.h"
#include <stdio.h>

/* ============================================================================
 * Screen Widget Handles
 * ========================================================================= */

static lv_obj_t *lbl_label;
static lv_obj_t *lbl_value;
static lv_obj_t *bar_threshold;
static lv_obj_t *lbl_range_min;
static lv_obj_t *lbl_range_max;
static lv_obj_t *lbl_hint;

/* ============================================================================
 * Styles
 * ========================================================================= */

static lv_style_t style_bar_bg;
static lv_style_t style_bar_fill;
static bool styles_initialized = false;

static void init_styles(void)
{
    if (styles_initialized) return;

    /* Bar background */
    lv_style_init(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, UI_COLOR_BAR_BG);
    lv_style_set_bg_opa(&style_bar_bg, LV_OPA_100);
    lv_style_set_radius(&style_bar_bg, 4);

    /* Bar fill: gradient from green to red */
    lv_style_init(&style_bar_fill);
    lv_style_set_bg_color(&style_bar_fill, UI_COLOR_OK);
    lv_style_set_bg_grad_color(&style_bar_fill, UI_COLOR_DANGER);
    lv_style_set_bg_grad_dir(&style_bar_fill, LV_GRAD_DIR_HOR);
    lv_style_set_bg_opa(&style_bar_fill, LV_OPA_100);
    lv_style_set_radius(&style_bar_fill, 4);

    styles_initialized = true;
}

/* ============================================================================
 * Screen Create
 * ========================================================================= */

void screen_settings_create(lv_obj_t *parent)
{
    init_styles();

    /* Set background color */
    lv_obj_set_style_bg_color(parent, UI_COLOR_BG, 0);

    /* --- Row 1: Section Title "Temp Threshold:" (left) & Pending Value (right) (y=2) --- */
    lbl_label = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_label, UI_COLOR_ACCENT, 0);
    lv_label_set_text(lbl_label, "Temp Threshold:");
    lv_obj_align(lbl_label, LV_ALIGN_TOP_LEFT, 4, 4);

    lbl_value = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_value, UI_COLOR_TEMP, 0);
    lv_label_set_text(lbl_value, "30.0C");
    lv_obj_align(lbl_value, LV_ALIGN_TOP_RIGHT, -6, 2);

    /* --- Row 2: Threshold bar (y=24, width=150) --- */
    bar_threshold = lv_bar_create(parent);
    lv_obj_set_size(bar_threshold, 150, 12);
    lv_obj_align(bar_threshold, LV_ALIGN_TOP_LEFT, 4, 24);
    lv_bar_set_range(bar_threshold,
                     (int)(TEMP_THRESHOLD_MIN * 10),
                     (int)(TEMP_THRESHOLD_MAX * 10));
    lv_bar_set_value(bar_threshold,
                     (int)(TEMP_THRESHOLD_DEFAULT * 10),
                     LV_ANIM_OFF);
    lv_obj_add_style(bar_threshold, &style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(bar_threshold, &style_bar_fill, LV_PART_INDICATOR);

    /* --- Row 3: Range labels at start and end of bar (y=42) --- */
    lbl_range_min = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_range_min, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_range_min, lv_color_hex(0x888888), 0);
    lv_label_set_text(lbl_range_min, "20C");
    lv_obj_align(lbl_range_min, LV_ALIGN_TOP_LEFT, 4, 42);

    lbl_range_max = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_range_max, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_range_max, lv_color_hex(0x888888), 0);
    lv_label_set_text(lbl_range_max, "40C");
    lv_obj_align(lbl_range_max, LV_ALIGN_TOP_RIGHT, -6, 42);

    /* --- Row 4: Status feedback / hint (y=62) --- */
    lbl_hint = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_hint, UI_COLOR_OK, 0);
    lv_label_set_text(lbl_hint, "Saved (Active)");
    lv_obj_align(lbl_hint, LV_ALIGN_TOP_LEFT, 4, 62);
}

/* ============================================================================
 * Screen Update
 * ========================================================================= */

void screen_settings_update(const display_data_t *data)
{
    char buf[16];

    /* Update bar position based on pending threshold (interactive tuning) */
    lv_bar_set_value(bar_threshold,
                     (int)(data->pending_threshold * 10),
                     LV_ANIM_ON);

    /* Update value label */
    snprintf(buf, sizeof(buf), "%.1fC", (double)data->pending_threshold);
    lv_label_set_text(lbl_value, buf);

    /* Visual status feedback */
    if (data->threshold_pending) {
        lv_label_set_text(lbl_hint, "[BTN2] Press to Save");
        lv_obj_set_style_text_color(lbl_hint, UI_COLOR_WARN, 0);
        lv_obj_set_style_text_color(lbl_value, UI_COLOR_WARN, 0);
    } else {
        lv_label_set_text(lbl_hint, "Saved (Active)");
        lv_obj_set_style_text_color(lbl_hint, UI_COLOR_OK, 0);
        lv_obj_set_style_text_color(lbl_value, UI_COLOR_TEMP, 0);
    }
}

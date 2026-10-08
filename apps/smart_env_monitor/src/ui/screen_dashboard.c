/**
 * @file screen_dashboard.c
 * @brief Dashboard Screen - Real-time sensor data + system state display
 *
 * Layout (160x80 pixels):
 * ┌────────────────────────────┐
 * │  28.5C              65.2%  │  Row 1: Temperature (left) + Humidity (right)
 * │  ▓▓▓▓▓░░░░        Fan: 0%  │  Row 2: Fan bar (75px) + Fan % (right aligned)
 * │  STATE: IDLE               │  Row 3: Current state
 * │  Thr: 30.0C                │  Row 4: Threshold value
 * └────────────────────────────┘
 */

#include "ui.h"
#include <stdio.h>

/* ============================================================================
 * Screen Widget Handles
 * ========================================================================= */

static lv_obj_t *lbl_temp;          /* Temperature value */
static lv_obj_t *lbl_humid;         /* Humidity value */
static lv_obj_t *bar_fan;           /* Fan speed bar */
static lv_obj_t *lbl_fan_pct;       /* Fan percentage text */
static lv_obj_t *lbl_state;         /* System state text */
static lv_obj_t *lbl_threshold;     /* Threshold value */

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
    lv_style_set_radius(&style_bar_bg, 3);

    /* Bar fill */
    lv_style_init(&style_bar_fill);
    lv_style_set_bg_color(&style_bar_fill, UI_COLOR_ACCENT);
    lv_style_set_bg_opa(&style_bar_fill, LV_OPA_100);
    lv_style_set_radius(&style_bar_fill, 3);

    styles_initialized = true;
}

/* ============================================================================
 * State to display text helpers
 * ========================================================================= */

static const char *state_to_str(system_state_t state)
{
    switch (state) {
    case STATE_IDLE:     return "IDLE";
    case STATE_WARNING:  return "WARNING";
    case STATE_COOLING:  return "COOLING";
    case STATE_CRITICAL: return "CRITICAL";
    default:             return "UNKNOWN";
    }
}

static lv_color_t state_to_color(system_state_t state)
{
    switch (state) {
    case STATE_IDLE:     return UI_COLOR_OK;
    case STATE_WARNING:  return UI_COLOR_WARN;
    case STATE_COOLING:  return UI_COLOR_TEMP;
    case STATE_CRITICAL: return UI_COLOR_DANGER;
    default:             return UI_COLOR_TEXT;
    }
}

/* ============================================================================
 * Screen Create
 * ========================================================================= */

void screen_dashboard_create(lv_obj_t *parent)
{
    init_styles();

    /* Set background color */
    lv_obj_set_style_bg_color(parent, UI_COLOR_BG, 0);

    /* --- Row 1: Temperature (left) + Humidity (right) (y=2) --- */
    lbl_temp = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_temp, UI_COLOR_TEMP, 0);
    lv_label_set_text(lbl_temp, "--.-C");
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_LEFT, 4, 2);

    lbl_humid = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_humid, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_humid, UI_COLOR_HUMID, 0);
    lv_label_set_text(lbl_humid, "--.-%");
    lv_obj_align(lbl_humid, LV_ALIGN_TOP_RIGHT, -6, 2);

    /* --- Row 2: Fan bar (left, 75px) + Fan % (right, aligned like humidity) (y=24) --- */
    bar_fan = lv_bar_create(parent);
    lv_obj_set_size(bar_fan, 75, 10);
    lv_obj_align(bar_fan, LV_ALIGN_TOP_LEFT, 4, 25);
    lv_bar_set_range(bar_fan, 0, 100);
    lv_bar_set_value(bar_fan, 0, LV_ANIM_OFF);
    lv_obj_add_style(bar_fan, &style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(bar_fan, &style_bar_fill, LV_PART_INDICATOR);

    lbl_fan_pct = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_fan_pct, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_fan_pct, UI_COLOR_ACCENT, 0);
    lv_label_set_text(lbl_fan_pct, "Fan: 0%");
    lv_obj_align(lbl_fan_pct, LV_ALIGN_TOP_RIGHT, -6, 24);

    /* --- Row 3: System state (y=42) --- */
    lbl_state = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_state, UI_COLOR_OK, 0);
    lv_label_set_text(lbl_state, "IDLE");
    lv_obj_align(lbl_state, LV_ALIGN_TOP_LEFT, 4, 42);

    /* --- Row 4: Threshold (y=62) --- */
    lbl_threshold = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_threshold, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_threshold, UI_COLOR_ACCENT, 0);
    lv_label_set_text(lbl_threshold, "Thr: --.-C");
    lv_obj_align(lbl_threshold, LV_ALIGN_TOP_LEFT, 4, 62);
}

/* ============================================================================
 * Screen Update
 * ========================================================================= */

void screen_dashboard_update(const display_data_t *data)
{
    char buf[24];

    /* Temperature */
    snprintf(buf, sizeof(buf), "%.1fC", (double)data->temperature);
    lv_label_set_text(lbl_temp, buf);

    /* Humidity */
    snprintf(buf, sizeof(buf), "%.1f%%", (double)data->humidity);
    lv_label_set_text(lbl_humid, buf);

    /* Fan bar */
    lv_bar_set_value(bar_fan, (int)data->fan_duty_pct, LV_ANIM_ON);

    /* Fan percentage text */
    snprintf(buf, sizeof(buf), "Fan: %d%%", (int)data->fan_duty_pct);
    lv_label_set_text(lbl_fan_pct, buf);

    /* State with color */
    lv_label_set_text(lbl_state, state_to_str(data->state));
    lv_obj_set_style_text_color(lbl_state, state_to_color(data->state), 0);

    /* Threshold (active threshold with * indicator if tuning is pending) */
    if (data->threshold_pending) {
        snprintf(buf, sizeof(buf), "Thr:%.1fC*", (double)data->temp_threshold);
    } else {
        snprintf(buf, sizeof(buf), "Thr:%.1fC", (double)data->temp_threshold);
    }
    lv_label_set_text(lbl_threshold, buf);
}

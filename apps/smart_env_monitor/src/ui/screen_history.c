/**
 * @file screen_history.c
 * @brief History Screen - Temperature trend line chart (last 60 samples)
 *
 * Renders a mini temperature chart using LVGL line widget.
 * The chart displays the last TEMP_HISTORY_SIZE temperature readings
 * as a connected line graph, providing visual trend information.
 *
 * Layout (160x80 pixels):
 * ┌────────────────────────────┐
 * │  TEMP HISTORY              │  Row 1: Title + current temp
 * │  ╱╲   ╱╲                  │
 * │      ╲╱   ╲               │  Rows 2-4: Line chart area
 * │             ╲──            │
 * └────────────────────────────┘
 *
 * Note: Since the 160x80 LCD is very small, we use a simplified
 * line-based chart instead of lv_chart (which requires more memory).
 */

#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Configuration
 * ========================================================================= */

/** Chart area dimensions within the 160x80 screen */
#define CHART_X_START       4
#define CHART_X_END         156
#define CHART_Y_START       18
#define CHART_Y_END         72
#define CHART_WIDTH         (CHART_X_END - CHART_X_START)
#define CHART_HEIGHT        (CHART_Y_END - CHART_Y_START)

/** Temperature display range for chart Y-axis */
#define CHART_TEMP_MIN      20.0f
#define CHART_TEMP_MAX      40.0f

/** Max points to display (limited by screen width) */
#define CHART_MAX_POINTS    60

/* ============================================================================
 * Screen Widget Handles
 * ========================================================================= */

static lv_obj_t *lbl_title;
static lv_obj_t *lbl_current;
static lv_obj_t *lbl_y_max;
static lv_obj_t *lbl_y_min;
static lv_obj_t *line_chart;
static lv_obj_t *line_threshold;

/* Line points buffer */
static lv_point_t chart_points[CHART_MAX_POINTS];
static lv_point_t thresh_points[2];

/* ============================================================================
 * Styles
 * ========================================================================= */

static lv_style_t style_line_chart;
static lv_style_t style_line_thresh;
static bool styles_initialized = false;

static void init_styles(void)
{
    if (styles_initialized) return;

    /* Chart line style: orange, 2px */
    lv_style_init(&style_line_chart);
    lv_style_set_line_color(&style_line_chart, UI_COLOR_TEMP);
    lv_style_set_line_width(&style_line_chart, 2);
    lv_style_set_line_rounded(&style_line_chart, true);

    /* Threshold line style: dashed cyan, 1px */
    lv_style_init(&style_line_thresh);
    lv_style_set_line_color(&style_line_thresh, UI_COLOR_ACCENT);
    lv_style_set_line_width(&style_line_thresh, 1);
    lv_style_set_line_dash_width(&style_line_thresh, 4);
    lv_style_set_line_dash_gap(&style_line_thresh, 3);

    styles_initialized = true;
}

/* ============================================================================
 * Helper: Map temperature to Y pixel coordinate
 * ========================================================================= */

static int16_t temp_to_y(float temp)
{
    if (temp < CHART_TEMP_MIN) temp = CHART_TEMP_MIN;
    if (temp > CHART_TEMP_MAX) temp = CHART_TEMP_MAX;

    /* Invert: higher temp = lower Y (top of screen) */
    float ratio = (temp - CHART_TEMP_MIN) / (CHART_TEMP_MAX - CHART_TEMP_MIN);
    return (int16_t)(CHART_Y_END - (ratio * CHART_HEIGHT));
}

/* ============================================================================
 * Screen Create
 * ========================================================================= */

void screen_history_create(lv_obj_t *parent)
{
    init_styles();

    /* Set background color */
    lv_obj_set_style_bg_color(parent, UI_COLOR_BG, 0);

    /* --- Title --- */
    lbl_title = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_ACCENT, 0);
    lv_label_set_text(lbl_title, "TEMP HISTORY");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 4, 2);

    /* Current temperature (top right) */
    lbl_current = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_current, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_current, UI_COLOR_TEMP, 0);
    lv_label_set_text(lbl_current, "--.-C");
    lv_obj_align(lbl_current, LV_ALIGN_TOP_RIGHT, -4, 2);

    /* Y-axis labels */
    lbl_y_max = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_y_max, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_y_max, lv_color_hex(0x555555), 0);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", (double)CHART_TEMP_MAX);
        lv_label_set_text(lbl_y_max, buf);
    }
    lv_obj_align(lbl_y_max, LV_ALIGN_TOP_LEFT, 4, CHART_Y_START - 2);

    lbl_y_min = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_y_min, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_y_min, lv_color_hex(0x555555), 0);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", (double)CHART_TEMP_MIN);
        lv_label_set_text(lbl_y_min, buf);
    }
    lv_obj_align(lbl_y_min, LV_ALIGN_TOP_LEFT, 4, CHART_Y_END - 8);

    /* --- Threshold horizontal line --- */
    thresh_points[0].x = CHART_X_START;
    thresh_points[0].y = temp_to_y(TEMP_THRESHOLD_DEFAULT);
    thresh_points[1].x = CHART_X_END;
    thresh_points[1].y = temp_to_y(TEMP_THRESHOLD_DEFAULT);

    line_threshold = lv_line_create(parent);
    lv_obj_add_style(line_threshold, &style_line_thresh, 0);
    lv_line_set_points(line_threshold, thresh_points, 2);

    /* --- Temperature chart line --- */
    /* Initialize with a single point at center */
    chart_points[0].x = CHART_X_START;
    chart_points[0].y = temp_to_y(25.0f);

    line_chart = lv_line_create(parent);
    lv_obj_add_style(line_chart, &style_line_chart, 0);
    lv_line_set_points(line_chart, chart_points, 1);
}

/* ============================================================================
 * Screen Update
 * ========================================================================= */

void screen_history_update(const display_data_t *data)
{
    char buf[16];
    int num_points;
    int i, src_idx;

    /* Update current temp label */
    snprintf(buf, sizeof(buf), "%.1fC", (double)data->temperature);
    lv_label_set_text(lbl_current, buf);

    /* Update threshold line Y position */
    int16_t thresh_y = temp_to_y(data->temp_threshold);
    thresh_points[0].y = thresh_y;
    thresh_points[1].y = thresh_y;
    lv_line_set_points(line_threshold, thresh_points, 2);

    /* Build chart points from circular history buffer */
    num_points = data->history_count;
    if (num_points <= 0) return;
    if (num_points > CHART_MAX_POINTS) num_points = CHART_MAX_POINTS;

    /* Calculate X spacing */
    float x_step = (num_points > 1) ?
                   (float)CHART_WIDTH / (float)(num_points - 1) : 0.0f;

    /*
     * Read from the circular buffer in chronological order.
     * The oldest entry is at (history_index - history_count) % SIZE,
     * and the newest is at (history_index - 1) % SIZE.
     */
    int start = (data->history_index - num_points + TEMP_HISTORY_SIZE)
                % TEMP_HISTORY_SIZE;

    for (i = 0; i < num_points; i++) {
        src_idx = (start + i) % TEMP_HISTORY_SIZE;
        chart_points[i].x = CHART_X_START + (int16_t)(i * x_step);
        chart_points[i].y = temp_to_y(data->temp_history[src_idx]);
    }

    lv_line_set_points(line_chart, chart_points, num_points);
}

/**
 * @file ui.h
 * @brief UI helper functions and screen declarations for LVGL rendering
 *
 * Each screen is implemented as a separate module for clean separation:
 *   - screen_dashboard: Real-time sensor data + system state
 *   - screen_settings:  Threshold adjustment visualization
 *   - screen_history:   Temperature trend line chart
 */

#ifndef UI_H
#define UI_H

#include <lvgl.h>
#include "../common.h"

/* ============================================================================
 * Color Palette (consistent across all screens)
 * ========================================================================= */

#define UI_COLOR_BG         lv_color_hex(0x1A1A2E)  /* Dark navy background */
#define UI_COLOR_TEXT       lv_color_hex(0xEEEEEE)  /* Light text */
#define UI_COLOR_ACCENT     lv_color_hex(0x00D2FF)  /* Cyan accent */
#define UI_COLOR_TEMP       lv_color_hex(0xFF6B35)  /* Orange for temperature */
#define UI_COLOR_HUMID      lv_color_hex(0x00B4D8)  /* Blue for humidity */
#define UI_COLOR_OK         lv_color_hex(0x2DC653)  /* Green for idle/ok */
#define UI_COLOR_WARN       lv_color_hex(0xFFBE0B)  /* Yellow for warning */
#define UI_COLOR_DANGER     lv_color_hex(0xE63946)  /* Red for critical */
#define UI_COLOR_BAR_BG     lv_color_hex(0x2A2A4A)  /* Bar background */

/* ============================================================================
 * Screen Interface
 * ========================================================================= */

/**
 * @brief Create the Dashboard screen LVGL objects.
 * @param parent  LVGL screen object to add widgets to
 */
void screen_dashboard_create(lv_obj_t *parent);

/**
 * @brief Update Dashboard screen with new data.
 * @param data  Pointer to latest display_data_t
 */
void screen_dashboard_update(const display_data_t *data);

/**
 * @brief Create the Settings screen LVGL objects.
 * @param parent  LVGL screen object to add widgets to
 */
void screen_settings_create(lv_obj_t *parent);

/**
 * @brief Update Settings screen with new data.
 * @param data  Pointer to latest display_data_t
 */
void screen_settings_update(const display_data_t *data);

/**
 * @brief Create the History screen LVGL objects.
 * @param parent  LVGL screen object to add widgets to
 */
void screen_history_create(lv_obj_t *parent);

/**
 * @brief Update History screen with new data.
 * @param data  Pointer to latest display_data_t
 */
void screen_history_update(const display_data_t *data);

#endif /* UI_H */

/**
 * @file display_thread.c
 * @brief Display Thread - LVGL Multi-Screen Rendering on ST7735S LCD
 *
 * Manages 3 LVGL screens (Dashboard, Settings, History) and switches
 * between them based on the screen_mode set by the Controller thread.
 * Receives display_data_t from display_msgq and updates the active screen.
 *
 * Zephyr concepts demonstrated:
 *   - Display driver API (display_blanking_off)
 *   - LVGL integration (lv_scr_act, lv_scr_load, lv_task_handler)
 *   - Message queue consumer (k_msgq_get)
 *   - Multi-screen UI management
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <lvgl.h>

#include "common.h"
#include "ui/ui.h"

/* ============================================================================
 * Configuration
 * ========================================================================= */

/** LVGL tick interval — target ~20 FPS */
#define DISPLAY_TICK_MS     50

/* ============================================================================
 * Screen Objects
 * ========================================================================= */

static lv_obj_t *scr_dashboard;
static lv_obj_t *scr_settings;
static lv_obj_t *scr_history;

/** Currently loaded screen */
static screen_mode_t active_screen = SCREEN_DASHBOARD;

/* ============================================================================
 * Helper: Get screen object from mode enum
 * ========================================================================= */

static lv_obj_t *get_screen_obj(screen_mode_t mode)
{
    switch (mode) {
    case SCREEN_DASHBOARD: return scr_dashboard;
    case SCREEN_SETTINGS:  return scr_settings;
    case SCREEN_HISTORY:   return scr_history;
    default:               return scr_dashboard;
    }
}

/* ============================================================================
 * Display Thread Entry Point
 * ========================================================================= */

void display_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    display_data_t disp_data;

    /* --- Initialize Display Device --- */
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        printk("[disp] ERROR: Display device not ready\n");
        return;
    }
    printk("[disp] Display initialized\n");

    /* --- Create LVGL Screens --- */

    /*
     * Create 3 independent screen objects. Only one is loaded at a time.
     * lv_scr_act() returns the currently active screen.
     * lv_scr_load() switches the visible screen.
     */

    /* Dashboard Screen (default active) */
    scr_dashboard = lv_obj_create(NULL);
    screen_dashboard_create(scr_dashboard);

    /* Settings Screen */
    scr_settings = lv_obj_create(NULL);
    screen_settings_create(scr_settings);

    /* History Screen */
    scr_history = lv_obj_create(NULL);
    screen_history_create(scr_history);

    /* Load dashboard as the initial screen */
    lv_scr_load(scr_dashboard);
    active_screen = SCREEN_DASHBOARD;

    /* Disable display blanking (turn on the screen) */
    display_blanking_off(display);

    printk("[disp] All screens created. Display thread running.\n");

    /* --- Main Render Loop --- */
    while (1) {

        /* Check for new display data (non-blocking) */
        if (k_msgq_get(&display_msgq, &disp_data, K_NO_WAIT) == 0) {

            /* Switch screen if mode changed */
            if (disp_data.screen != active_screen) {
                active_screen = disp_data.screen;
                lv_scr_load(get_screen_obj(active_screen));
                printk("[disp] Switched to screen %d\n", active_screen);
            }

            /* Update the active screen's content */
            switch (active_screen) {
            case SCREEN_DASHBOARD:
                screen_dashboard_update(&disp_data);
                break;
            case SCREEN_SETTINGS:
                screen_settings_update(&disp_data);
                break;
            case SCREEN_HISTORY:
                screen_history_update(&disp_data);
                break;
            default:
                break;
            }
        }

        /* LVGL tick handler — must be called periodically */
        lv_task_handler();

        /* Sleep to maintain target frame rate */
        k_msleep(DISPLAY_TICK_MS);
    }
}

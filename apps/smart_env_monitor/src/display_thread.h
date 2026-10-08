/**
 * @file display_thread.h
 * @brief Display thread public interface
 */

#ifndef DISPLAY_THREAD_H
#define DISPLAY_THREAD_H

#include "common.h"

/**
 * @brief Display thread entry point.
 *        Receives display_data_t, renders on ST7735S LCD via LVGL.
 */
void display_thread_entry(void *arg1, void *arg2, void *arg3);

#endif /* DISPLAY_THREAD_H */

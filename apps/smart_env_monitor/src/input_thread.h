/**
 * @file input_thread.h
 * @brief Input thread public interface
 */

#ifndef INPUT_THREAD_H
#define INPUT_THREAD_H

#include "common.h"

/**
 * @brief Input thread entry point.
 *        Handles button ISRs via workqueue (debounce) and ADC potentiometer reads.
 */
void input_thread_entry(void *arg1, void *arg2, void *arg3);

#endif /* INPUT_THREAD_H */

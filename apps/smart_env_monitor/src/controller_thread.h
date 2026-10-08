/**
 * @file controller_thread.h
 * @brief Controller thread public interface
 */

#ifndef CONTROLLER_THREAD_H
#define CONTROLLER_THREAD_H

#include "common.h"

/**
 * @brief Controller thread entry point.
 *        Runs state machine: receives sensor + input data, controls fan/LED,
 *        pushes display_data_t to display_msgq.
 */
void controller_thread_entry(void *arg1, void *arg2, void *arg3);

#endif /* CONTROLLER_THREAD_H */

/**
 * @file sensor_thread.h
 * @brief Sensor thread public interface
 */

#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include "common.h"

/**
 * @brief Sensor thread entry point.
 *        Reads BME280 every 1 second, pushes sensor_data_t to sensor_msgq.
 */
void sensor_thread_entry(void *arg1, void *arg2, void *arg3);

#endif /* SENSOR_THREAD_H */

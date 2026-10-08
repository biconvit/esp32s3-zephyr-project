/**
 * @file sensor_thread.c
 * @brief Sensor Thread - BME280 Temperature/Humidity/Pressure Reader
 *
 * Periodically reads the BME280 sensor via I2C and pushes structured
 * sensor_data_t packets to sensor_msgq for the Controller thread.
 *
 * Zephyr concepts demonstrated:
 *   - Sensor driver API (sensor_sample_fetch, sensor_channel_get)
 *   - Devicetree device binding (DEVICE_DT_GET_ANY)
 *   - Message queue producer (k_msgq_put)
 *   - Periodic task via k_msleep
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

#include "common.h"

/* ============================================================================
 * Configuration
 * ========================================================================= */

/** Sensor sampling interval in milliseconds */
#define SENSOR_SAMPLE_INTERVAL_MS   1000

/* ============================================================================
 * Sensor Thread Entry Point
 * ========================================================================= */

void sensor_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int ret;
    struct sensor_value temp, humidity, pressure;
    sensor_data_t data;

    /* Get BME280 device handle from Devicetree */
    const struct device *const bme280 = DEVICE_DT_GET_ANY(bosch_bme280);

    if (bme280 == NULL) {
        printk("[sensor] ERROR: BME280 not found in devicetree\n");
        return;
    }

    if (!device_is_ready(bme280)) {
        printk("[sensor] ERROR: BME280 device %s is not ready\n", bme280->name);
        return;
    }

    printk("[sensor] BME280 initialized: %s\n", bme280->name);
    printk("[sensor] Sampling every %d ms\n", SENSOR_SAMPLE_INTERVAL_MS);

    while (1) {
        /* Sleep at the start to prevent fast loop on failure */
        k_msleep(SENSOR_SAMPLE_INTERVAL_MS);

        /* Fetch all channels from BME280 */
        ret = sensor_sample_fetch(bme280);
        if (ret < 0) {
            printk("[sensor] Fetch error: %d\n", ret);
            continue;
        }

        /* Read temperature channel */
        ret = sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        if (ret < 0) {
            printk("[sensor] Temp channel error: %d\n", ret);
            continue;
        }

        /* Read humidity channel */
        ret = sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, &humidity);
        if (ret < 0) {
            printk("[sensor] Humidity channel error: %d\n", ret);
            continue;
        }

        /* Read pressure channel */
        ret = sensor_channel_get(bme280, SENSOR_CHAN_PRESS, &pressure);
        if (ret < 0) {
            printk("[sensor] Pressure channel error: %d\n", ret);
            continue;
        }

        /* Pack data into message struct */
        data.temperature  = sensor_value_to_double(&temp);
        data.humidity     = sensor_value_to_double(&humidity);
        data.pressure     = sensor_value_to_double(&pressure);
        data.timestamp_ms = k_uptime_get();

        /* Push to sensor message queue (non-blocking) */
        ret = k_msgq_put(&sensor_msgq, &data, K_NO_WAIT);
        if (ret < 0) {
            printk("[sensor] Queue full, dropping reading\n");
        }
    }
}

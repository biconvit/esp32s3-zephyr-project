/**
 * @file main.c
 * @brief Smart Environment Monitor & Controller - Application Entry Point
 *
 * Initializes all message queues and spawns the 4 application threads:
 *   1. Sensor Thread   (Priority 5) - BME280 readings
 *   2. Input Thread    (Priority 4) - Buttons + Potentiometer
 *   3. Controller Thread (Priority 6) - State machine + actuators
 *   4. Display Thread  (Priority 7) - LVGL rendering on LCD
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "common.h"

/* ============================================================================
 * Thread Stack & Configuration
 * ========================================================================= */

#define SENSOR_STACK_SIZE       1024
#define INPUT_STACK_SIZE        1024
#define CONTROLLER_STACK_SIZE   2048
#define DISPLAY_STACK_SIZE      4096

#define SENSOR_PRIORITY         5
#define INPUT_PRIORITY          4
#define CONTROLLER_PRIORITY     6
#define DISPLAY_PRIORITY        7

K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
K_THREAD_STACK_DEFINE(input_stack, INPUT_STACK_SIZE);
K_THREAD_STACK_DEFINE(controller_stack, CONTROLLER_STACK_SIZE);
K_THREAD_STACK_DEFINE(display_stack, DISPLAY_STACK_SIZE);

static struct k_thread sensor_thread_data;
static struct k_thread input_thread_data;
static struct k_thread controller_thread_data;
static struct k_thread display_thread_data;

/* ============================================================================
 * Message Queue Definitions
 * ========================================================================= */

/** Sensor → Controller: up to 10 sensor readings buffered */
K_MSGQ_DEFINE(sensor_msgq, sizeof(sensor_data_t), 10, 4);

/** Input → Controller: up to 10 input events buffered */
K_MSGQ_DEFINE(input_msgq, sizeof(input_event_t), 10, 4);

/** Controller → Display: 2-deep buffer (latest state) */
K_MSGQ_DEFINE(display_msgq, sizeof(display_data_t), 2, 4);

/* ============================================================================
 * Main Entry Point
 * ========================================================================= */

int main(void)
{
    printk("==============================================\n");
    printk(" Smart Environment Monitor & Controller\n");
    printk(" Platform: ESP32-S3-DevKitC + Zephyr RTOS\n");
    printk("==============================================\n\n");

    /* --- Spawn Sensor Thread --- */
    k_thread_create(&sensor_thread_data,
                    sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_thread_entry,
                    NULL, NULL, NULL,
                    SENSOR_PRIORITY,
                    0,
                    K_NO_WAIT);
    k_thread_name_set(&sensor_thread_data, "sensor");
    printk("[main] Sensor thread created (priority %d)\n", SENSOR_PRIORITY);

    /* --- Spawn Input Thread --- */
    k_thread_create(&input_thread_data,
                    input_stack,
                    K_THREAD_STACK_SIZEOF(input_stack),
                    input_thread_entry,
                    NULL, NULL, NULL,
                    INPUT_PRIORITY,
                    0,
                    K_NO_WAIT);
    k_thread_name_set(&input_thread_data, "input");
    printk("[main] Input thread created (priority %d)\n", INPUT_PRIORITY);

    /* --- Spawn Controller Thread --- */
    k_thread_create(&controller_thread_data,
                    controller_stack,
                    K_THREAD_STACK_SIZEOF(controller_stack),
                    controller_thread_entry,
                    NULL, NULL, NULL,
                    CONTROLLER_PRIORITY,
                    0,
                    K_NO_WAIT);
    k_thread_name_set(&controller_thread_data, "controller");
    printk("[main] Controller thread created (priority %d)\n", CONTROLLER_PRIORITY);

    /* --- Spawn Display Thread --- */
    k_thread_create(&display_thread_data,
                    display_stack,
                    K_THREAD_STACK_SIZEOF(display_stack),
                    display_thread_entry,
                    NULL, NULL, NULL,
                    DISPLAY_PRIORITY,
                    0,
                    K_NO_WAIT);
    k_thread_name_set(&display_thread_data, "display");
    printk("[main] Display thread created (priority %d)\n", DISPLAY_PRIORITY);

    printk("\n[main] All threads started. System running.\n");

    /* Main thread has nothing else to do */
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}

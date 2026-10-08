/**
 * @file common.h
 * @brief Shared data structures, enums, and message queue definitions
 *        for the Smart Environment Monitor & Controller.
 *
 * This header defines the inter-thread communication protocol:
 *   - sensor_data_t:   Sensor thread → Controller thread
 *   - input_event_t:   Input thread  → Controller thread
 *   - display_data_t:  Controller    → Display thread
 */

#ifndef COMMON_H
#define COMMON_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

/* ============================================================================
 * System Constants
 * ========================================================================= */

/** Temperature threshold range (°C) adjustable via potentiometer */
#define TEMP_THRESHOLD_MIN      20.0f
#define TEMP_THRESHOLD_MAX      40.0f
#define TEMP_THRESHOLD_DEFAULT  30.0f

/** Warning and critical offsets relative to threshold */
#define TEMP_WARNING_OFFSET     0.5f    /* Warning when within 0.5°C of threshold */
#define TEMP_CRITICAL_OFFSET    3.0f    /* Critical when 3.0°C above threshold */

/** Hysteresis to prevent rapid state oscillation (hunting) */
#define TEMP_HYSTERESIS         0.3f

/** History buffer for temperature chart */
#define TEMP_HISTORY_SIZE       60

/* ============================================================================
 * Actuator & Sensor Calibration Limits
 * ========================================================================= */

/**
 * Potentiometer ADC Calibration Limits:
 * - On ESP32-S3 with 3.3V supply and ADC_GAIN_1_4, the raw 12-bit ADC reading
 *   tops out at ~2850 at 3.3V (not 4095).
 * - Defining realistic MIN/MAX bounds ensures full 20.0°C - 40.0°C sweep.
 */
#define POT_ADC_RAW_MIN         20
#define POT_ADC_RAW_MAX         2850

/**
 * L298N Motor Driver Safety Limits (12V Adapter -> 3-6V DC Motor):
 * - Supply = 12V, L298N V_drop ≈ 1.8V -> Peak voltage ≈ 10.2V.
 * - MAX 50% PWM duty limits effective average voltage to ~5.1V - 5.5V (protects motor).
 * - MIN 28% PWM duty ensures motor overcomes static friction (~2.8V - 3.0V).
 */
#define FAN_PWM_MIN_DUTY_PCT    28.0f   /**< Min PWM duty when spinning (prevents stall) */
#define FAN_PWM_MAX_DUTY_PCT    50.0f   /**< Max PWM duty (caps voltage <= 5.5V on 12V supply) */

/**
 * LED PWM Limits:
 * - Limits maximum brightness to protect LED and avoid harsh glare.
 */
#define LED_PWM_MAX_DUTY_PCT    80.0f   /**< Max brightness limit (0-100%) */

/* ============================================================================
 * Enumerations
 * ========================================================================= */

/**
 * @brief System operating states (State Machine)
 */
typedef enum {
    STATE_IDLE = 0,     /**< Temp < (Threshold - 0.5°C) → fan off (0%), LED off */
    STATE_WARNING,      /**< Temp approaching Threshold (within 0.5°C) → fan 30%, LED blink slow */
    STATE_COOLING,      /**< Temp >= Threshold → fan 70%, LED on solid */
    STATE_CRITICAL,     /**< Temp >= Threshold + 3.0°C → fan 100%, LED blink fast */
} system_state_t;

/**
 * @brief Display screen modes (cycled by Button 1)
 */
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_SETTINGS,
    SCREEN_HISTORY,
    SCREEN_COUNT        /**< Total number of screens */
} screen_mode_t;

/**
 * @brief Input event types
 */
typedef enum {
    INPUT_EVT_MODE_PRESS = 0,   /**< Button 1 pressed → cycle screen */
    INPUT_EVT_CONFIRM_PRESS,    /**< Button 2 pressed → confirm action / save threshold */
    INPUT_EVT_THRESHOLD_CHANGE, /**< Potentiometer value changed (staged preview) */
} input_event_type_t;

/* ============================================================================
 * Data Structures
 * ========================================================================= */

/**
 * @brief Sensor data packet (Sensor Thread → Controller Thread)
 */
typedef struct {
    float temperature;      /**< Temperature in °C */
    float humidity;         /**< Relative humidity in % */
    float pressure;         /**< Atmospheric pressure in kPa */
    int64_t timestamp_ms;   /**< Kernel uptime when sampled */
} sensor_data_t;

/**
 * @brief Input event packet (Input Thread → Controller Thread)
 */
typedef struct {
    input_event_type_t type;
    float value;            /**< For THRESHOLD_CHANGE: staged threshold in °C */
} input_event_t;

/**
 * @brief Display data packet (Controller Thread → Display Thread)
 *        Contains everything the display needs to render.
 */
typedef struct {
    /* Current sensor readings */
    float temperature;
    float humidity;
    float pressure;

    /* System state */
    system_state_t state;
    screen_mode_t  screen;
    float temp_threshold;       /**< Active threshold controlling state machine */
    float pending_threshold;    /**< Staged threshold being adjusted by potentiometer */
    bool  threshold_pending;    /**< True if user changed potentiometer without confirming */
    float fan_duty_pct;         /**< Fan duty cycle 0-100% */

    /* Temperature history for chart */
    float temp_history[TEMP_HISTORY_SIZE];
    int   history_count;    /**< Number of valid entries in history */
    int   history_index;    /**< Next write index (circular) */
} display_data_t;

/* ============================================================================
 * Message Queue Declarations (defined in main.c)
 * ========================================================================= */

/** Sensor → Controller: sensor readings */
extern struct k_msgq sensor_msgq;

/** Input → Controller: user input events */
extern struct k_msgq input_msgq;

/** Controller → Display: display update data */
extern struct k_msgq display_msgq;

/* ============================================================================
 * Thread Entry Point Declarations
 * ========================================================================= */

/**
 * @brief Sensor thread: reads BME280 periodically and pushes to sensor_msgq.
 */
void sensor_thread_entry(void *arg1, void *arg2, void *arg3);

/**
 * @brief Input thread: handles button ISRs (via workqueue) and ADC reads.
 */
void input_thread_entry(void *arg1, void *arg2, void *arg3);

/**
 * @brief Controller thread: state machine, fan/LED control.
 */
void controller_thread_entry(void *arg1, void *arg2, void *arg3);

/**
 * @brief Display thread: LVGL rendering on ST7735S LCD.
 */
void display_thread_entry(void *arg1, void *arg2, void *arg3);

#endif /* COMMON_H */

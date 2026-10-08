/**
 * @file controller_thread.c
 * @brief Controller Thread - State Machine + Fan/LED Actuator Control
 *
 * The brain of the system. Receives sensor data and user input, runs a
 * state machine to determine the appropriate system response, controls
 * the fan (PWM) and LED (PWM), and pushes aggregated display data.
 *
 * State Machine:
 *   IDLE     → temp < (threshold - hysteresis)
 *   WARNING  → temp within WARNING_OFFSET of threshold
 *   COOLING  → temp > threshold
 *   CRITICAL → temp > (threshold + CRITICAL_OFFSET)
 *
 * Zephyr concepts demonstrated:
 *   - Message queue consumer (k_msgq_get with K_NO_WAIT for polling)
 *   - PWM driver (pwm_set_pulse_dt)
 *   - Kernel timer for LED blink patterns (k_timer)
 *   - State machine design pattern
 *   - Hysteresis for stable state transitions
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#include <string.h>

#include "common.h"

/* ============================================================================
 * Configuration
 * ========================================================================= */

/** Controller loop interval */
#define CONTROLLER_LOOP_MS      100

/** LED blink periods (in timer ticks) */
#define LED_BLINK_SLOW_MS       500     /* Warning: 1 Hz */
#define LED_BLINK_FAST_MS       100     /* Critical: 5 Hz */

/* ============================================================================
 * Devicetree Bindings
 * ========================================================================= */

/** LED and Fan are defined as pwm-leds in the overlay */
static const struct pwm_dt_spec pwm_led =
    PWM_DT_SPEC_GET(DT_ALIAS(my_led));
static const struct pwm_dt_spec pwm_fan =
    PWM_DT_SPEC_GET(DT_ALIAS(my_fan));

/* ============================================================================
 * Internal State
 * ========================================================================= */

static system_state_t current_state = STATE_IDLE;
static screen_mode_t  current_screen = SCREEN_DASHBOARD;
static float active_threshold = TEMP_THRESHOLD_DEFAULT;   /* Applied threshold for control */
static float pending_threshold = TEMP_THRESHOLD_DEFAULT;  /* Knob preview threshold */
static bool  threshold_pending = false;                   /* True if knob changed without confirm */
static float current_fan_duty = 0.0f;

/* Latest sensor data */
static sensor_data_t latest_sensor = {0};

/* Temperature history (circular buffer) */
static float temp_history[TEMP_HISTORY_SIZE] = {0};
static int   history_count = 0;
static int   history_index = 0;

/* LED blink state */
static bool led_blink_on = false;
static int  led_blink_counter = 0;

/* ============================================================================
 * PWM Helpers
 * ========================================================================= */

/**
 * @brief Set fan speed from logical percentage (0-100%).
 *
 * Maps logical speed to safe physical PWM duty cycle
 * [FAN_PWM_MIN_DUTY_PCT, FAN_PWM_MAX_DUTY_PCT] to protect 3-6V DC motors
 * when powered by a 12V adapter through L298N.
 *
 * @param logical_pct  Logical fan speed (0.0 to 100.0%)
 */
static void set_fan_speed(float logical_pct)
{
    if (logical_pct <= 0.0f) {
        current_fan_duty = 0.0f;
        pwm_set_pulse_dt(&pwm_fan, 0);
        return;
    }

    if (logical_pct > 100.0f) {
        logical_pct = 100.0f;
    }

    /* Save logical percentage for UI display */
    current_fan_duty = logical_pct;

    /* Linearly map logical 0-100% to safe physical hardware PWM [MIN, MAX] */
    float physical_duty = FAN_PWM_MIN_DUTY_PCT +
        (logical_pct / 100.0f) * (FAN_PWM_MAX_DUTY_PCT - FAN_PWM_MIN_DUTY_PCT);

    /* Absolute hardware safety clamp */
    if (physical_duty > FAN_PWM_MAX_DUTY_PCT) {
        physical_duty = FAN_PWM_MAX_DUTY_PCT;
    } else if (physical_duty < FAN_PWM_MIN_DUTY_PCT) {
        physical_duty = FAN_PWM_MIN_DUTY_PCT;
    }

    uint32_t pulse = (uint32_t)((physical_duty / 100.0f) * pwm_fan.period);
    int ret = pwm_set_pulse_dt(&pwm_fan, pulse);
    if (ret < 0) {
        printk("[ctrl] Fan PWM error: %d\n", ret);
    }
}

/**
 * @brief Set LED brightness from logical percentage (0-100%).
 *
 * Scales output to [0, LED_PWM_MAX_DUTY_PCT] for safety and eye comfort.
 *
 * @param logical_pct  Logical LED brightness (0.0 to 100.0%)
 */
static void set_led_brightness(float logical_pct)
{
    if (logical_pct <= 0.0f) {
        pwm_set_pulse_dt(&pwm_led, 0);
        return;
    }

    if (logical_pct > 100.0f) {
        logical_pct = 100.0f;
    }

    float physical_duty = (logical_pct / 100.0f) * LED_PWM_MAX_DUTY_PCT;
    if (physical_duty > LED_PWM_MAX_DUTY_PCT) {
        physical_duty = LED_PWM_MAX_DUTY_PCT;
    }

    uint32_t pulse = (uint32_t)((physical_duty / 100.0f) * pwm_led.period);
    int ret = pwm_set_pulse_dt(&pwm_led, pulse);
    if (ret < 0) {
        printk("[ctrl] LED PWM error: %d\n", ret);
    }
}

/* ============================================================================
 * State Machine
 * ========================================================================= */

/**
 * @brief Determine the next system state based on temperature and threshold.
 *        Implements hysteresis to prevent rapid oscillation (hunting).
 *
 * @param temp       Current temperature in °C
 * @param threshold  Current temperature threshold in °C
 * @return           New system state
 */
static system_state_t evaluate_state(float temp, float threshold)
{
    float warning_line  = threshold - TEMP_WARNING_OFFSET;
    float cooling_line  = threshold;
    float critical_line = threshold + TEMP_CRITICAL_OFFSET;

    /*
     * Hysteresis: when transitioning DOWN (cooling), require temperature
     * to drop further before changing state. This prevents rapid toggling
     * when temp hovers near a boundary.
     */
    switch (current_state) {
    case STATE_IDLE:
        if (temp >= critical_line) {
            return STATE_CRITICAL;
        } else if (temp >= cooling_line) {
            return STATE_COOLING;
        } else if (temp >= warning_line) {
            return STATE_WARNING;
        }
        return STATE_IDLE;

    case STATE_WARNING:
        if (temp >= critical_line) {
            return STATE_CRITICAL;
        } else if (temp >= cooling_line) {
            return STATE_COOLING;
        } else if (temp < (warning_line - TEMP_HYSTERESIS)) {
            return STATE_IDLE;
        }
        return STATE_WARNING;

    case STATE_COOLING:
        if (temp >= critical_line) {
            return STATE_CRITICAL;
        } else if (temp < (cooling_line - TEMP_HYSTERESIS)) {
            if (temp < (warning_line - TEMP_HYSTERESIS)) {
                return STATE_IDLE;
            }
            return STATE_WARNING;
        }
        return STATE_COOLING;

    case STATE_CRITICAL:
        if (temp < (critical_line - TEMP_HYSTERESIS)) {
            if (temp < (cooling_line - TEMP_HYSTERESIS)) {
                if (temp < (warning_line - TEMP_HYSTERESIS)) {
                    return STATE_IDLE;
                }
                return STATE_WARNING;
            }
            return STATE_COOLING;
        }
        return STATE_CRITICAL;

    default:
        return STATE_IDLE;
    }
}

/**
 * @brief Apply actuator outputs based on current state.
 */
static void apply_state_outputs(system_state_t state)
{
    switch (state) {
    case STATE_IDLE:
        /* Safe environment: fan OFF, LED OFF */
        set_fan_speed(0.0f);
        set_led_brightness(0.0f);
        break;

    case STATE_WARNING:
        /* Approaching threshold (within 0.5°C): gentle preventive fan (30%), slow LED blink */
        set_fan_speed(30.0f);
        led_blink_counter++;
        if (led_blink_counter >= (LED_BLINK_SLOW_MS / CONTROLLER_LOOP_MS)) {
            led_blink_counter = 0;
            led_blink_on = !led_blink_on;
        }
        set_led_brightness(led_blink_on ? 60.0f : 0.0f);
        break;

    case STATE_COOLING:
        /* At or above threshold: active cooling fan (70%), solid LED */
        set_fan_speed(70.0f);
        set_led_brightness(100.0f);  /* Solid on */
        break;

    case STATE_CRITICAL:
        /* Danger zone (> Threshold + 3.0°C): maximum fan (100%), rapid warning LED */
        set_fan_speed(100.0f);
        led_blink_counter++;
        if (led_blink_counter >= (LED_BLINK_FAST_MS / CONTROLLER_LOOP_MS)) {
            led_blink_counter = 0;
            led_blink_on = !led_blink_on;
        }
        set_led_brightness(led_blink_on ? 100.0f : 0.0f);
        break;
    }
}

/**
 * @brief Record temperature into the circular history buffer.
 */
static void record_history(float temp)
{
    temp_history[history_index] = temp;
    history_index = (history_index + 1) % TEMP_HISTORY_SIZE;
    if (history_count < TEMP_HISTORY_SIZE) {
        history_count++;
    }
}

/**
 * @brief Pack current state into display_data_t and push to display queue.
 */
static void push_display_update(void)
{
    display_data_t disp = {
        .temperature        = latest_sensor.temperature,
        .humidity           = latest_sensor.humidity,
        .pressure           = latest_sensor.pressure,
        .state              = current_state,
        .screen             = current_screen,
        .temp_threshold     = active_threshold,
        .pending_threshold  = pending_threshold,
        .threshold_pending  = threshold_pending,
        .fan_duty_pct       = current_fan_duty,
        .history_count      = history_count,
        .history_index      = history_index,
    };

    /* Copy history buffer */
    memcpy(disp.temp_history, temp_history, sizeof(temp_history));

    /* Purge old display data and put the latest */
    k_msgq_purge(&display_msgq);
    k_msgq_put(&display_msgq, &disp, K_NO_WAIT);
}

/* ============================================================================
 * Controller Thread Entry Point
 * ========================================================================= */

void controller_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    sensor_data_t sensor_data;
    input_event_t input_evt;
    system_state_t new_state;
    int display_update_counter = 0;

    /* Verify PWM devices are ready */
    if (!pwm_is_ready_dt(&pwm_led)) {
        printk("[ctrl] ERROR: LED PWM not ready\n");
        return;
    }
    if (!pwm_is_ready_dt(&pwm_fan)) {
        printk("[ctrl] ERROR: Fan PWM not ready\n");
        return;
    }

    /* Initialize outputs to off */
    set_fan_speed(0.0f);
    set_led_brightness(0.0f);

    printk("[ctrl] Controller thread running\n");
    printk("[ctrl] Initial active threshold: %.1f°C\n", (double)active_threshold);

    while (1) {
        k_msleep(CONTROLLER_LOOP_MS);

        /* --- Process all pending sensor readings --- */
        while (k_msgq_get(&sensor_msgq, &sensor_data, K_NO_WAIT) == 0) {
            latest_sensor = sensor_data;
            record_history(sensor_data.temperature);
        }

        /* --- Process all pending input events --- */
        while (k_msgq_get(&input_msgq, &input_evt, K_NO_WAIT) == 0) {
            switch (input_evt.type) {
            case INPUT_EVT_MODE_PRESS:
                /* Cycle through display screens */
                current_screen = (current_screen + 1) % SCREEN_COUNT;
                printk("[ctrl] Screen mode: %d\n", current_screen);
                break;

            case INPUT_EVT_CONFIRM_PRESS:
                /* Confirm button: Commit pending threshold to active threshold */
                if (threshold_pending) {
                    active_threshold = pending_threshold;
                    threshold_pending = false;
                    printk("[ctrl] Threshold CONFIRMED & APPLIED: %.1f°C\n",
                           (double)active_threshold);
                } else {
                    printk("[ctrl] Threshold confirmed (already active: %.1f°C)\n",
                           (double)active_threshold);
                }
                break;

            case INPUT_EVT_THRESHOLD_CHANGE:
                /* Stage new threshold preview only. Do NOT affect active control yet! */
                pending_threshold = input_evt.value;
                threshold_pending = true;
                break;
            }
        }

        /* --- Evaluate state machine against ACTIVE threshold ONLY --- */
        new_state = evaluate_state(latest_sensor.temperature, active_threshold);

        if (new_state != current_state) {
            printk("[ctrl] State: %d → %d (temp=%.1f, active_thresh=%.1f)\n",
                   current_state, new_state,
                   (double)latest_sensor.temperature,
                   (double)active_threshold);
            current_state = new_state;
            led_blink_counter = 0;  /* Reset blink phase on state change */
        }

        /* --- Apply outputs --- */
        apply_state_outputs(current_state);

        /* --- Push display update every 200ms (every 2 loops) --- */
        display_update_counter++;
        if (display_update_counter >= 2) {
            display_update_counter = 0;
            push_display_update();
        }
    }
}

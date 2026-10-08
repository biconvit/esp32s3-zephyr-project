/**
 * @file input_thread.c
 * @brief Input Thread - Button ISR/Workqueue + ADC Potentiometer Reader
 *
 * Handles two physical buttons using GPIO interrupts with debounce via
 * delayable workqueue, and reads a potentiometer via ADC for threshold
 * adjustment. All input events are pushed as input_event_t to input_msgq.
 *
 * Zephyr concepts demonstrated:
 *   - GPIO interrupt configuration (gpio_pin_interrupt_configure_dt)
 *   - GPIO callback / ISR (gpio_init_callback, gpio_add_callback)
 *   - Delayable workqueue for debounce (k_work_reschedule)
 *   - ADC channel configuration and reading (adc_channel_setup, adc_read)
 *   - Message queue producer (k_msgq_put)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#include "common.h"

/* ============================================================================
 * Configuration
 * ========================================================================= */

#define DEBOUNCE_DELAY_MS       50
#define ADC_READ_INTERVAL_MS    200

/* ============================================================================
 * Devicetree Bindings
 * ========================================================================= */

/* Buttons */
static const struct gpio_dt_spec btn_mode =
    GPIO_DT_SPEC_GET(DT_ALIAS(my_button_mode), gpios);
static const struct gpio_dt_spec btn_confirm =
    GPIO_DT_SPEC_GET(DT_ALIAS(my_button_confirm), gpios);

/* ADC */
#define MY_ADC_CH  DT_ALIAS(my_adc_channel)
static const struct device *adc_dev = DEVICE_DT_GET(DT_ALIAS(my_adc));
static const struct adc_channel_cfg adc_ch_cfg = ADC_CHANNEL_CFG_DT(MY_ADC_CH);

/* ============================================================================
 * GPIO Callback Data
 * ========================================================================= */

static struct gpio_callback btn_mode_cb_data;
static struct gpio_callback btn_confirm_cb_data;

/* ============================================================================
 * Delayable Work Items (for button debounce)
 * ========================================================================= */

static struct k_work_delayable mode_work;
static struct k_work_delayable confirm_work;

/* ============================================================================
 * Button ISRs → Workqueue
 * ========================================================================= */

/**
 * @brief GPIO ISR for Mode button (Button 1).
 *        Schedules debounced work item instead of processing directly in ISR.
 */
static void btn_mode_isr(const struct device *dev,
                         struct gpio_callback *cb,
                         uint32_t pins)
{
    k_work_reschedule(&mode_work, K_MSEC(DEBOUNCE_DELAY_MS));
}

/**
 * @brief GPIO ISR for Confirm button (Button 2).
 */
static void btn_confirm_isr(const struct device *dev,
                            struct gpio_callback *cb,
                            uint32_t pins)
{
    k_work_reschedule(&confirm_work, K_MSEC(DEBOUNCE_DELAY_MS));
}

/* ============================================================================
 * Debounced Work Handlers
 * ========================================================================= */

/**
 * @brief Work handler for Mode button (runs after debounce delay).
 *        Reads pin state and sends INPUT_EVT_MODE_PRESS if still active.
 */
static void mode_work_handler(struct k_work *work)
{
    int state = gpio_pin_get_dt(&btn_mode);

    if (state < 0) {
        printk("[input] Error reading mode button: %d\n", state);
        return;
    }

    if (state) {
        input_event_t evt = {
            .type = INPUT_EVT_MODE_PRESS,
            .value = 0.0f
        };
        k_msgq_put(&input_msgq, &evt, K_NO_WAIT);
        printk("[input] Mode button pressed\n");
    }
}

/**
 * @brief Work handler for Confirm button (runs after debounce delay).
 */
static void confirm_work_handler(struct k_work *work)
{
    int state = gpio_pin_get_dt(&btn_confirm);

    if (state < 0) {
        printk("[input] Error reading confirm button: %d\n", state);
        return;
    }

    if (state) {
        input_event_t evt = {
            .type = INPUT_EVT_CONFIRM_PRESS,
            .value = 0.0f
        };
        k_msgq_put(&input_msgq, &evt, K_NO_WAIT);
        printk("[input] Confirm button pressed\n");
    }
}

/* ============================================================================
 * ADC Helper: Read potentiometer and map to threshold range
 * ========================================================================= */

static uint16_t pot_raw_min = POT_ADC_RAW_MIN;
static uint16_t pot_raw_max = POT_ADC_RAW_MAX;

/**
 * @brief Reads the potentiometer ADC value and maps it to the
 *        temperature threshold range [TEMP_THRESHOLD_MIN, TEMP_THRESHOLD_MAX].
 *        Uses calibrated bounds for ESP32-S3 (3.3V rail -> ~2850 raw max).
 *
 * @param seq   Pointer to pre-configured ADC sequence
 * @param buf   Pointer to ADC buffer
 * @return      Mapped threshold value in °C, or negative on error
 */
static float read_potentiometer(struct adc_sequence *seq, uint16_t *buf)
{
    int ret = adc_read(adc_dev, seq);
    if (ret < 0) {
        return -1.0f;
    }

    uint16_t raw = *buf;

    /* Dynamic adaptation: expand range if hardware exceeds bounds */
    if (raw > pot_raw_max) {
        pot_raw_max = raw;
    }
    if (raw < pot_raw_min) {
        pot_raw_min = raw;
    }

    /* Normalize to 0.0 - 1.0 */
    float ratio;
    if (pot_raw_max > pot_raw_min) {
        ratio = (float)(raw - pot_raw_min) / (float)(pot_raw_max - pot_raw_min);
    } else {
        ratio = 0.5f;
    }

    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    /* Map to temperature range [TEMP_THRESHOLD_MIN, TEMP_THRESHOLD_MAX] */
    float threshold = TEMP_THRESHOLD_MIN +
                      ratio * (TEMP_THRESHOLD_MAX - TEMP_THRESHOLD_MIN);

    return threshold;
}

/* ============================================================================
 * Input Thread Entry Point
 * ========================================================================= */

void input_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int ret;
    uint16_t adc_buf;
    float prev_threshold = -1.0f;

    /* --- Initialize delayable work items --- */
    k_work_init_delayable(&mode_work, mode_work_handler);
    k_work_init_delayable(&confirm_work, confirm_work_handler);

    /* --- Configure Mode Button (GPIO interrupt) --- */
    if (!gpio_is_ready_dt(&btn_mode)) {
        printk("[input] ERROR: Mode button GPIO not ready\n");
        return;
    }
    ret = gpio_pin_configure_dt(&btn_mode, GPIO_INPUT);
    if (ret < 0) {
        printk("[input] ERROR: Could not configure mode button\n");
        return;
    }
    ret = gpio_pin_interrupt_configure_dt(&btn_mode, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("[input] ERROR: Could not configure mode button interrupt\n");
        return;
    }
    gpio_init_callback(&btn_mode_cb_data, btn_mode_isr, BIT(btn_mode.pin));
    gpio_add_callback(btn_mode.port, &btn_mode_cb_data);
    printk("[input] Mode button configured (GPIO%d)\n", btn_mode.pin);

    /* --- Configure Confirm Button (GPIO interrupt) --- */
    if (!gpio_is_ready_dt(&btn_confirm)) {
        printk("[input] ERROR: Confirm button GPIO not ready\n");
        return;
    }
    ret = gpio_pin_configure_dt(&btn_confirm, GPIO_INPUT);
    if (ret < 0) {
        printk("[input] ERROR: Could not configure confirm button\n");
        return;
    }
    ret = gpio_pin_interrupt_configure_dt(&btn_confirm, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("[input] ERROR: Could not configure confirm button interrupt\n");
        return;
    }
    gpio_init_callback(&btn_confirm_cb_data, btn_confirm_isr, BIT(btn_confirm.pin));
    gpio_add_callback(btn_confirm.port, &btn_confirm_cb_data);
    printk("[input] Confirm button configured (GPIO%d)\n", btn_confirm.pin);

    /* --- Configure ADC (Potentiometer) --- */
    if (!device_is_ready(adc_dev)) {
        printk("[input] ERROR: ADC not ready\n");
        return;
    }
    ret = adc_channel_setup(adc_dev, &adc_ch_cfg);
    if (ret < 0) {
        printk("[input] ERROR: ADC channel setup failed: %d\n", ret);
        return;
    }
    printk("[input] ADC configured for potentiometer\n");

    /* ADC sequence configuration */
    struct adc_sequence seq = {
        .channels    = BIT(adc_ch_cfg.channel_id),
        .buffer      = &adc_buf,
        .buffer_size = sizeof(adc_buf),
        .resolution  = DT_PROP(MY_ADC_CH, zephyr_resolution),
    };

    printk("[input] Input thread running\n");

    /* --- Main loop: periodically read potentiometer --- */
    /* Button events are handled asynchronously via ISR → workqueue */
    while (1) {
        k_msleep(ADC_READ_INTERVAL_MS);

        /* Read potentiometer and detect significant changes */
        float threshold = read_potentiometer(&seq, &adc_buf);
        if (threshold < 0) {
            continue;   /* ADC read failed */
        }

        /*
         * Send event if threshold changed by more than 0.2°C
         * to give smooth, responsive adjustment without flooding the queue.
         */
        float diff = threshold - prev_threshold;
        if (diff < 0) diff = -diff;  /* abs() */

        if (diff >= 0.2f || prev_threshold < 0.0f) {
            prev_threshold = threshold;

            input_event_t evt = {
                .type  = INPUT_EVT_THRESHOLD_CHANGE,
                .value = threshold,
            };
            k_msgq_put(&input_msgq, &evt, K_NO_WAIT);
        }
    }
}

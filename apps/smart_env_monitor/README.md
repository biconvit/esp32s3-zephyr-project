# 🌡️ Smart Environment Monitor & Controller

A real-time environment monitoring and control system built with **Zephyr RTOS** on **ESP32-S3-DevKitC**.

## Overview

This project reads temperature, humidity, and pressure from a BME280 sensor, displays data on a 160x80 ST7735S LCD with a multi-screen LVGL UI, allows threshold configuration via a potentiometer, and automatically controls a cooling fan when temperature exceeds the user-defined threshold.

## Hardware

| Component | Interface | GPIO |
|---|---|---|
| BME280 (Temp/Hum/Press) | I2C0 | SDA=GPIO15, SCL=GPIO16 |
| ST7735S LCD (160x80) | SPI2 | MOSI=10, MISO=11, SCLK=12, CS=9, DC=18, RST=8 |
| LED + 220Ω | PWM (LEDC CH0) | GPIO13 |
| Cooling Fan | PWM (LEDC CH1) | GPIO14 |
| Button 1 (Mode) | GPIO (ISR) | GPIO5 (Active Low, Pull-Up) |
| Button 2 (Confirm) | GPIO (ISR) | GPIO4 (Active Low, Pull-Up) |
| Potentiometer | ADC1 CH0 | GPIO1 |

## Architecture

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│ Sensor Thread│────►│  Controller  │────►│Display Thread│
│  (BME280)   │ msgq│   Thread     │ msgq│  (LVGL/LCD)  │
└─────────────┘     │ State Machine│     └──────────────┘
                    │ Fan/LED PWM  │
┌─────────────┐     │              │
│ Input Thread │────►│              │
│ (Btn+ADC)   │ msgq└──────────────┘
└─────────────┘
```

### Threads
- **Sensor** (Priority 5): Reads BME280 every 1s
- **Input** (Priority 4): Button ISR + Workqueue debounce, ADC polling every 200ms
- **Controller** (Priority 6): State machine with hysteresis, PWM control
- **Display** (Priority 7): LVGL rendering at ~20 FPS

### State Machine
| State | Condition | Fan | LED |
|---|---|---|---|
| IDLE | temp < threshold - 0.5°C | Off | Off |
| WARNING | temp near threshold (±0.5°C) | 30% | Slow blink |
| COOLING | temp > threshold | 50% | Solid on |
| CRITICAL | temp > threshold + 3°C | 100% | Fast blink |

### UI Screens (Button 1 cycles)
1. **Dashboard**: Temperature, humidity, fan bar, state indicator, threshold
2. **Settings**: Threshold adjustment bar (controlled by potentiometer)
3. **History**: Temperature trend line chart (last 60 samples)

## Zephyr RTOS Concepts Used
- Multi-threading (`k_thread_create`)
- Message Queues (`K_MSGQ_DEFINE`, `k_msgq_put/get`)
- GPIO Interrupts + Delayable Workqueue (debounce pattern)
- PWM Driver (`pwm_set_pulse_dt`)
- ADC Driver (`adc_read`)
- I2C Sensor Driver (`sensor_sample_fetch`)
- SPI Display + LVGL
- Devicetree Overlays
- Kconfig Configuration

## Build & Flash

```bash
# From the workspace root:
west build -b esp32s3_devkitc apps/smart_env_monitor -p
west flash
west espressif monitor
```

## Project Structure

```
smart_env_monitor/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── esp32s3_devkitc.overlay
├── src/
│   ├── main.c                  # Thread creation, queue definitions
│   ├── common.h                # Shared types, enums, queue externs
│   ├── sensor_thread.c/h       # BME280 reader
│   ├── input_thread.c/h        # Button ISR + ADC
│   ├── controller_thread.c/h   # State machine + actuators
│   ├── display_thread.c/h      # LVGL screen manager
│   └── ui/
│       ├── ui.h                # Color palette, screen interface
│       ├── screen_dashboard.c  # Real-time data display
│       ├── screen_settings.c   # Threshold adjustment UI
│       └── screen_history.c    # Temperature chart
└── README.md
```

## License

MIT

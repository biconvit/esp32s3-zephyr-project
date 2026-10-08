cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_env_monitor)

target_sources(app PRIVATE
    src/main.c
    src/sensor_thread.c
    src/input_thread.c
    src/controller_thread.c
    src/display_thread.c
    src/ui/screen_dashboard.c
    src/ui/screen_settings.c
    src/ui/screen_history.c
)

target_include_directories(app PRIVATE src)

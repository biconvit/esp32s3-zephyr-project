cmake_minimum_required(VERSION 3.20.0)
# Tìm bộ Zephyr có sẵn trong container thông qua biến $ENV{ZEPHYR_BASE}
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(01_blinky)

target_sources(app PRIVATE src/main.c)
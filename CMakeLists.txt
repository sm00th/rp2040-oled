# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(rp2040-oled)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_library(rp2040-oled
    src/rp2040-oled.c
    src/rp2040-oled.h
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(rp2040-oled pico_stdlib hardware_i2c)

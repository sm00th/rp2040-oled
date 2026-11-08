# SPDX-License-Identifier: MIT


add_library(oled-font
    font.c
)

target_include_directories(oled-font
    PUBLIC
        ../include
)

add_library(oled-display
    display.c
)

target_include_directories(oled-display
    PUBLIC
        ../include
)

target_link_libraries(oled-display
    PUBLIC
        hardware_spi
)

add_library(oled-spi
    spi.c
)

target_include_directories(oled-spi
    PUBLIC
        ../include
)

target_link_libraries(oled-spi
    PUBLIC
        hardware_spi
    PRIVATE
        hardware_gpio
)

add_library(oled-gfx
    gfx.c
)

target_include_directories(oled-gfx
    PUBLIC
        ../include
)

target_link_libraries(oled-gfx
    PUBLIC
        hardware_spi
    PRIVATE
        oled-font
        oled-spi
)

add_library(rp2040-oled
    rp2040-oled.c
)

target_include_directories(rp2040-oled
    PUBLIC
        ../include
)

target_link_libraries(rp2040-oled
    PUBLIC
        hardware_spi
    PRIVATE
        hardware_gpio
        pico_time
        oled-display
        oled-gfx
        oled-spi
)

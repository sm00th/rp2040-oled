# SPDX-License-Identifier: MIT


add_library(font
    font.c
)

target_include_directories(font
    ../include
)

add_library(display
    display.c
)

target_include_directories(display
    ../include
)

target_link_libraries(display
    PRIVATE
        pico_stdlib
        hardware_spi
)

add_library(spi
    spi.c
)

target_include_directories(spi
    ../include
)

target_link_libraries(spi
    PUBLIC
        hardware_spi
    INTERFACE
        pico_stdlib
    PRIVATE
        hardware_gpio
)

add_library(gfx
    gfx.c
)

target_include_directories(gfx
    ../include
)

target_link_libraries(gfx
    PUBLIC
        hardware_spi
        pico_stdlib
    PRIVATE
        font
        spi
)

add_library(rp2040-oled
    rp2040-oled.c
)

target_include_directories(rp2040-oled
    ../include
)

target_link_libraries(rp2040-oled
    PUBLIC
        hardware_spi
        pico_stdlib
    PRIVATE
        display
        gfx
        spi
)

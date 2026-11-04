/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
#pragma once

#include <stdint.h>
#include <stdlib.h>

// sh1106 startup commands, sent in this order with dc low. from adafruit's
// circuitpython setup for the macropad
extern const uint8_t oled_sh1106_init[];

extern const size_t oled_sh1106_init_len;

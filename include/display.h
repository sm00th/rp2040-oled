/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
/**
 * @file display.h
 * @brief Startup command sequence for the SH1106 controller.
 */
#pragma once

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief SH1106 startup commands, sent in this order with DC low.
 *
 * Taken from Adafruit's CircuitPython setup for the MacroPad.
 */
extern const uint8_t oled_sh1106_init[];

/** @brief Number of bytes in oled_sh1106_init. */
extern const size_t oled_sh1106_init_len;

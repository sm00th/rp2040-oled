/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
/**
 * @file font.h
 * @brief The 6x8 bitmap font.
 */
#pragma once

#include <stdint.h>

/**
 * @brief Glyphs for printable ASCII, starting at ' ' (32).
 *
 * Each character is 5 column bytes with bit 0 on top, so character `c` starts
 * at `font_6x8 + (c - 32) * 5`. rp2040_oled_write_string() adds a blank
 * column, which makes it 6x8.
 */
extern const uint8_t font_6x8[];

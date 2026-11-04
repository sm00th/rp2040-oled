/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
#pragma once

#include <stdint.h>

// printable ascii starting at ' ' (32). each character is 5 column bytes with
// bit 0 on top, so c starts at font_6x8 + (c - 32) * 5. write_string adds a
// blank column, which makes it 6x8
extern const uint8_t font_6x8[];

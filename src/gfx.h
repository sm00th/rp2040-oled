/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#include "include/rp2040-oled.h"

// sends the whole framebuffer, dirty or not. used to blank the screen during
// init
bool rp2040_oled_force_flush(rp2040_oled_t *oled);

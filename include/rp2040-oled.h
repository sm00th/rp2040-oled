/*
 * SPDX-License-Identifier: MIT
 * 2026, Artem Savkov, Ramzey Burdette
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rp2040-oled-types.h"

// most drawing functions take `render`. false only changes the framebuffer, and
// rp2040_oled_flush sends it later. true sends the change right away.

// sets up spi, resets the screen, and sends the startup commands
void rp2040_oled_init(rp2040_oled_t *oled);

// sets brightness, 0x00 to 0xff
bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast);

// turns the panel on or off. its ram is kept while off
bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled);

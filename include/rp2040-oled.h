/*
 * SPDX-License-Identifier: MIT
 * 2026, Artem Savkov, Ramzey Burdette
 */
/**
 * @file rp2040-oled.h
 * @brief Setup and panel control for an SH1106 OLED over SPI.
 *
 * Most drawing functions in gfx.h take a `render` flag. With `render` false,
 * only the framebuffer changes, and rp2040_oled_flush() sends it later. With
 * `render` true, the change is sent right away.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rp2040-oled-types.h"

/**
 * @brief Sets up SPI, resets the screen, and sends the startup commands.
 *
 * Fill in the config fields of @p oled (spi, pins, baudrate, invert, flip,
 * use_doublebuf) before calling. This sets the width and height, allocates the
 * framebuffers, and blanks the screen.
 *
 * @param oled Display to start.
 * @warning Only safe with `use_doublebuf` false. With doublebuf on, the
 *          initial flush runs before `dirty_buf` is allocated.
 */
void rp2040_oled_init(rp2040_oled_t *oled);

/**
 * @brief Sets the panel's brightness.
 *
 * @param oled Display to change.
 * @param contrast Brightness, 0x00 (dimmest) to 0xff (full).
 * @return true if the command was sent.
 */
bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast);

/**
 * @brief Turns the panel on or off.
 *
 * The controller keeps its RAM while off, so the picture comes back when the
 * panel is turned on again.
 *
 * @param oled Display to change.
 * @param enabled true to turn the panel on, false to turn it off.
 * @return true if the command was sent.
 */
bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled);

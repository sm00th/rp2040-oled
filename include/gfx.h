/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
#pragma once

#include "rp2040-oled-types.h"

// sends `size` bytes of page y from gdram, starting at column x. with
// doublebuf, copies that span from dirty_buf into gdram first
bool rp2040_oled_render_gdram(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                              size_t gdram_offset, uint8_t size);

// clears the framebuffer and the screen
bool rp2040_oled_clear(rp2040_oled_t *oled);

// clears the framebuffer. the screen changes on the next flush
bool rp2040_oled_clear_gdram(rp2040_oled_t *oled);

// draws `len` characters in the 6x8 font. y rounds down to a page
bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                              char *msg, size_t len, bool render);

// turns one pixel on (WHITE) or off (BLACK)
bool rp2040_oled_set_pixel(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                           rp2040_oled_color_t color, bool render);

// draws a sprite in page format: `width` bytes per page, bit 0 on top. clipped
// at the screen edges
bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, const uint8_t *sprite,
                             int16_t x, int16_t y, uint8_t width,
                             uint8_t height, rp2040_oled_color_t color,
                             bool render);

// draws a row by row 1 bit per pixel image, msb leftmost. pitch = bytes per row
bool rp2040_oled_draw_sprite_pitched(rp2040_oled_t *oled, uint8_t *sprite,
                                     int16_t x, int16_t y, uint8_t width,
                                     uint8_t height, uint8_t pitch,
                                     rp2040_oled_color_t color, bool render);

// draws a line. both ends must be on screen
bool rp2040_oled_draw_line(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                           uint8_t x1, uint8_t y1, rp2040_oled_color_t color,
                           bool render);

// draws a rectangle between two corners, outline or filled
bool rp2040_oled_draw_rectangle(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                                uint8_t x1, uint8_t y1,
                                rp2040_oled_color_t color, bool fill,
                                bool render);

// draws a circle centered at (x, y) with radius r
bool rp2040_oled_draw_circle(rp2040_oled_t *oled, int16_t x, int16_t y,
                             uint8_t r, rp2040_oled_color_t color, bool fill,
                             bool render);

// draws an ellipse centered at (x, y) with radii rx and ry
bool rp2040_oled_draw_ellipse(rp2040_oled_t *oled, int16_t x, int16_t y,
                              uint8_t rx, uint8_t ry, rp2040_oled_color_t color,
                              bool fill, bool render);

// sends everything that changed since the last flush
bool rp2040_oled_flush(rp2040_oled_t *oled);

/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
/**
 * @file gfx.h
 * @brief Drawing into the framebuffer and sending it to the screen.
 *
 * Coordinates are in pixels, with (0, 0) at the top left. With `render` false,
 * a function only changes the framebuffer, and rp2040_oled_flush() sends it
 * later. With `render` true, the change is sent right away.
 */
#pragma once

#include "rp2040-oled-types.h"

/**
 * @brief Sends a span of one page of gdram to the screen.
 *
 * With doublebuf, the span is first copied from `dirty_buf` into `gdram`.
 *
 * @param oled Display to draw on.
 * @param x Starting column.
 * @param y Page number (not a pixel row).
 * @param gdram_offset Index into gdram of the first byte to send.
 * @param size Number of bytes (columns) to send.
 * @return true if everything was sent.
 */
bool rp2040_oled_render_gdram(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                              size_t gdram_offset, uint8_t size);

/**
 * @brief Clears the framebuffer and the screen.
 *
 * @param oled Display to clear.
 * @return true on success.
 */
bool rp2040_oled_clear(rp2040_oled_t *oled);

/**
 * @brief Clears the framebuffer only.
 *
 * The screen changes on the next rp2040_oled_flush().
 *
 * @param oled Display to clear.
 * @return true on success.
 */
bool rp2040_oled_clear_gdram(rp2040_oled_t *oled);

/**
 * @brief Draws text in the 6x8 font.
 *
 * Each character is a blank column plus 5 font columns. Characters are ORed
 * in, so the background stays. Text has to fit on one page row.
 *
 * @param oled Display to draw on.
 * @param x Left edge, in pixels.
 * @param y Top edge, in pixels. Rounds down to a page (multiple of 8).
 * @param msg Characters to draw. Doesn't need to be null-terminated.
 * @param len Number of characters to draw.
 * @param render true to send the change right away.
 * @return false if the start is off screen or the text runs past the right
 *         edge.
 * @warning Characters aren't checked. Anything outside printable ASCII reads
 *          outside the font table.
 */
bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                              char *msg, size_t len, bool render);

/**
 * @brief Turns one pixel on or off.
 *
 * @param oled Display to draw on.
 * @param x Column.
 * @param y Row.
 * @param color OLED_COLOR_WHITE for on, OLED_COLOR_BLACK for off.
 * @param render true to send the change right away.
 * @return false if the pixel is off screen or @p color is
 *         OLED_COLOR_FULL_BYTE.
 */
bool rp2040_oled_set_pixel(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                           rp2040_oled_color_t color, bool render);

/**
 * @brief Draws a sprite stored in page format.
 *
 * Page format means `width` bytes per page, one byte per column, bit 0 on top.
 * The sprite is clipped at the screen edges, and can start partway into a page.
 *
 * @param oled Display to draw on.
 * @param sprite Sprite data, `width * ceil(height / 8)` bytes.
 * @param x Left edge. Can be negative.
 * @param y Top edge. Can be negative.
 * @param width Sprite width in pixels.
 * @param height Sprite height in pixels.
 * @param color How the sprite combines with the framebuffer.
 * @param render true to send the change right away.
 * @return false if the sprite is fully off the top or left, or a write fails.
 */
bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, const uint8_t *sprite,
                             int16_t x, int16_t y, uint8_t width,
                             uint8_t height, rp2040_oled_color_t color,
                             bool render);

/**
 * @brief Draws a row-by-row 1 bit per pixel image.
 *
 * Each row starts on a new byte, with the most significant bit leftmost. The
 * image is converted to page format, then drawn with rp2040_oled_draw_sprite().
 *
 * @param oled Display to draw on.
 * @param sprite Image data, `pitch * height` bytes.
 * @param x Left edge. Can be negative.
 * @param y Top edge. Can be negative.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param pitch Bytes per row in @p sprite.
 * @param color How the image combines with the framebuffer.
 * @param render true to send the change right away.
 * @return The result of rp2040_oled_draw_sprite().
 */
bool rp2040_oled_draw_sprite_pitched(rp2040_oled_t *oled, uint8_t *sprite,
                                     int16_t x, int16_t y, uint8_t width,
                                     uint8_t height, uint8_t pitch,
                                     rp2040_oled_color_t color, bool render);

/**
 * @brief Draws a line between two points.
 *
 * Vertical lines are written a whole byte per page. Every other line uses
 * Bresenham's algorithm with rp2040_oled_set_pixel().
 *
 * @param oled Display to draw on.
 * @param x0 Start column.
 * @param y0 Start row.
 * @param x1 End column.
 * @param y1 End row.
 * @param color OLED_COLOR_WHITE or OLED_COLOR_BLACK.
 * @param render true to send the change right away.
 * @return false if either end is off screen.
 */
bool rp2040_oled_draw_line(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                           uint8_t x1, uint8_t y1, rp2040_oled_color_t color,
                           bool render);

/**
 * @brief Draws a rectangle between two opposite corners.
 *
 * The corners can be given in any order. An outline is four lines, and a
 * filled rectangle is one vertical line per column.
 *
 * @param oled Display to draw on.
 * @param x0 Column of one corner.
 * @param y0 Row of one corner.
 * @param x1 Column of the opposite corner.
 * @param y1 Row of the opposite corner.
 * @param color OLED_COLOR_WHITE or OLED_COLOR_BLACK.
 * @param fill true for a filled rectangle, false for an outline.
 * @param render true to send the change right away.
 * @return Always true.
 */
bool rp2040_oled_draw_rectangle(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                                uint8_t x1, uint8_t y1,
                                rp2040_oled_color_t color, bool fill,
                                bool render);

/**
 * @brief Draws a circle with the midpoint circle algorithm.
 *
 * @param oled Display to draw on.
 * @param x Center column.
 * @param y Center row.
 * @param r Radius in pixels.
 * @param color OLED_COLOR_WHITE or OLED_COLOR_BLACK.
 * @param fill true for a filled circle, false for an outline.
 * @param render true to send the change right away.
 * @return Always true.
 */
bool rp2040_oled_draw_circle(rp2040_oled_t *oled, int16_t x, int16_t y,
                             uint8_t r, rp2040_oled_color_t color, bool fill,
                             bool render);

/**
 * @brief Draws an ellipse with the midpoint ellipse algorithm.
 *
 * Equal radii are drawn with rp2040_oled_draw_circle() instead.
 *
 * @param oled Display to draw on.
 * @param x Center column.
 * @param y Center row.
 * @param rx Horizontal radius in pixels.
 * @param ry Vertical radius in pixels.
 * @param color OLED_COLOR_WHITE or OLED_COLOR_BLACK.
 * @param fill true for a filled ellipse, false for an outline.
 * @param render true to send the change right away.
 * @return Always true.
 */
bool rp2040_oled_draw_ellipse(rp2040_oled_t *oled, int16_t x, int16_t y,
                              uint8_t rx, uint8_t ry, rp2040_oled_color_t color,
                              bool fill, bool render);

/**
 * @brief Sends everything that changed since the last flush.
 *
 * Changed columns next to each other are grouped into one write. Does nothing
 * if nothing changed. Resets the cursor to (0, 0).
 *
 * @param oled Display to flush.
 * @return Always true.
 */
bool rp2040_oled_flush(rp2040_oled_t *oled);

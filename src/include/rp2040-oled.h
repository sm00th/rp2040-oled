/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#ifndef _RP2040_OLED_H
#define _RP2040_OLED_H

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdlib.h>

// means "no pin". used for reset_pin
#define PIN_UNDEF 0xff

#define GPIO_LEVEL_HIGH 1
#define GPIO_LEVEL_LOW 0
// ram is split into pages 8 pixels tall. one byte is one column of a page, bit
// 0 on top
#define PAGE_BITS 8

// how new bytes combine with the framebuffer
typedef enum {
  OLED_COLOR_BLACK = 0, // AND: 0 bits turn pixels off
  OLED_COLOR_WHITE,     // OR: 1 bits turn pixels on
  OLED_COLOR_FULL_BYTE, // overwrite the whole byte
} rp2040_oled_color_t;

// controller command bytes. some get OR'd with a value, and some take an
// argument byte after them
typedef enum {
  OLED_CMD_SET_LC_ADDR = 0x00,               // OR in the column's low nibble
  OLED_CMD_SET_HC_ADDR = 0x10,               // OR in the column's high nibble
  OLED_CMD_SET_SSD1306_ADDR_MODE = 0x20,     // ssd1306 only
  OLED_CMD_SET_ADDR_PAGE = 0x20,             // sh1107 only
  OLED_CMD_SET_ADDR_VERTICAL = 0x21,         // sh1107 only
  OLED_CMD_SET_DISPLAY_STARTLINE0 = 0x40,    // start at line 0
  OLED_CMD_SET_CONTRAST = 0x81,              // arg: 0x00 to 0xff
  OLED_CMD_SET_CHARGE_PUMP = 0x8d,           // ssd1306 only
  OLED_CMD_SET_SEGMENT_REMAP_NORMAL = 0xa0,  // column 0 on the left
  OLED_CMD_SET_SEGMENT_REMAP_REVERSE = 0xa1, // columns mirrored
  OLED_CMD_SET_DISPLAY_RAM = 0xa4,           // show ram contents
  OLED_CMD_SET_DISPLAY_ENTIRE = 0xa5,        // light every pixel
  OLED_CMD_SET_DISPLAY_NORMAL = 0xa6,        // 1 bit = lit
  OLED_CMD_SET_DISPLAY_INVERSE = 0xa7,       // 0 bit = lit
  OLED_CMD_SET_MULTIPLEX_RATIO = 0xa8,       // arg: rows - 1
  OLED_CMD_SET_DC_DC = 0xad,                 // arg: dc-dc setting
  OLED_CMD_DISPLAY_OFF = 0xae,
  OLED_CMD_DISPLAY_ON = 0xaf,
  OLED_CMD_SET_PAGE_ADDR = 0xb0,           // OR in the page number
  OLED_CMD_SET_SCAN_DIR_NORMAL = 0xc0,     // rows top to bottom
  OLED_CMD_SET_SCAN_DIR_REVERSE = 0xc8,    // rows mirrored
  OLED_CMD_SET_DISPLAY_OFFSET = 0xd3,      // arg: vertical shift
  OLED_CMD_SET_DISPLAY_CLOCK = 0xd5,       // arg: clock divide and oscillator
  OLED_CMD_SET_PRECHARGE_PERIOD = 0xd9,    // arg: precharge period
  OLED_CMD_SET_COM_PINS = 0xda,            // arg: com pin layout
  OLED_CMD_SET_VCOM_DESELECT_LEVEL = 0xdb, // arg: vcom level
  OLED_CMD_SET_DISPLAY_STARTLINE = 0xdc,   // sh1107 only
  OLED_CMD_RMW_START = 0xe0,               // read-modify-write, unused
  OLED_CMD_RMW_END = 0xee,                 // read-modify-write, unused
} rp2040_oled_cmd_t;

// mirrors the image
typedef enum {
  FLIP_NONE = 0x0,
  FLIP_HORIZONTAL = 0x1,
  FLIP_VERTICAL = 0x2,
  FLIP_BOTH = (FLIP_HORIZONTAL | FLIP_VERTICAL)
} rp2040_oled_flip_t;

// one per display. set the config fields, and init fills in the rest
typedef struct _rp2040_oled {
  // config: set before calling rp2040_oled_init
  spi_inst_t *spi;
  uint8_t sck_pin;
  uint8_t mosi_pin;
  uint8_t cs_pin;
  uint8_t dc_pin; // low = commands, high = pixels
  uint32_t baudrate;
  uint8_t reset_pin; // PIN_UNDEF = not wired up
  uint8_t width;     // set by init
  uint8_t height;    // set by init
  bool invert;
  rp2040_oled_flip_t flip;

  // driver state, managed by the library. gdram is the framebuffer, or with
  // doublebuf, what's on the screen right now
  uint8_t *gdram;
  size_t gdram_size;
  // where the next write lands. x = column, y = page
  struct {
    uint8_t x;
    uint8_t y;
  } cursor;
  // doublebuf on: the framebuffer drawing goes into. off: one dirty bit per
  // column
  uint8_t *dirty_buf;
  size_t dirty_buf_size;
  bool is_dirty; // changed since the last flush
  bool use_doublebuf;
} rp2040_oled_t;

#ifdef __cplusplus
extern "C" {
#endif


// most drawing functions take `render`. false only changes the framebuffer, and
// rp2040_oled_flush sends it later. true sends the change right away.

// sets up spi, resets the screen, and sends the startup commands
void rp2040_oled_init(rp2040_oled_t *oled);
// clears the framebuffer and the screen
bool rp2040_oled_clear(rp2040_oled_t *oled);
// clears the framebuffer. the screen changes on the next flush
bool rp2040_oled_clear_gdram(rp2040_oled_t *oled);
// sets brightness, 0x00 to 0xff
bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast);
// turns the panel on or off. its ram is kept while off
bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled);
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

#ifdef __cplusplus
}
#endif
#endif /* _RP2040_OLED_H */

/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#ifndef _RP2040_OLED_H
#define _RP2040_OLED_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define PIN_UNDEF 0xff

#define GPIO_LEVEL_HIGH 1
#define GPIO_LEVEL_LOW 0
#define PAGE_BITS 8

enum {
        OLED_CB_CONTINUATION_BIT = 0x80,
        OLED_CB_DATA_BIT         = 0x40,
};

typedef enum {
        OLED_128x128 = 1,
        OLED_128x64,
        OLED_128x32,
        OLED_132x64,
        OLED_96x16,
        OLED_64x128,
        OLED_64x32,
        OLED_72x40,
} rp2040_oled_size_t;

typedef enum {
        OLED_COLOR_BLACK = 0,
        OLED_COLOR_WHITE,
        OLED_COLOR_FULL_BYTE,
} rp2040_oled_color_t;

typedef enum {
        OLED_NOT_FOUND = -1,
        OLED_SSD1306_3C,
        OLED_SSD1306_3D,
        OLED_SH1106_3C,
        OLED_SH1106_3D,
        OLED_SH1107_3C,
        OLED_SH1107_3D,
} rp2040_oled_type_t;

typedef enum {
        OLED_CMD_SET_LC_ADDR               = 0x00,
        OLED_CMD_SET_HC_ADDR               = 0x10,
        OLED_CMD_SET_SSD1306_ADDR_MODE     = 0x20,
        OLED_CMD_SET_ADDR_PAGE             = 0x20,
        OLED_CMD_SET_ADDR_VERTICAL         = 0x21,
        OLED_CMD_SET_DISPLAY_STARTLINE0    = 0x40,
        OLED_CMD_SET_CONTRAST              = 0x81,
        OLED_CMD_SET_CHARGE_PUMP           = 0x8d,
        OLED_CMD_SET_SEGMENT_REMAP_NORMAL  = 0xa0,
        OLED_CMD_SET_SEGMENT_REMAP_REVERSE = 0xa1,
        OLED_CMD_SET_DISPLAY_RAM           = 0xa4,
        OLED_CMD_SET_DISPLAY_ENTIRE        = 0xa5,
        OLED_CMD_SET_DISPLAY_NORMAL        = 0xa6,
        OLED_CMD_SET_DISPLAY_INVERSE       = 0xa7,
        OLED_CMD_SET_MULTIPLEX_RATIO       = 0xa8,
        OLED_CMD_SET_DC_DC                 = 0xad,
        OLED_CMD_DISPLAY_OFF               = 0xae,
        OLED_CMD_DISPLAY_ON                = 0xaf,
        OLED_CMD_SET_PAGE_ADDR             = 0xb0,
        OLED_CMD_SET_SCAN_DIR_NORMAL       = 0xc0,
        OLED_CMD_SET_SCAN_DIR_REVERSE      = 0xc8,
        OLED_CMD_SET_DISPLAY_OFFSET        = 0xd3,
        OLED_CMD_SET_DISPLAY_CLOCK         = 0xd5,
        OLED_CMD_SET_PRECHARGE_PERIOD      = 0xd9,
        OLED_CMD_SET_COM_PINS              = 0xda,
        OLED_CMD_SET_VCOM_DESELECT_LEVEL   = 0xdb,
        OLED_CMD_SET_DISPLAY_STARTLINE     = 0xdc,
        OLED_CMD_RMW_START                 = 0xe0,
        OLED_CMD_RMW_END                   = 0xee,
} rp2040_oled_cmd_t;

typedef enum {
        FLIP_NONE       = 0x0,
        FLIP_HORIZONTAL = 0x1,
        FLIP_VERTICAL   = 0x2,
        FLIP_BOTH       = (FLIP_HORIZONTAL | FLIP_VERTICAL)
} rp2040_oled_flip_t;

typedef struct _rp2040_oled {
        i2c_inst_t         *i2c;
        uint8_t            sda_pin;
        uint8_t            scl_pin;
        uint32_t           baudrate;
        uint8_t            addr;
        uint8_t            reset_pin;
        rp2040_oled_size_t size;
        uint8_t            width;
        uint8_t            height;
        bool               invert;
        rp2040_oled_flip_t flip;
        uint8_t            *gdram;
        size_t             gdram_size;
        struct {
                uint8_t x;
                uint8_t y;
        } cursor;
        uint8_t *dirty_buf;
        size_t  dirty_buf_size;
        bool    is_dirty;
} rp2040_oled_t;

#ifdef __cplusplus
extern "C" {
#endif

rp2040_oled_type_t rp2040_oled_init(rp2040_oled_t *oled);
bool rp2040_oled_clear(rp2040_oled_t *oled);
bool rp2040_oled_clear_gdram(rp2040_oled_t *oled);
bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast);
bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled);
bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y, char *msg,
                              size_t len);
bool rp2040_oled_set_pixel(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                           rp2040_oled_color_t color, bool render);
bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, const uint8_t *sprite, int16_t x,
                             int16_t y, uint8_t width, uint8_t height,
                             rp2040_oled_color_t color);
bool rp2040_oled_draw_sprite_pitched(rp2040_oled_t *oled, uint8_t *sprite, int16_t x,
                                     int16_t y, uint8_t width, uint8_t height, uint8_t pitch,
                                     rp2040_oled_color_t color);
bool rp2040_oled_draw_line(rp2040_oled_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                           rp2040_oled_color_t color, bool render);
bool rp2040_oled_draw_rectangle(rp2040_oled_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                rp2040_oled_color_t color, bool fill, bool render);
bool rp2040_oled_draw_circle(rp2040_oled_t *oled, int16_t x, int16_t y, uint8_t r,
                             rp2040_oled_color_t color, bool fill, bool render);
bool rp2040_oled_draw_ellipse(rp2040_oled_t *oled, int16_t x, int16_t y, uint8_t rx,
                              uint8_t ry, rp2040_oled_color_t color, bool fill,
                              bool render);
bool rp2040_oled_flush(rp2040_oled_t *oled);

#ifdef __cplusplus
}
#endif
#endif /* _RP2040_OLED_H */

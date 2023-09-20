/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#include <stdlib.h>
#include <string.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/error.h"

#include "rp2040-oled.h"
#include "rp2040-oled-internal.h"

static void rp2040_i2c_init(rp2040_oled_t *oled)
{
        i2c_init(oled->i2c, oled->baudrate);

        gpio_set_function(oled->sda_pin, GPIO_FUNC_I2C);
        gpio_set_function(oled->scl_pin, GPIO_FUNC_I2C);

        gpio_pull_up(oled->sda_pin);
        gpio_pull_up(oled->scl_pin);
}

static uint8_t *rp2040_oled_alloc_data_buf(size_t size)
{
        uint8_t *buf;

        buf = malloc(size + 1);
        memset(buf, 0x00, size + 1);
        buf[0] = OLED_CB_DATA_BIT;

        return buf + 1;
}

static void rp2040_oled_free_data_buf(uint8_t *buf)
{
        free(buf - 1);
}

static bool rp2040_i2c_test_addr(rp2040_oled_t *oled, uint8_t addr)
{
        uint8_t buf;
        int ret;

        ret = i2c_read_blocking(oled->i2c, addr, &buf, 1, false);
        return ret != PICO_ERROR_GENERIC;
}

static int rp2040_i2c_read_register(rp2040_oled_t *oled, uint8_t reg, uint8_t *data, size_t len)
{
        int ret;

        ret = i2c_write_blocking(oled->i2c, oled->addr, &reg, 1, true);
        if (ret < 0)
                return ret;

        ret = i2c_read_blocking(oled->i2c, oled->addr, data, len, false);
        return ret;
}

static size_t rp2040_i2c_write(rp2040_oled_t *oled, const uint8_t *data, size_t len)
{
        uint8_t buf[32];
        size_t sent = 0;
        size_t ret = 0;
        uint8_t leftover;
        while (len - sent >= 32) {
                if (sent == 0) {
                        memcpy(buf, data, 32);
                        data += 32;
                        sent += 32;
                } else {
                        buf[0] = OLED_CB_DATA_BIT;
                        memcpy(buf + 1, data, 31);
                        data += 31;
                        sent += 31;
                }
                ret = i2c_write_blocking(oled->i2c, oled->addr, buf, 32, true);
        }

        leftover = len - sent;
        if (leftover == len) {
                ret = i2c_write_blocking(oled->i2c, oled->addr, data, len, true);
                return ret == len ? ret : -1;

        }
        if (leftover > 0) {
                buf[0] = OLED_CB_DATA_BIT;
                memcpy(buf + 1 , data, leftover);
                ret = i2c_write_blocking(oled->i2c, oled->addr, buf, leftover + 1, true);
                if (ret != (leftover + 1))
                        return -1;

                sent += leftover;
        }

        return sent;
}

static bool rp2040_oled_write_command(rp2040_oled_t *oled, uint8_t cmd)
{
        uint8_t buf[] = { 0x00, cmd };
        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_write_command_with_arg(rp2040_oled_t *oled, uint8_t cmd, uint8_t arg)
{
        uint8_t buf[] = { 0x00, cmd, arg };
        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_set_position(rp2040_oled_t *oled, uint8_t x, uint8_t y)
{
        uint8_t buf[4];

        y /= 8;

        if (oled->size == OLED_64x32) {
                x += 32;
                if (oled->flip == 0)
                        y += 4;
        } else if (oled->size == OLED_132x64) {
                x += 2;
        } else if (oled->size == OLED_96x16) {
                if (oled->flip == 0)
                        y += 2;
                else
                        x += 32;
        } else if (oled->size == OLED_72x40) {
                x += 28;
                if (oled->flip == 0)
                        y += 3;
        }

        buf[0] = 0x00;
        buf[1] = OLED_CMD_SET_PAGE_ADDR | y;
        buf[2] = OLED_CMD_SET_LC_ADDR | (x & 0x0f);
        buf[3] = OLED_CMD_SET_HC_ADDR | (x >> 4);

        oled->cursor.x = x;
        oled->cursor.y = y;

        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_write_gdram(rp2040_oled_t *oled, uint8_t *buf, size_t size,
                                    rp2040_oled_color_t color, bool render)
{
        size_t gdram_offset = oled->cursor.x + (oled->cursor.y * oled->width);

        if (oled->cursor.x + size > oled->width)
                return false;

        if (color != OLED_COLOR_FULL_BYTE) {
                for (size_t i = 0; i < size; i++) {
                        if (color == OLED_COLOR_WHITE)
                                buf[i] = buf[i] | *(oled->gdram + gdram_offset + i);
                        else if (color == OLED_COLOR_BLACK)
                                buf[i] = buf[i] & *(oled->gdram + gdram_offset + i);
                }
        }
        memcpy(oled->gdram + gdram_offset, buf, size);

        if (!render) {
                if (!oled->dirty_area.is_dirty) {
                        oled->dirty_area.is_dirty = true;
                        oled->dirty_area.y0 = oled->cursor.y;
                        oled->dirty_area.y1 = oled->cursor.y;
                        oled->dirty_area.x0 = oled->cursor.x;
                        oled->dirty_area.x1 = oled->cursor.x + size;
                } else {
                        if (oled->cursor.y < oled->dirty_area.y0)
                                oled->dirty_area.y0 = oled->cursor.y;
                        if (oled->cursor.y > oled->dirty_area.y1)
                                oled->dirty_area.y1 = oled->cursor.y;

                        if (oled->cursor.x < oled->dirty_area.x0)
                                oled->dirty_area.x0 = oled->cursor.x;
                        if (oled->cursor.x > oled->dirty_area.x1)
                                oled->dirty_area.x1 = oled->cursor.x;
                }
        }

        oled->cursor.x += size;

        if (!render)
                return true;

        return rp2040_i2c_write(oled, buf - 1, size + 1) == size + 1;
}

bool rp2040_oled_flush(rp2040_oled_t *oled)
{
        uint8_t *buf;
        uint8_t width = oled->dirty_area.x1 - oled->dirty_area.x0 + 1;

        if (!oled->dirty_area.is_dirty)
                return true;

        buf = rp2040_oled_alloc_data_buf(width);
        for (uint8_t y = oled->dirty_area.y0; y <= oled->dirty_area.y1; y++) {
                size_t gdram_offset = oled->dirty_area.x0 + (y * oled->width);
                memcpy(buf, oled->gdram + gdram_offset, width);
                if (!rp2040_oled_set_position(oled, oled->dirty_area.x0, y * 8)) {
                        return false;
                }
                if (rp2040_i2c_write(oled, buf - 1, width + 1) != width + 1) {
                        rp2040_oled_free_data_buf(buf);
                        return false;
                }
        }
        rp2040_oled_free_data_buf(buf);

        oled->dirty_area.x0 = 0;
        oled->dirty_area.y0 = 0;
        oled->dirty_area.x1 = 0;
        oled->dirty_area.y1 = 0;
        oled->dirty_area.is_dirty = false;

        oled->cursor.x = 0;
        oled->cursor.y = 0;

        return true;
}

static void rp2040_oled_reset(rp2040_oled_t *oled)
{
        gpio_put(oled->reset_pin, GPIO_LEVEL_LOW);
        sleep_ms(50);
        gpio_put(oled->reset_pin, GPIO_LEVEL_HIGH);
        sleep_ms(10);
}

static int rp2040_oled_display_init(rp2040_oled_t *oled)
{
        const uint8_t *initbuf;
        size_t initlen;

        switch(oled->size) {
                case OLED_128x128:
                        oled->width =  128;
                        oled->height = 128;
                        initbuf = oled128_initbuf;
                        initlen = sizeof(oled128_initbuf);
                        break;
                case OLED_128x64:
                        oled->width =  128;
                        oled->height = 64;
                        initbuf = oled64_initbuf;
                        initlen = sizeof(oled64_initbuf);
                        break;
                case OLED_128x32:
                        oled->width =  128;
                        oled->height = 32;
                        initbuf = oled32_initbuf;
                        initlen = sizeof(oled32_initbuf);
                        break;
                case OLED_132x64:
                        oled->width =  132;
                        oled->height = 64;
                        initbuf = oled64_initbuf;
                        initlen = sizeof(oled64_initbuf);
                        break;
                case OLED_96x16:
                        oled->width =  96;
                        oled->height = 16;
                        initbuf = oled32_initbuf;
                        initlen = sizeof(oled32_initbuf);
                        break;
                case OLED_72x40:
                        oled->width =  72;
                        oled->height = 40;
                        initbuf = oled72_initbuf;
                        initlen = sizeof(oled72_initbuf);
                        break;
                case OLED_64x128:
                        oled->width =  64;
                        oled->height = 128;
                        initbuf = oled64x128_initbuf;
                        initlen = sizeof(oled64x128_initbuf);
                        break;
                case OLED_64x32:
                        oled->width =  64;
                        oled->height = 32;
                        initbuf = oled64_initbuf;
                        initlen = sizeof(oled64_initbuf);
                        break;
                default:
                        return -1;
        };

        rp2040_i2c_write(oled, initbuf, initlen);

        if (oled->invert)
                rp2040_oled_write_command(oled, OLED_CMD_SET_DISPLAY_INVERSE);

        if (oled->flip & FLIP_HORIZONTAL)
                rp2040_oled_write_command(oled, OLED_CMD_SET_SEGMENT_REMAP_NORMAL);
        else if (oled->flip & FLIP_VERTICAL)
                rp2040_oled_write_command(oled, OLED_CMD_SET_SCAN_DIR_NORMAL);

        oled->gdram_size = oled->width * oled->height / 8;
        oled->gdram = malloc(oled->gdram_size);
        memset(oled->gdram, 0x00, oled->gdram_size);

        oled->cursor.x = 0;
        oled->cursor.y = 0;

        oled->dirty_area.x0 = 0;
        oled->dirty_area.y0 = 0;
        oled->dirty_area.x1 = 0;
        oled->dirty_area.y1 = 0;
        oled->dirty_area.is_dirty = false;

        return 0;
}

static uint8_t rp2040_oled_scan(rp2040_oled_t *oled)
{
        uint8_t i;
        uint8_t addr = 0x00;

        for (i = 0; i < sizeof(scan_addrs); i++) {
                addr = scan_addrs[i];
                if (rp2040_i2c_test_addr(oled, addr))
                        return addr;
        }

        return PIN_UNDEF;
}

bool rp2040_oled_is_sh1106(rp2040_oled_t *oled)
{
        const uint8_t TEST_DATA[] = { 0xf2, 0x3a, 0x45, 0x8b, 0x00 };
        uint8_t buf[4];
        uint8_t i;

        rp2040_oled_set_power(oled, false);

        for (i = 0; i < sizeof(TEST_DATA); i++) {
                buf[0] = OLED_CB_CONTINUATION_BIT;
                buf[1] = OLED_CMD_RMW_START;
                buf[2] = 0xc0;
                if (rp2040_i2c_write(oled, buf, 3) != 3)
                        break;

                if (i > 0 && buf[1] != TEST_DATA[i - 1])
                        break;

                buf[0] = 0xc0;
                buf[1] = TEST_DATA[i];
                buf[2] = OLED_CB_CONTINUATION_BIT;
                buf[3] = OLED_CMD_RMW_END;

                if (rp2040_i2c_write(oled, buf, 4) != 4)
                        break;
        }

        rp2040_oled_set_power(oled, true);

        return i == sizeof(TEST_DATA);
}

rp2040_oled_type_t rp2040_oled_autodetect(rp2040_oled_t *oled)
{
        rp2040_oled_type_t type = OLED_NOT_FOUND;
        uint8_t status = 0x00;

        if (rp2040_i2c_read_register(oled, 0x00, &status, 1) < 0)
                return OLED_NOT_FOUND;

        status &= 0x0f;

        if ((status == 0x07 || status == 0x0f) && oled->size == OLED_128x128) {
                oled->flip = !oled->flip;
                type = OLED_SH1107_3C;
        } else if (status == 0x08) {
                type = OLED_SH1106_3C;
        } else if (status == 0x03 || status == 0x06 || status == 0x07) {
                if (rp2040_oled_is_sh1106(oled)) {
                        type = OLED_SH1106_3C;
                } else {
                        type = OLED_SSD1306_3C;
                }
        }

        if (type != OLED_NOT_FOUND && oled->addr == 0x3d)
                type++;

        return type;
}

rp2040_oled_type_t rp2040_oled_init(rp2040_oled_t *oled)
{
        rp2040_oled_type_t type = OLED_NOT_FOUND;
        rp2040_i2c_init(oled);

        if (oled->reset_pin != PIN_UNDEF) {
                gpio_set_dir(oled->reset_pin, GPIO_OUT);
                rp2040_oled_reset(oled);
        }

        if (oled->addr == PIN_UNDEF || oled->addr == 0x00) {
                oled->addr = rp2040_oled_scan(oled);
                if (oled->addr == PIN_UNDEF)
                        return OLED_NOT_FOUND;
        } else if (!rp2040_i2c_test_addr(oled, oled->addr)) {
                return OLED_NOT_FOUND;
        }

        type = rp2040_oled_autodetect(oled);

        rp2040_oled_display_init(oled);

        return type;
}

static bool rp2040_oled_fill(rp2040_oled_t *oled, uint8_t fill_byte)
{
        uint8_t *fill_buf;
        size_t fill_buf_size = oled->width;

        fill_buf = rp2040_oled_alloc_data_buf(fill_buf_size);
        memset(fill_buf, fill_byte, fill_buf_size);

        for (uint8_t y = 0; y < oled->height; y += 8) {
                if (!rp2040_oled_set_position(oled, 0, y)) {
                        rp2040_oled_free_data_buf(fill_buf);
                        return false;
                }

                if (!rp2040_oled_write_gdram(oled, fill_buf, fill_buf_size, OLED_COLOR_FULL_BYTE, true)) {
                        rp2040_oled_free_data_buf(fill_buf);
                        return false;
                }
        }

        rp2040_oled_free_data_buf(fill_buf);
        return true;
}

bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast)
{
        return rp2040_oled_write_command_with_arg(oled, OLED_CMD_SET_CONTRAST, contrast);
}

bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled)
{
        return rp2040_oled_write_command(oled, enabled ? OLED_CMD_DISPLAY_ON : OLED_CMD_DISPLAY_OFF);
}

bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y, char *msg, size_t len)
{
        uint8_t *buf;
        size_t buf_size = len * 6;
        size_t i = 0;
        if (x >= oled->width || y >= oled->height)
                return false;

        if (!rp2040_oled_set_position(oled, x, y))
                return false;

        buf = rp2040_oled_alloc_data_buf(buf_size);

        for (i = 0; i < len; i++) {
                uint8_t font_index = msg[i] - 32;

                buf[i * 6] = 0x00;
                memcpy(buf + (1 + (i * 6)), font_6x8 + (font_index * 5), 5);
        }

        if (!rp2040_oled_write_gdram(oled, buf, buf_size, OLED_COLOR_WHITE, true)) {
                rp2040_oled_free_data_buf(buf);
                return false;
        }

        rp2040_oled_free_data_buf(buf);
        return true;
}

bool rp2040_oled_set_pixel(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                           rp2040_oled_color_t color, bool render)
{
        bool ret = true;
        uint8_t page = y / 8;
        uint8_t page_offset = y % 8;
        uint8_t bit_mask = 1 << page_offset;
        uint8_t buf[2] = {OLED_CB_DATA_BIT, 0x00};

        if (x >= oled->width || y >= oled->height)
                return false;

        buf[1] = oled->gdram[x + (page * oled->width)];
        if (color == OLED_COLOR_WHITE)
                buf[1] |= bit_mask;
        else if (OLED_COLOR_BLACK)
                buf[1] &= ~bit_mask;
        else
                return false;

        if (!rp2040_oled_set_position(oled, x, page * 8))
                return false;

        if (!rp2040_oled_write_gdram(oled, buf + 1, 1, color, render))
                ret = false;

        return ret;
}

bool rp2040_oled_draw_line(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                           uint8_t x1, uint8_t y1, rp2040_oled_color_t color) {
        int16_t dx = abs(x1 - x0);
        int16_t dy = -abs(y1 - y0);
        int8_t sx = x0 < x1 ? 1 : -1;
        int8_t sy = y0 < y1 ? 1 : -1;
        int16_t err = dx + dy;
        int16_t err2;
        uint8_t bit_mask = 0x00;
        uint8_t buf[2] = {OLED_CB_DATA_BIT, 0x00};

        if (x0 < 0 || x1 < 0 || y0 < 0 || y1 < 0 || x0 >= oled->width ||
            x1 >= oled->width || y0 >= oled->height || y1 >= oled->height)
                return false;

        if (x0 == x1) {
                if (y0 / 8 == y1 / 8) {
                        for (uint8_t y = y0; y != y1; y += sy) {
                                bit_mask |= 1 << (sy == 1 ? y % 8 : 8 - y % 8);
                        }
                        rp2040_oled_set_position(oled, x0, y0);
                        buf[1] = bit_mask;
                        rp2040_oled_write_gdram(oled, buf + 1, 1, color, true);
                } else {
                        uint8_t tshift, bshift, y;
                        if (y0 > y1) {
                                y = y1;
                                y1 = y0;
                                y0 = y;
                        }

                        tshift = 8 - y0 % 8;
                        bshift = y1 % 8;
                        y = y0;

                        if (tshift) {
                                bit_mask = 0x00;
                                for (uint8_t i = 0; i < tshift; i++) {
                                        bit_mask |= 1 << (7 - i);
                                }
                                rp2040_oled_set_position(oled, x0, y);
                                buf[1] = bit_mask;
                                rp2040_oled_write_gdram(oled, buf + 1, 1, color, false);
                                y += tshift;
                        }

                        buf[1] = 0xff;
                        while (y / 8 < y1 / 8) {
                                rp2040_oled_set_position(oled, x0, y);
                                rp2040_oled_write_gdram(oled, buf + 1, 1, color, false);
                                y += 8;
                        }

                        if (bshift) {
                                bit_mask = 0x00;
                                for (uint8_t i = 0; i < bshift; i++) {
                                        bit_mask |= 1 << i;
                                }
                                rp2040_oled_set_position(oled, x0, y);
                                buf[1] = bit_mask;
                                rp2040_oled_write_gdram(oled, buf + 1, 1, color, false);
                                y += bshift;
                        }

                        rp2040_oled_flush(oled);
                }
                return true;
        }

        while(1) {
                rp2040_oled_set_pixel(oled, x0, y0, color, false);
                if (x0 == x1 && y0 == y1)
                        break;

                err2 = 2 * err;
                if (err2 >= dy) {
                        if (x0 == x1)
                                break;
                        err = err + dy;
                        x0 = x0 + sx;
                }

                if (err2 <= dx) {
                        if (y0 == y1)
                                break;
                        err = err + dx;
                        y0 = y0 + sy;
                }
                
        }
        rp2040_oled_flush(oled);
        return true;
}

bool rp2040_oled_clear(rp2040_oled_t *oled)
{
        return rp2040_oled_fill(oled, 0x00);
}

bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, uint8_t *sprite, uint8_t x,
                             uint8_t y, uint8_t width, uint8_t height, rp2040_oled_color_t color)
{
        bool ret = true;
        uint8_t *buf;
        size_t buf_size;
        uint8_t orig_width = width;
        uint8_t *final_sprite = sprite;
        uint8_t final_pages = height / 8;
        uint8_t *osprite = NULL;
        size_t osprite_size = width * (height + 8) / 8;

        if (x + width >= oled->width)
                width = oled->width - x;

        if (y + height >= oled->height)
                height = oled->height - y;

        if (y % 8 != 0) {
                uint8_t yoffset = y % 8;
                osprite = malloc(osprite_size);
                memset(osprite, 0x00, osprite_size);
                for (uint8_t cur_page = 0; cur_page < height / 8; cur_page++) {
                        for (uint8_t cur_x = 0; cur_x < width; cur_x++) {
                                if (cur_page > 0)
                                        osprite[cur_x + cur_page * width] |= sprite[cur_x + (cur_page - 1) * width] << (8 - yoffset);

                                osprite[cur_x + cur_page * width] |= sprite[cur_x + cur_page * width] >> yoffset;
                        }
                }
                final_sprite = osprite;
                final_pages++;
        }

        buf_size = width;
        buf = rp2040_oled_alloc_data_buf(buf_size);

        for (uint8_t cur_page = 0; cur_page < final_pages; cur_page++) {
                if (!rp2040_oled_set_position(oled, x, cur_page * 8)) {
                        ret = false;
                        goto out;
                }
                memcpy(buf, final_sprite + (cur_page * orig_width), width);
                if (!rp2040_oled_write_gdram(oled, buf, buf_size, color, true)) {
                        ret = false;
                        goto out;
                }
        }

out:
        rp2040_oled_free_data_buf(buf);
        if (osprite)
                free(osprite);
        return ret;
}

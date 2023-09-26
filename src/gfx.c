/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "font.h"

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

static bool rp2040_oled_set_position(rp2040_oled_t *oled, uint8_t x, uint8_t y, bool render)
{
        uint8_t buf[4];

        y /= PAGE_BITS;

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

        oled->cursor.x = x;
        oled->cursor.y = y;

        if (!render)
                return true;

        buf[0] = 0x00;
        buf[1] = OLED_CMD_SET_PAGE_ADDR | y;
        buf[2] = OLED_CMD_SET_LC_ADDR | (x & 0x0f);
        buf[3] = OLED_CMD_SET_HC_ADDR | (x >> 4);

        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_render_gdram(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                                     size_t gdram_offset, uint8_t size)
{
        uint8_t *buf = NULL;

        if (oled->use_doublebuf)
                memcpy(oled->gdram + gdram_offset, oled->dirty_buf + gdram_offset,
                       size);

        buf = rp2040_oled_alloc_data_buf(size);
        memcpy(buf, oled->gdram + gdram_offset, size);

        if (!rp2040_oled_set_position(oled, x, y * PAGE_BITS, true)) {
                return false;
        }
        if (rp2040_i2c_write(oled, buf - 1, size + 1) != size + 1) {
                rp2040_oled_free_data_buf(buf);
                return false;
        }

        rp2040_oled_free_data_buf(buf);
        return true;
}

bool rp2040_oled_flush(rp2040_oled_t *oled)
{
        if (!oled->is_dirty)
                return true;

        for (uint8_t y = 0; y < oled->height / PAGE_BITS; y++) {
                uint8_t xstart = 0;
                uint8_t width = 0;

                if (oled->use_doublebuf) {
                        for (uint8_t x = 0; x < oled->width; x++) {
                                if (*(oled->gdram + (y * oled->width + x)) != *(oled->dirty_buf + (y * oled->width + x))) {
                                        if (width == 0)
                                                xstart = x;
                                        width++;
                                } else {
                                        if (width != 0) {
                                                size_t gdram_offset = xstart + (y * oled->width);
                                                rp2040_oled_render_gdram(oled, xstart, y, gdram_offset, width);

                                                width = 0;
                                        }
                                }

                        }
                } else {
                        for (uint8_t xpage = 0; xpage < oled->width / 8; xpage++) {
                                uint8_t page = oled->dirty_buf[y * (oled->width / 8) + xpage];
                                if (page) {
                                        for (uint8_t dx = 0; dx < 8; dx++) {
                                                if (page & 1 << dx) {
                                                        if (width == 0) {
                                                                xstart = xpage * 8 + dx;
                                                        }
                                                        width++;
                                                } else {
                                                        if (width != 0) {
                                                                size_t gdram_offset = xstart + (y * oled->width);
                                                                rp2040_oled_render_gdram(oled, xstart, y, gdram_offset, width);

                                                                width = 0;
                                                        }
                                                }
                                        }
                                }
                        }
                }

                if (width != 0) {
                        size_t gdram_offset = xstart + (y * oled->width);
                        rp2040_oled_render_gdram(oled, xstart, y, gdram_offset, width);

                        width = 0;
                }
        }

        oled->is_dirty = false;

        if (!oled->use_doublebuf) {
                memset(oled->dirty_buf, 0x00, oled->dirty_buf_size);
        }

        oled->cursor.x = 0;
        oled->cursor.y = 0;

        return true;
}

static bool rp2040_oled_write_gdram(rp2040_oled_t *oled, uint8_t *buf, size_t size,
                                    rp2040_oled_color_t color, bool render)
{
        size_t gdram_offset = oled->cursor.x + (oled->cursor.y * oled->width);
        uint8_t *gdram = oled->use_doublebuf ? oled->dirty_buf : oled->gdram;

        if (oled->cursor.x + size > oled->width)
                return false;

        if (color != OLED_COLOR_FULL_BYTE) {
                for (size_t i = 0; i < size; i++) {
                        if (color == OLED_COLOR_WHITE)
                                buf[i] = buf[i] | *(gdram + gdram_offset + i);
                        else if (color == OLED_COLOR_BLACK)
                                buf[i] = buf[i] & *(gdram + gdram_offset + i);
                }
        }
        memcpy(gdram + gdram_offset, buf, size);

        if (!render) {
                if (!oled->use_doublebuf)
                        for (uint8_t x = oled->cursor.x; x < oled->cursor.x + size; x++)
                                oled->dirty_buf[oled->cursor.y * (oled->width / 8) + x / 8] |= 1 << x % 8;

                oled->is_dirty = true;
        } else if (oled->use_doublebuf) {
                oled->is_dirty = true;
        }

        oled->cursor.x += size;

        if (!render)
                return true;

        return oled->use_doublebuf ? rp2040_oled_flush(oled) : rp2040_i2c_write(oled, buf - 1, size + 1) == size + 1;
}

static bool rp2040_oled_fill(rp2040_oled_t *oled, uint8_t fill_byte, bool render)
{
        uint8_t *fill_buf;
        size_t fill_buf_size = oled->width;

        fill_buf = rp2040_oled_alloc_data_buf(fill_buf_size);
        memset(fill_buf, fill_byte, fill_buf_size);

        for (uint8_t y = 0; y < oled->height; y += PAGE_BITS) {
                if (!rp2040_oled_set_position(oled, 0, y, false)) {
                        rp2040_oled_free_data_buf(fill_buf);
                        return false;
                }

                if (!rp2040_oled_write_gdram(oled, fill_buf, fill_buf_size, OLED_COLOR_FULL_BYTE, false)) {
                        rp2040_oled_free_data_buf(fill_buf);
                        return false;
                }
        }

        if (render)
                rp2040_oled_flush(oled);

        rp2040_oled_free_data_buf(fill_buf);
        return true;
}

bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y, char *msg,
                              size_t len, bool render)
{
        uint8_t *buf;
        size_t buf_size = len * 6;
        size_t i = 0;
        if (x >= oled->width || y >= oled->height)
                return false;

        if (!rp2040_oled_set_position(oled, x, y, render))
                return false;

        buf = rp2040_oled_alloc_data_buf(buf_size);

        for (i = 0; i < len; i++) {
                uint8_t font_index = msg[i] - 32;

                buf[i * 6] = 0x00;
                memcpy(buf + (1 + (i * 6)), font_6x8 + (font_index * 5), 5);
        }

        if (!rp2040_oled_write_gdram(oled, buf, buf_size, OLED_COLOR_WHITE, render)) {
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
        uint8_t page = y / PAGE_BITS;
        uint8_t page_offset = y % PAGE_BITS;
        uint8_t bit_mask = 1 << page_offset;
        uint8_t buf[2] = {OLED_CB_DATA_BIT, 0x00};
        uint8_t *gdram = oled->use_doublebuf ? oled->dirty_buf : oled->gdram;

        if (x >= oled->width || y >= oled->height)
                return false;

        buf[1] = gdram[x + (page * oled->width)];
        if (color == OLED_COLOR_WHITE)
                buf[1] |= bit_mask;
        else if (color == OLED_COLOR_BLACK)
                buf[1] &= ~bit_mask;
        else
                return false;

        if (!rp2040_oled_set_position(oled, x, page * PAGE_BITS, render))
                return false;

        if (!rp2040_oled_write_gdram(oled, buf + 1, 1, color, render))
                ret = false;

        return ret;
}

bool rp2040_oled_draw_line(rp2040_oled_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                           rp2040_oled_color_t color, bool render)
{
        int16_t dx = abs(x1 - x0);
        int16_t dy = -abs(y1 - y0);
        int8_t sx = x0 < x1 ? 1 : -1;
        int8_t sy = y0 < y1 ? 1 : -1;
        int16_t err = dx + dy;
        int16_t err2;
        uint8_t bit_mask = (color == OLED_COLOR_WHITE) ? 0x00 : 0xff;
        uint8_t buf[2] = {OLED_CB_DATA_BIT, 0x00};

        if (x0 >= oled->width || x1 >= oled->width || y0 >= oled->height || y1 >= oled->height)
                return false;

        if (x0 == x1) {
                if (y0 / PAGE_BITS == y1 / PAGE_BITS) {
                        for (uint8_t y = y0; y != y1; y += sy) {
                                uint8_t bitshift = 1 << (sy == 1 ? y % PAGE_BITS : PAGE_BITS - y % PAGE_BITS);
                                if (color == OLED_COLOR_WHITE)
                                        bit_mask |= bitshift;
                                else
                                        bit_mask &= ~bitshift;
                        }
                        rp2040_oled_set_position(oled, x0, y0, render);
                        buf[1] = bit_mask;
                        rp2040_oled_write_gdram(oled, buf + 1, 1, color, render);
                } else {
                        uint8_t tshift, bshift, y;
                        if (y0 > y1) {
                                y = y1;
                                y1 = y0;
                                y0 = y;
                        }

                        tshift = PAGE_BITS - y0 % PAGE_BITS;
                        bshift = y1 % PAGE_BITS;
                        y = y0;

                        if (tshift) {
                                bit_mask = (color == OLED_COLOR_WHITE) ? 0x00 : 0xff;
                                for (uint8_t i = 0; i < tshift; i++) {
                                        uint8_t bitshift = 1 << ((PAGE_BITS - 1) - i);
                                        if (color == OLED_COLOR_WHITE)
                                                bit_mask |= bitshift;
                                        else
                                                bit_mask &= ~bitshift;
                                }
                                rp2040_oled_set_position(oled, x0, y, false);
                                buf[1] = bit_mask;
                                rp2040_oled_write_gdram(oled, buf + 1, 1, color, false);
                                y += tshift;
                        }

                        buf[1] = (color == OLED_COLOR_WHITE) ? 0xff : 0x00;
                        while (y / PAGE_BITS < y1 / PAGE_BITS) {
                                rp2040_oled_set_position(oled, x0, y, false);
                                rp2040_oled_write_gdram(oled, buf + 1, 1, color, false);
                                y += PAGE_BITS;
                        }

                        if (bshift) {
                                bit_mask = (color == OLED_COLOR_WHITE) ? 0x00 : 0xff;
                                for (uint8_t i = 0; i < bshift; i++) {
                                        uint8_t bitshift = 1 << i;
                                        if (color == OLED_COLOR_WHITE)
                                                bit_mask |= bitshift;
                                        else
                                                bit_mask &= ~bitshift;
                                }
                                rp2040_oled_set_position(oled, x0, y, false);
                                buf[1] = bit_mask;
                                rp2040_oled_write_gdram(oled, buf + 1, 1, color, false);
                                y += bshift;
                        }

                        if (render)
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

        if (render)
                rp2040_oled_flush(oled);
        return true;
}

bool rp2040_oled_draw_rectangle(rp2040_oled_t *oled, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                rp2040_oled_color_t color, bool fill, bool render)
{
        uint8_t tmp = 0;

        if (x0 > x1) {
                tmp = x0;
                x0 = x1;
                x1 = tmp;
        }

        if (y0 > y1) {
                tmp = y0;
                y0 = y1;
                y1 = tmp;
        }

        if (!fill) {
                rp2040_oled_draw_line(oled, x0, y0, x0, y1, color, false);
                rp2040_oled_draw_line(oled, x0, y0, x1, y0, color, false);
                rp2040_oled_draw_line(oled, x1, y0, x1, y1, color, false);
                rp2040_oled_draw_line(oled, x0, y1, x1, y1, color, false);
        } else {
                for (uint8_t x = x0; x <= x1; x++) {
                        rp2040_oled_draw_line(oled, x, y0, x, y1, color, false);
                }
        }

        if (render)
                rp2040_oled_flush(oled);

        return true;
}

bool rp2040_oled_clear(rp2040_oled_t *oled)
{
        return rp2040_oled_fill(oled, 0x00, true);
}

bool rp2040_oled_clear_gdram(rp2040_oled_t *oled)
{
        return rp2040_oled_fill(oled, 0x00, false);
}

bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, const uint8_t *sprite, int16_t x,
                             int16_t y, uint8_t width, uint8_t height,
                             rp2040_oled_color_t color, bool render)
{
        bool ret = true;
        uint8_t *buf;
        size_t buf_size;
        uint8_t orig_width = width;
        const uint8_t *final_sprite = sprite;
        uint8_t orig_pages, final_pages;
        uint8_t *osprite = NULL;
        size_t osprite_size;
        uint8_t xshift = 0, yshift = 0;

        if (x + width < 0 || y + height < 0)
                return false;

        if (x < 0) {
                width += x;
                xshift = -x;
                x = 0;
        }

        if (y < 0) {
                height += y;
                yshift = -y;
                y = 0;
        }

        if (x + width > oled->width)
                width = oled->width - x;

        if (y + height > oled->height)
                height = oled->height - y;

        orig_pages = (height + PAGE_BITS - 1) / PAGE_BITS;
        final_pages = orig_pages;

        if ((y % PAGE_BITS != 0) || yshift != 0) {
                uint8_t yoffset = (y + yshift) % PAGE_BITS;
                uint8_t pshift = yshift / PAGE_BITS;
                final_pages = (height + yoffset + PAGE_BITS - 1) / PAGE_BITS;
                osprite_size = orig_width * final_pages;
                osprite = malloc(osprite_size);
                memset(osprite, 0x00, osprite_size);
                final_sprite = osprite;
                for (uint8_t cur_page = 0; cur_page < final_pages; cur_page++) {
                        for (uint8_t cur_x = 0; cur_x < orig_width; cur_x++) {
                                if (y != 0) {
                                        if ((cur_page) > 0)
                                                osprite[cur_x + cur_page * orig_width] |= sprite[cur_x + (cur_page - 1) * orig_width] >> (PAGE_BITS - yoffset);

                                        if (cur_page < orig_pages)
                                                osprite[cur_x + cur_page * orig_width] |= sprite[cur_x + cur_page * orig_width] << yoffset;
                                } else {
                                        osprite[cur_x + cur_page * orig_width] |= sprite[cur_x + (pshift + cur_page) * orig_width] >> yoffset;

                                        if (cur_page < orig_pages)
                                                osprite[cur_x + cur_page * orig_width] |= sprite[cur_x + (pshift + cur_page + 1) * orig_width] << (PAGE_BITS - yoffset);
                                }
                        }
                }
        }

        buf_size = width;
        buf = rp2040_oled_alloc_data_buf(buf_size);

        for (uint8_t cur_page = 0; cur_page < final_pages; cur_page++) {
                if (!rp2040_oled_set_position(oled, x, y + cur_page * PAGE_BITS, false)) {
                        ret = false;
                        goto out;
                }
                memcpy(buf, final_sprite + xshift + (cur_page * orig_width), width);
                //memcpy(buf, final_sprite + xshift + ((yshift + cur_page) * orig_width), width);
                if (cur_page == final_pages - 1 && (y + height) % PAGE_BITS) {
                        uint8_t mask = 0x00;
                        for (uint8_t i = 0; i < (y + height) % PAGE_BITS; i++) {
                                mask |= 1 << i;
                        }

                        for (uint8_t i = 0; i < width; i++) {
                                *(buf + i) &= mask;
                        }
                }
                if (!rp2040_oled_write_gdram(oled, buf, buf_size, color, false)) {
                        ret = false;
                        goto out;
                }
        }

        if (render)
                rp2040_oled_flush(oled);

out:
        rp2040_oled_free_data_buf(buf);
        if (osprite)
                free(osprite);
        return ret;
}

bool rp2040_oled_draw_sprite_pitched(rp2040_oled_t *oled, uint8_t *sprite, int16_t x,
                                     int16_t y, uint8_t width, uint8_t height, uint8_t pitch,
                                     rp2040_oled_color_t color, bool render)
{
        bool ret = true;
        uint8_t *mem_sprite = NULL;
        size_t mem_sprite_size = width * ((height + PAGE_BITS - 1) / PAGE_BITS);

        mem_sprite = malloc(mem_sprite_size);
        memset(mem_sprite, 0x00, mem_sprite_size);

        for (uint8_t cy = 0; cy < height; cy++) {
                uint8_t cpage = cy / PAGE_BITS;
                uint8_t sx = 0;
                uint8_t *src = sprite + (cy * pitch);
                uint8_t *dst = mem_sprite + (cpage * width);
                uint8_t src_mask = 0x80;
                uint8_t dst_mask = 1 << (cy & (PAGE_BITS - 1));
                for (uint8_t dx = 0; dx < width; dx++) {
                        if (*(src + sx) & src_mask)
                                *(dst + dx) |= dst_mask;
                        src_mask >>= 1;
                        if (!src_mask) {
                                src_mask = 0x80;
                                sx++;
                        }
                }
        }

        ret = rp2040_oled_draw_sprite(oled, mem_sprite, x, y, width, height, color, render);
        free(mem_sprite);
        return ret;
}

bool rp2040_oled_draw_circle(rp2040_oled_t *oled, int16_t x, int16_t y, uint8_t r,
                             rp2040_oled_color_t color, bool fill, bool render)
{
        uint8_t dx = r, dy = 0;
        int16_t t1 = r / 16;
        int16_t t2;

        while (dx >= dy) {
                if (fill) {
                        rp2040_oled_draw_line(oled, x + dx, y + dy, x + dx, y - dy, color, false);
                        rp2040_oled_draw_line(oled, x - dx, y + dy, x - dx, y - dy, color, false);
                        rp2040_oled_draw_line(oled, x + dy, y + dx, x + dy, y - dx, color, false);
                        rp2040_oled_draw_line(oled, x - dy, y + dx, x - dy, y - dx, color, false);
                } else {
                        rp2040_oled_set_pixel(oled, x + dx, y + dy, color, false);
                        rp2040_oled_set_pixel(oled, x + dx, y - dy, color, false);
                        rp2040_oled_set_pixel(oled, x - dx, y + dy, color, false);
                        rp2040_oled_set_pixel(oled, x - dx, y - dy, color, false);
                        rp2040_oled_set_pixel(oled, x + dy, y + dx, color, false);
                        rp2040_oled_set_pixel(oled, x + dy, y - dx, color, false);
                        rp2040_oled_set_pixel(oled, x - dy, y + dx, color, false);
                        rp2040_oled_set_pixel(oled, x - dy, y - dx, color, false);
                }

                dy++;
                t1 += dy;
                t2 = t1 - dx;
                if (t2 >= 0) {
                        t1 = t2;
                        dx--;
                }
        }

        if (render)
                rp2040_oled_flush(oled);

        return true;
}

bool rp2040_oled_draw_ellipse(rp2040_oled_t *oled, int16_t x, int16_t y, uint8_t rx,
                              uint8_t ry, rp2040_oled_color_t color, bool fill,
                              bool render)
{
        float dx, dy, d1, d2;
        int16_t sx, sy;
        int32_t rx2, ry2;

        if (rx == ry)
                return rp2040_oled_draw_circle(oled, x, y, rx, color, fill, render);

        sx = 0;
        sy = ry;

        rx2 = rx * rx;
        ry2 = ry * ry;

        d1 = ry2 - (rx2 * ry) + (0.25 * rx2);
        dx = 2 * ry2 * sx;
        dy = 2 * rx2 * sy;

        while (dx < dy) {
                if (fill) {
                        rp2040_oled_draw_line(oled, x + sx, y - sy, x + sx, y + sy, color, false);
                        rp2040_oled_draw_line(oled, x - sx, y - sy, x - sx, y + sy, color, false);
                } else {
                        rp2040_oled_set_pixel(oled, x + sx, y - sy, color, false);
                        rp2040_oled_set_pixel(oled, x + sx, y + sy, color, false);
                        rp2040_oled_set_pixel(oled, x - sx, y - sy, color, false);
                        rp2040_oled_set_pixel(oled, x - sx, y + sy, color, false);
                }

                if (d1 < 0) {
                        sx++;
                        dx = dx + (2 * ry2);
                        d1 = d1 + dx + ry2;
                } else {
                        sx++;
                        sy--;
                        dx = dx + (2 * ry2);
                        dy = dy - (2 * rx2);
                        d1 = d1 + dx - dy + ry2;
                }
        }

        d2 = (ry2 * ((sx + 0.5) * (sx + 0.5))) + (rx2 * ((sy - 1) * (sy - 1))) - (rx2 * ry2);
        while (sy >= 0) {
                if (fill) {
                        rp2040_oled_draw_line(oled, x + sx, y - sy, x + sx, y + sy, color, false);
                        rp2040_oled_draw_line(oled, x - sx, y - sy, x - sx, y + sy, color, false);
                } else {
                        rp2040_oled_set_pixel(oled, x + sx, y - sy, color, false);
                        rp2040_oled_set_pixel(oled, x + sx, y + sy, color, false);
                        rp2040_oled_set_pixel(oled, x - sx, y - sy, color, false);
                        rp2040_oled_set_pixel(oled, x - sx, y + sy, color, false);
                }

                if (d2 > 0) {
                        sy--;
                        dy = dy - (2 * rx2);
                        d2 = d2 + rx2 - dy;
                } else {
                        sy--;
                        sx++;
                        dx = dx + (2 * ry2);
                        dy = dy - (2 * rx2);
                        d2 = d2 + dx - dy + rx2;
                }
        }

        if (render)
                rp2040_oled_flush(oled);

        return true;
}

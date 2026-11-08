/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
/**
 * @file gfx.c
 * @brief Framebuffer drawing and flushing.
 */
#include "gfx.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "rp2040-oled-types.h"
#include "spi.h"

/**
 * @brief Writes bytes into the framebuffer at the cursor.
 *
 * Every drawing function ends up here. Combines @p buf with the framebuffer
 * according to @p color, then either marks those columns dirty or, with
 * @p render, sends them. Advances the cursor by @p size columns.
 *
 * @param oled Display to draw on.
 * @param[in,out] buf Bytes to write, one per column. Overwritten with the
 *                    combined bytes.
 * @param size Number of bytes in @p buf.
 * @param color How @p buf combines with the framebuffer.
 * @param render true to send the change right away. Without doublebuf, the
 *               screen's write position must already match the cursor (see
 *               rp2040_oled_set_position()).
 * @return false if the write would run past the right edge or the send fails.
 */
static bool rp2040_oled_write_gdram(rp2040_oled_t *oled, uint8_t *buf,
                                    size_t size, rp2040_oled_color_t color,
                                    bool render) {
  size_t gdram_offset = oled->cursor.x + (oled->cursor.y * oled->width);
  // doublebuf draws into dirty_buf, otherwise straight into gdram
  uint8_t *gdram = oled->use_doublebuf ? oled->dirty_buf : oled->gdram;

  // writes can't wrap onto the next page
  if (oled->cursor.x + size > oled->width)
    return false;

  // WHITE ORs, BLACK ANDs, FULL_BYTE overwrites
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
    // doublebuf finds changes by comparing buffers, so only this mode needs
    // dirty bits
    if (!oled->use_doublebuf)
      for (uint8_t x = oled->cursor.x; x < oled->cursor.x + size; x++)
        oled->dirty_buf[oled->cursor.y * (oled->width / 8) + x / 8] |= 1
                                                                       << x % 8;

    oled->is_dirty = true;
  } else if (oled->use_doublebuf) {
    oled->is_dirty = true;
  }

  oled->cursor.x += size;

  if (!render)
    return true;

  // a direct send relies on set_position having moved the screen's write
  // position
  return oled->use_doublebuf ? rp2040_oled_flush(oled)
                             : rp2040_spi_write(oled, buf, size, false) == size;
}

/**
 * @brief Allocates a scratch buffer for pixel data.
 *
 * @param size Bytes to allocate.
 * @return The buffer. Not checked for NULL.
 */
static uint8_t *rp2040_oled_alloc_data_buf(size_t size) {
  return (uint8_t *)malloc(size);
}

/**
 * @brief Frees a buffer from rp2040_oled_alloc_data_buf().
 *
 * @param buf Buffer to free.
 */
static void rp2040_oled_free_data_buf(uint8_t *buf) { free(buf); }

/**
 * @brief Moves the cursor to column @p x and the page holding pixel row @p y.
 *
 * @param oled Display to move on.
 * @param x Column.
 * @param y Pixel row. Rounds down to its page.
 * @param render true to also move the screen's write position to match.
 * @return true on success, false if the command couldn't be sent.
 */
static bool rp2040_oled_set_position(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                                     bool render) {
  uint8_t buf[3];

  y /= PAGE_BITS;

  oled->cursor.x = x;
  oled->cursor.y = y;

  if (!render)
    return true;

  // the screen shows columns 2 to 129 of the sh1106's 132 column ram, so screen
  // column = x + 2. x stays as is because the cursor and framebuffer start at 0
  uint8_t column = x + 2;

  // page address, then the column split into two nibbles
  buf[0] = OLED_CMD_SET_PAGE_ADDR | y;
  buf[1] = OLED_CMD_SET_LC_ADDR | (column & 0x0f);
  buf[2] = OLED_CMD_SET_HC_ADDR | (column >> 4);

  return rp2040_spi_write(oled, buf, sizeof(buf), true) == sizeof(buf);
}

/**
 * @brief Fills the framebuffer with one byte, one page at a time.
 *
 * @param oled Display to fill.
 * @param fill_byte Byte written to every column of every page.
 * @param render true to flush right after.
 * @return false if a write fails.
 */
static bool rp2040_oled_fill(rp2040_oled_t *oled, uint8_t fill_byte,
                             bool render) {
  uint8_t *fill_buf;
  size_t fill_buf_size = oled->width;

  fill_buf = rp2040_oled_alloc_data_buf(fill_buf_size);
  memset(fill_buf, fill_byte, fill_buf_size);

  for (uint8_t y = 0; y < oled->height; y += PAGE_BITS) {
    if (!rp2040_oled_set_position(oled, 0, y, false)) {
      rp2040_oled_free_data_buf(fill_buf);
      return false;
    }

    if (!rp2040_oled_write_gdram(oled, fill_buf, fill_buf_size,
                                 OLED_COLOR_FULL_BYTE, false)) {
      rp2040_oled_free_data_buf(fill_buf);
      return false;
    }
  }

  if (render)
    rp2040_oled_flush(oled);

  rp2040_oled_free_data_buf(fill_buf);
  return true;
}

bool rp2040_oled_flush(rp2040_oled_t *oled) {
  if (!oled->is_dirty)
    return true;

  for (uint8_t y = 0; y < oled->height / PAGE_BITS; y++) {
    // start and length of the current run of changed columns
    uint8_t xstart = 0;
    uint8_t width = 0;

    if (oled->use_doublebuf) {
      // doublebuf: a column changed if dirty_buf and gdram differ
      for (uint8_t x = 0; x < oled->width; x++) {
        if (*(oled->gdram + (y * oled->width + x)) !=
            *(oled->dirty_buf + (y * oled->width + x))) {
          if (width == 0)
            xstart = x;
          width++;
        } else {
          // run ended, send it
          if (width != 0) {
            size_t gdram_offset = xstart + (y * oled->width);
            rp2040_oled_render_gdram(oled, xstart, y, gdram_offset, width);

            width = 0;
          }
        }
      }
    } else {
      // no doublebuf: one dirty bit per column
      for (uint8_t x = 0; x < oled->width; x++) {
        uint8_t dirty_byte = oled->dirty_buf[y * (oled->width / 8) + x / 8];
        if (dirty_byte & (1 << (x % 8))) {
          if (width == 0)
            xstart = x;
          width++;
        } else {
          // run ended, send it
          if (width != 0) {
            size_t gdram_offset = xstart + (y * oled->width);
            rp2040_oled_render_gdram(oled, xstart, y, gdram_offset, width);

            width = 0;
          }
        }
      }
    }

    // run reached the right edge
    if (width != 0) {
      size_t gdram_offset = xstart + (y * oled->width);
      rp2040_oled_render_gdram(oled, xstart, y, gdram_offset, width);

      width = 0;
    }
  }

  oled->is_dirty = false;

  // start the next flush with a clean bitmap. with doublebuf, gdram already
  // matches
  if (!oled->use_doublebuf) {
    memset(oled->dirty_buf, 0x00, oled->dirty_buf_size);
  }

  oled->cursor.x = 0;
  oled->cursor.y = 0;

  return true;
}

bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                              char *msg, size_t len, bool render) {
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

// reads the byte holding the pixel, changes its bit, and writes it back
bool rp2040_oled_set_pixel(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                           rp2040_oled_color_t color, bool render) {
  bool ret = true;
  uint8_t page = y / PAGE_BITS;
  uint8_t page_offset = y % PAGE_BITS;
  uint8_t bit_mask = 1 << page_offset;
  uint8_t buf[1] = {0x00};
  uint8_t *gdram = oled->use_doublebuf ? oled->dirty_buf : oled->gdram;

  if (x >= oled->width || y >= oled->height)
    return false;

  buf[0] = gdram[x + (page * oled->width)];
  if (color == OLED_COLOR_WHITE)
    buf[0] |= bit_mask;
  else if (color == OLED_COLOR_BLACK)
    buf[0] &= ~bit_mask;
  else
    return false;

  if (!rp2040_oled_set_position(oled, x, page * PAGE_BITS, render))
    return false;

  if (!rp2040_oled_write_gdram(oled, buf, 1, color, render))
    ret = false;

  return ret;
}

bool rp2040_oled_draw_line(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                           uint8_t x1, uint8_t y1, rp2040_oled_color_t color,
                           bool render) {
  // bresenham setup: distance and step direction on each axis
  int16_t dx = abs(x1 - x0);
  int16_t dy = -abs(y1 - y0);
  int8_t sx = x0 < x1 ? 1 : -1;
  int8_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  int16_t err2;
  // white starts off and sets bits, black starts on and clears them, to match
  // write_gdram
  uint8_t bit_mask = (color == OLED_COLOR_WHITE) ? 0x00 : 0xff;
  uint8_t buf[1] = {0x00};

  if (x0 >= oled->width || x1 >= oled->width || y0 >= oled->height ||
      y1 >= oled->height)
    return false;

  if (x0 == x1) {
    // inside one page: a single byte
    if (y0 / PAGE_BITS == y1 / PAGE_BITS) {
      for (uint8_t y = y0; y != y1; y += sy) {
        uint8_t bitshift =
            1 << (sy == 1 ? y % PAGE_BITS : PAGE_BITS - y % PAGE_BITS);
        if (color == OLED_COLOR_WHITE)
          bit_mask |= bitshift;
        else
          bit_mask &= ~bitshift;
      }
      rp2040_oled_set_position(oled, x0, y0, render);
      buf[0] = bit_mask;
      rp2040_oled_write_gdram(oled, buf, 1, color, render);
    } else {
      // across pages: partial top byte, full middle bytes, partial bottom byte
      uint8_t tshift, bshift, y;
      // always draw top to bottom
      if (y0 > y1) {
        y = y1;
        y1 = y0;
        y0 = y;
      }

      // tshift = pixels from y0 to its page's bottom. bshift = pixels above y1
      // in its page
      tshift = PAGE_BITS - y0 % PAGE_BITS;
      bshift = y1 % PAGE_BITS;
      y = y0;

      // top page
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
        buf[0] = bit_mask;
        rp2040_oled_write_gdram(oled, buf, 1, color, false);
        y += tshift;
      }

      // middle pages: every pixel in the byte
      buf[0] = (color == OLED_COLOR_WHITE) ? 0xff : 0x00;
      while (y / PAGE_BITS < y1 / PAGE_BITS) {
        rp2040_oled_set_position(oled, x0, y, false);
        rp2040_oled_write_gdram(oled, buf, 1, color, false);
        y += PAGE_BITS;
      }

      // bottom page
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
        buf[0] = bit_mask;
        rp2040_oled_write_gdram(oled, buf, 1, color, false);
        y += bshift;
      }

      if (render)
        rp2040_oled_flush(oled);
    }
    return true;
  }

  // bresenham: err decides whether the next step moves in x, y, or both
  while (1) {
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

bool rp2040_oled_draw_rectangle(rp2040_oled_t *oled, uint8_t x0, uint8_t y0,
                                uint8_t x1, uint8_t y1,
                                rp2040_oled_color_t color, bool fill,
                                bool render) {
  uint8_t tmp = 0;

  // make (x0, y0) the top left corner
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

// clips the sprite to the screen, shifts it into a temporary copy if it isn't
// page aligned, then writes it one page at a time
bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, const uint8_t *sprite,
                             int16_t x, int16_t y, uint8_t width,
                             uint8_t height, rp2040_oled_color_t color,
                             bool render) {
  bool ret = true;
  uint8_t *buf;
  size_t buf_size;
  // full width, for stepping between sprite pages after clipping
  uint8_t orig_width = width;
  const uint8_t *final_sprite = sprite;
  uint8_t orig_pages, final_pages;
  uint8_t *osprite = NULL;
  size_t osprite_size;
  // columns and rows clipped off the left and top
  uint8_t xshift = 0, yshift = 0;

  // fully off screen
  if (x + width < 0 || y + height < 0)
    return false;

  // clip left and top
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

  // clip right and bottom
  if (x + width > oled->width)
    width = oled->width - x;

  if (y + height > oled->height)
    height = oled->height - y;

  // pages needed, rounded up
  orig_pages = (height + PAGE_BITS - 1) / PAGE_BITS;
  final_pages = orig_pages;

  // not page aligned, or clipped at the top: build a shifted copy
  if ((y % PAGE_BITS != 0) || yshift != 0) {
    // yoffset = bits to shift. pshift = whole pages clipped off the top
    uint8_t yoffset = (y + yshift) % PAGE_BITS;
    uint8_t pshift = yshift / PAGE_BITS;
    // shifting down can spill into one more page
    final_pages = (height + yoffset + PAGE_BITS - 1) / PAGE_BITS;
    osprite_size = orig_width * final_pages;
    osprite = malloc(osprite_size);
    memset(osprite, 0x00, osprite_size);
    final_sprite = osprite;
    for (uint8_t cur_page = 0; cur_page < final_pages; cur_page++) {
      for (uint8_t cur_x = 0; cur_x < orig_width; cur_x++) {
        if (y != 0) {
          // starts partway down a page: shift down, mixing in bits from the
          // page above
          if ((cur_page) > 0)
            osprite[cur_x + cur_page * orig_width] |=
                sprite[cur_x + (cur_page - 1) * orig_width] >>
                (PAGE_BITS - yoffset);

          if (cur_page < orig_pages)
            osprite[cur_x + cur_page * orig_width] |=
                sprite[cur_x + cur_page * orig_width] << yoffset;
        } else {
          // clipped at the top: skip clipped pages and pull bits up from the
          // next one
          osprite[cur_x + cur_page * orig_width] |=
              sprite[cur_x + (pshift + cur_page) * orig_width] >> yoffset;

          if (cur_page < orig_pages)
            osprite[cur_x + cur_page * orig_width] |=
                sprite[cur_x + (pshift + cur_page + 1) * orig_width]
                << (PAGE_BITS - yoffset);
        }
      }
    }
  }

  // one page row of the clipped sprite at a time
  buf_size = width;
  buf = rp2040_oled_alloc_data_buf(buf_size);

  for (uint8_t cur_page = 0; cur_page < final_pages; cur_page++) {
    if (!rp2040_oled_set_position(oled, x, y + cur_page * PAGE_BITS, false)) {
      ret = false;
      goto out;
    }
    // skip columns clipped off the left
    memcpy(buf, final_sprite + xshift + (cur_page * orig_width), width);
    // last page: mask off rows below the sprite
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

// cleanup for success and error paths
out:
  rp2040_oled_free_data_buf(buf);
  if (osprite)
    free(osprite);
  return ret;
}

bool rp2040_oled_draw_sprite_pitched(rp2040_oled_t *oled, uint8_t *sprite,
                                     int16_t x, int16_t y, uint8_t width,
                                     uint8_t height, uint8_t pitch,
                                     rp2040_oled_color_t color, bool render) {
  bool ret = true;
  uint8_t *mem_sprite = NULL;
  size_t mem_sprite_size = width * ((height + PAGE_BITS - 1) / PAGE_BITS);

  mem_sprite = malloc(mem_sprite_size);
  memset(mem_sprite, 0x00, mem_sprite_size);

  for (uint8_t cy = 0; cy < height; cy++) {
    // src = this input row. dst = the page row it lands in. dst_mask = its bit
    // there
    uint8_t cpage = cy / PAGE_BITS;
    uint8_t sx = 0;
    uint8_t *src = sprite + (cy * pitch);
    uint8_t *dst = mem_sprite + (cpage * width);
    uint8_t src_mask = 0x80;
    uint8_t dst_mask = 1 << (cy & (PAGE_BITS - 1));
    for (uint8_t dx = 0; dx < width; dx++) {
      if (*(src + sx) & src_mask)
        *(dst + dx) |= dst_mask;
      // next input bit, and the next input byte after 8
      src_mask >>= 1;
      if (!src_mask) {
        src_mask = 0x80;
        sx++;
      }
    }
  }

  ret = rp2040_oled_draw_sprite(oled, mem_sprite, x, y, width, height, color,
                                render);
  free(mem_sprite);
  return ret;
}

// midpoint circle: walks one eighth of the circle and mirrors each point.
// filled circles draw vertical lines between mirrored points
bool rp2040_oled_draw_circle(rp2040_oled_t *oled, int16_t x, int16_t y,
                             uint8_t r, rp2040_oled_color_t color, bool fill,
                             bool render) {
  // start at (r, 0) and stop at the 45 degree mark
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

    // always step one row, and step one column once the error passes dx
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

// midpoint ellipse: walks one quarter and mirrors each point. region 1 is the
// flatter part near the top, region 2 the steeper part near the side
bool rp2040_oled_draw_ellipse(rp2040_oled_t *oled, int16_t x, int16_t y,
                              uint8_t rx, uint8_t ry, rp2040_oled_color_t color,
                              bool fill, bool render) {
  float dx, dy, d1, d2;
  int16_t sx, sy;
  int32_t rx2, ry2;

  // equal radii is just a circle, which is cheaper
  if (rx == ry)
    return rp2040_oled_draw_circle(oled, x, y, rx, color, fill, render);

  sx = 0;
  sy = ry;

  rx2 = rx * rx;
  ry2 = ry * ry;

  // region 1 starting decision value and slope terms
  d1 = ry2 - (rx2 * ry) + (0.25 * rx2);
  dx = 2 * ry2 * sx;
  dy = 2 * rx2 * sy;

  // region 1: until the slope passes -1
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

    // inside: step x. outside: step x and y
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

  // region 2: from where region 1 stopped down to y = 0
  d2 = (ry2 * ((sx + 0.5) * (sx + 0.5))) + (rx2 * ((sy - 1) * (sy - 1))) -
       (rx2 * ry2);
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

    // outside: step y. inside: step y and x
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

bool rp2040_oled_clear(rp2040_oled_t *oled) {
  return rp2040_oled_fill(oled, 0x00, true);
}

bool rp2040_oled_clear_gdram(rp2040_oled_t *oled) {
  return rp2040_oled_fill(oled, 0x00, false);
}

bool rp2040_oled_render_gdram(rp2040_oled_t *oled, uint8_t x, uint8_t y,
                              size_t gdram_offset, uint8_t size) {
  uint8_t *buf = NULL;

  if (oled->use_doublebuf)
    memcpy(oled->gdram + gdram_offset, oled->dirty_buf + gdram_offset, size);

  buf = rp2040_oled_alloc_data_buf(size);
  memcpy(buf, oled->gdram + gdram_offset, size);

  // set_position takes a pixel row, not a page
  if (!rp2040_oled_set_position(oled, x, y * PAGE_BITS, true)) {
    return false;
  }
  if (rp2040_spi_write(oled, buf, size, false) != size) {
    rp2040_oled_free_data_buf(buf);
    return false;
  }

  rp2040_oled_free_data_buf(buf);
  return true;
}

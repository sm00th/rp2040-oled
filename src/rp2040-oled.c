/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
#include "rp2040-oled.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rp2040-oled-types.h"

#include "display.h"
#include "gfx.h"
#include "spi.h"

// sends every page of gdram, dirty or not
static bool rp2040_oled_force_flush(rp2040_oled_t *oled) {
  for (uint8_t y = 0; y < oled->height / PAGE_BITS; y++) {
    size_t gdram_offset = y * oled->width;
    rp2040_oled_render_gdram(oled, 0, y, gdram_offset, oled->width);
  }

  oled->is_dirty = false;

  oled->cursor.x = 0;
  oled->cursor.y = 0;

  return true;
}

// sends one command byte
static bool rp2040_oled_write_command(rp2040_oled_t *oled, uint8_t cmd) {
  uint8_t buf[] = {cmd};
  return rp2040_spi_write(oled, buf, sizeof(buf), true) == sizeof(buf);
}

// sends a command and its argument byte
static bool rp2040_oled_write_command_with_arg(rp2040_oled_t *oled, uint8_t cmd,
                                               uint8_t arg) {
  uint8_t buf[] = {cmd, arg};
  return rp2040_spi_write(oled, buf, sizeof(buf), true) == sizeof(buf);
}

// pulses reset low, then gives the controller time to come back up
static void rp2040_oled_reset(rp2040_oled_t *oled) {
  gpio_put(oled->reset_pin, GPIO_LEVEL_LOW);
  sleep_ms(50);
  gpio_put(oled->reset_pin, GPIO_LEVEL_HIGH);
  sleep_ms(10);
}

// starts the 128x64 sh1106: sends its startup commands, allocates both buffers,
// and blanks the screen
static void rp2040_oled_display_init(rp2040_oled_t *oled) {
  oled->width = 128;
  oled->height = 64;

  rp2040_spi_write(oled, oled_sh1106_init, oled_sh1106_init_len, true);

  if (oled->invert)
    rp2040_oled_write_command(oled, OLED_CMD_SET_DISPLAY_INVERSE);

  // the startup list mirrors rows and columns, so "normal" is what flips the
  // image. this is an else if, so FLIP_BOTH only flips horizontally
  if (oled->flip & FLIP_HORIZONTAL)
    rp2040_oled_write_command(oled, OLED_CMD_SET_SEGMENT_REMAP_NORMAL);
  else if (oled->flip & FLIP_VERTICAL)
    rp2040_oled_write_command(oled, OLED_CMD_SET_SCAN_DIR_NORMAL);

  // one byte holds 8 rows of one column
  oled->gdram_size = oled->width * oled->height / PAGE_BITS;
  oled->gdram = (uint8_t *)malloc(oled->gdram_size);
  memset(oled->gdram, 0x00, oled->gdram_size);

  // doublebuf: a second full framebuffer. otherwise: one dirty bit per column
  if (oled->use_doublebuf) {
    oled->dirty_buf_size = oled->gdram_size;
  } else {
    oled->dirty_buf_size = (oled->width / 8) * (oled->height / PAGE_BITS);
  }

  // dirty_buf isn't allocated yet, so this is only safe with use_doublebuf off
  rp2040_oled_force_flush(oled);

  oled->dirty_buf = malloc(oled->dirty_buf_size);
  memset(oled->dirty_buf, 0x00, oled->dirty_buf_size);
}

// sets up the pins, resets the screen, then starts it
void rp2040_oled_init(rp2040_oled_t *oled) {
  rp2040_spi_init(oled);

  if (oled->reset_pin != PIN_UNDEF) {
    rp2040_oled_reset(oled);
  }

  rp2040_oled_display_init(oled);
}

bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast) {
  return rp2040_oled_write_command_with_arg(oled, OLED_CMD_SET_CONTRAST,
                                            contrast);
}

bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled) {
  return rp2040_oled_write_command(oled, enabled ? OLED_CMD_DISPLAY_ON
                                                 : OLED_CMD_DISPLAY_OFF);
}

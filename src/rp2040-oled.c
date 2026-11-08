/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
/**
 * @file rp2040-oled.c
 * @brief Display startup, reset, and panel commands.
 */
#include "rp2040-oled.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/gpio.h"
#include "pico/time.h"

#include "rp2040-oled-types.h"

#include "display.h"
#include "gfx.h"
#include "spi.h"

/**
 * @brief Sends every page of gdram to the screen, dirty or not.
 *
 * Clears `is_dirty` and resets the cursor to (0, 0).
 *
 * @param oled Display to flush.
 * @return Always true.
 */
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

/**
 * @brief Sends one command byte.
 *
 * @param oled Display to send to.
 * @param cmd Command byte.
 * @return true if the byte was sent.
 */
static bool rp2040_oled_write_command(rp2040_oled_t *oled, uint8_t cmd) {
  uint8_t buf[] = {cmd};
  return rp2040_spi_write(oled, buf, sizeof(buf), true) == sizeof(buf);
}

/**
 * @brief Sends a command byte followed by its argument byte.
 *
 * @param oled Display to send to.
 * @param cmd Command byte.
 * @param arg Argument byte.
 * @return true if both bytes were sent.
 */
static bool rp2040_oled_write_command_with_arg(rp2040_oled_t *oled, uint8_t cmd,
                                               uint8_t arg) {
  uint8_t buf[] = {cmd, arg};
  return rp2040_spi_write(oled, buf, sizeof(buf), true) == sizeof(buf);
}

/**
 * @brief Pulses the reset pin low, then waits for the controller to come back
 * up.
 *
 * Blocks for about 60 ms.
 *
 * @param oled Display to reset. `reset_pin` must be wired up.
 */
static void rp2040_oled_reset(rp2040_oled_t *oled) {
  gpio_put(oled->reset_pin, GPIO_LEVEL_LOW);
  sleep_ms(50);
  gpio_put(oled->reset_pin, GPIO_LEVEL_HIGH);
  sleep_ms(10);
}

/**
 * @brief Starts the 128x64 SH1106.
 *
 * Sends the startup commands, applies `invert` and `flip`, allocates gdram and
 * dirty_buf, and blanks the screen.
 *
 * @param oled Display to start.
 * @note FLIP_BOTH only flips horizontally (see the comment on the flip check).
 * @warning dirty_buf is allocated after the initial flush, so this is only
 *          safe with `use_doublebuf` false.
 */
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

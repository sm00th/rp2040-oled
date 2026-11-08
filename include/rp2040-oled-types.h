/*
 * SPDX-License-Identifier: MIT
 * 2026, Artem Savkov, Ramzey Burdette
 */
/**
 * @file rp2040-oled-types.h
 * @brief Types and constants shared across the OLED driver.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"

/** @brief Means "no pin". Used for `reset_pin`. */
#define PIN_UNDEF 0xff

/** @brief GPIO output level for high. */
#define GPIO_LEVEL_HIGH 1
/** @brief GPIO output level for low. */
#define GPIO_LEVEL_LOW 0

/** @brief Display SPI clock pin. */
#define DISPLAY_SCLK 26
/** @brief Display SPI data pin. */
#define DISPLAY_MOSI 27
/** @brief Display chip select pin. */
#define DISPLAY_CHIP_SELECT 22
/** @brief Display data/command pin. */
#define DISPLAY_DC 24
/** @brief Display reset pin. */
#define DISPLAY_RESET 23
/** @brief SPI clock for the display. */
#define DISPLAY_FREQ 10 * 1000 * 1000 // 10 MHz

/**
 * @brief Rows per page.
 *
 * Display RAM is split into pages 8 pixels tall. One byte is one column of a
 * page, bit 0 on top.
 */
#define PAGE_BITS 8

/** @brief How new bytes combine with the framebuffer. */
typedef enum {
  OLED_COLOR_BLACK = 0, // AND: 0 bits turn pixels off.
  OLED_COLOR_WHITE,     // OR: 1 bits turn pixels on.
  OLED_COLOR_FULL_BYTE, // Overwrite the whole byte.
} rp2040_oled_color_t;

/**
 * @brief Controller command bytes.
 *
 * Some get ORed with a value, and some take an argument byte after them.
 */
typedef enum {
  OLED_CMD_SET_LC_ADDR = 0x00,              ///< OR in the column's low nibble.
  OLED_CMD_SET_HC_ADDR = 0x10,              ///< OR in the column's high nibble.
  OLED_CMD_SET_SSD1306_ADDR_MODE = 0x20,    ///< SSD1306 only.
  OLED_CMD_SET_ADDR_PAGE = 0x20,            ///< SH1107 only.
  OLED_CMD_SET_ADDR_VERTICAL = 0x21,        ///< SH1107 only.
  OLED_CMD_SET_DISPLAY_STARTLINE0 = 0x40,   ///< Start at line 0.
  OLED_CMD_SET_CONTRAST = 0x81,             ///< Arg: 0x00 to 0xff.
  OLED_CMD_SET_CHARGE_PUMP = 0x8d,          ///< SSD1306 only.
  OLED_CMD_SET_SEGMENT_REMAP_NORMAL = 0xa0, ///< Column 0 on the left.
  OLED_CMD_SET_SEGMENT_REMAP_REVERSE = 0xa1, ///< Columns mirrored.
  OLED_CMD_SET_DISPLAY_RAM = 0xa4,           ///< Show RAM contents.
  OLED_CMD_SET_DISPLAY_ENTIRE = 0xa5,        ///< Light every pixel.
  OLED_CMD_SET_DISPLAY_NORMAL = 0xa6,        ///< 1 bit = lit.
  OLED_CMD_SET_DISPLAY_INVERSE = 0xa7,       ///< 0 bit = lit.
  OLED_CMD_SET_MULTIPLEX_RATIO = 0xa8,       ///< Arg: rows - 1.
  OLED_CMD_SET_DC_DC = 0xad,                 ///< Arg: DC-DC setting.
  OLED_CMD_DISPLAY_OFF = 0xae,               ///< Panel off. RAM is kept.
  OLED_CMD_DISPLAY_ON = 0xaf,                ///< Panel on.
  OLED_CMD_SET_PAGE_ADDR = 0xb0,             ///< OR in the page number.
  OLED_CMD_SET_SCAN_DIR_NORMAL = 0xc0,       ///< Rows top to bottom.
  OLED_CMD_SET_SCAN_DIR_REVERSE = 0xc8,      ///< Rows mirrored.
  OLED_CMD_SET_DISPLAY_OFFSET = 0xd3,        ///< Arg: vertical shift.
  OLED_CMD_SET_DISPLAY_CLOCK = 0xd5,    ///< Arg: clock divide and oscillator.
  OLED_CMD_SET_PRECHARGE_PERIOD = 0xd9, ///< Arg: precharge period.
  OLED_CMD_SET_COM_PINS = 0xda,         ///< Arg: COM pin layout.
  OLED_CMD_SET_VCOM_DESELECT_LEVEL = 0xdb, ///< Arg: VCOM level.
  OLED_CMD_SET_DISPLAY_STARTLINE = 0xdc,   ///< SH1107 only.
  OLED_CMD_RMW_START = 0xe0,               ///< Read-modify-write. Unused.
  OLED_CMD_RMW_END = 0xee,                 ///< Read-modify-write. Unused.
} rp2040_oled_cmd_t;

/** @brief Mirrors the image. */
typedef enum {
  FLIP_NONE = 0x0,                              ///< No mirroring.
  FLIP_HORIZONTAL = 0x1,                        ///< Mirror left to right.
  FLIP_VERTICAL = 0x2,                          ///< Mirror top to bottom.
  FLIP_BOTH = (FLIP_HORIZONTAL | FLIP_VERTICAL) ///< Both.
} rp2040_oled_flip_t;

/**
 * @brief One display: its wiring, settings and driver state.
 *
 * Set the config fields, then call rp2040_oled_init(), which fills in the
 * rest.
 */
typedef struct _rp2040_oled {
  /* config: set before calling rp2040_oled_init */
  spi_inst_t *spi;   ///< spi0 or spi1.
  uint8_t sck_pin;   ///< SPI clock pin.
  uint8_t mosi_pin;  ///< SPI data pin.
  uint8_t cs_pin;    ///< Chip select pin.
  uint8_t dc_pin;    ///< Data/command pin. Low = commands, high = pixels.
  uint32_t baudrate; ///< SPI clock speed in Hz (e.g. 10 MHz), not a UART rate.
  uint8_t reset_pin; ///< Reset pin, or PIN_UNDEF if not wired up.
  uint8_t width;     ///< Width in pixels. Set by init.
  uint8_t height;    ///< Height in pixels. Set by init.
  bool invert;       ///< true to show 0 bits as lit.
  rp2040_oled_flip_t flip; ///< How to mirror the image.

  /* driver state, managed by the library */
  /**
   * @brief The framebuffer, or with doublebuf, what's on the screen right now.
   */
  uint8_t *gdram;
  size_t gdram_size; ///< Bytes in gdram.
  /** @brief Where the next write lands. */
  struct {
    uint8_t x; ///< Column.
    uint8_t y; ///< Page.
  } cursor;
  /**
   * @brief With doublebuf, the framebuffer that drawing goes into. Without,
   * one dirty bit per column.
   */
  uint8_t *dirty_buf;
  size_t dirty_buf_size; ///< Bytes in dirty_buf.
  bool is_dirty;         ///< Something changed since the last flush.
  bool use_doublebuf;    ///< Config: use a second full framebuffer.
} rp2040_oled_t;

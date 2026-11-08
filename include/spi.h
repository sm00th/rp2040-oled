/*
 * SPDX-License-Identifier: MIT
 * 2026, Ramzey Burdette
 */
/**
 * @file spi.h
 * @brief SPI transport between the RP2040 and the display controller.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "rp2040-oled-types.h"

/**
 * @brief Starts the SPI peripheral and sets up the display's pins.
 *
 * SCK and MOSI go to the SPI peripheral. DC, CS and (if wired) reset become
 * outputs that start high.
 *
 * @param oled Display whose `spi`, `baudrate` and pin fields are set.
 */
void rp2040_spi_init(rp2040_oled_t *oled);

/**
 * @brief Sends bytes to the display as commands or pixel data.
 *
 * Drives DC low for commands and high for data, and holds CS low for the
 * transfer. Blocking: commands are rare and small, and a full page of data
 * takes about a millisecond at most.
 *
 * @param oled Display to send to.
 * @param data Bytes to send.
 * @param len Number of bytes in @p data.
 * @param is_command true to send as commands, false as pixel data.
 * @return Number of bytes written.
 */
size_t rp2040_spi_write(rp2040_oled_t *oled, const uint8_t *data, size_t len,
                        bool is_command);

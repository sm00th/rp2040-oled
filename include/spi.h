/*
 * SPDX-License-Identifier: MIT
 * 2026, Ramzey Burdette
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "rp2040-oled-types.h"

void rp2040_spi_init(rp2040_oled_t *oled);

size_t rp2040_spi_write(rp2040_oled_t *oled, const uint8_t *data, size_t len,
                        bool is_command);
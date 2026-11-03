#pragma once

#include "include/rp2040-oled.h"
#include <stdbool.h>

void rp2040_spi_init(rp2040_oled_t *oled);

size_t rp2040_spi_write(rp2040_oled_t *oled, const uint8_t *data, size_t len,
                        bool is_command);
/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#include "include/rp2040-oled.h"

void rp2040_i2c_init(rp2040_oled_t *oled);
bool rp2040_i2c_test_addr(rp2040_oled_t *oled, uint8_t addr);
int rp2040_i2c_read_register(rp2040_oled_t *oled, uint8_t reg, uint8_t *data, size_t len);
size_t rp2040_i2c_write(rp2040_oled_t *oled, const uint8_t *data, size_t len);

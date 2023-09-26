/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */

#include <string.h>

#include "i2c.h"

void rp2040_i2c_init(rp2040_oled_t *oled)
{
        i2c_init(oled->i2c, oled->baudrate);

        gpio_set_function(oled->sda_pin, GPIO_FUNC_I2C);
        gpio_set_function(oled->scl_pin, GPIO_FUNC_I2C);

        gpio_pull_up(oled->sda_pin);
        gpio_pull_up(oled->scl_pin);
}

bool rp2040_i2c_test_addr(rp2040_oled_t *oled, uint8_t addr)
{
        uint8_t buf;
        int ret;

        ret = i2c_read_blocking(oled->i2c, addr, &buf, 1, false);
        return ret != PICO_ERROR_GENERIC;
}

int rp2040_i2c_read_register(rp2040_oled_t *oled, uint8_t reg, uint8_t *data, size_t len)
{
        int ret;

        ret = i2c_write_blocking(oled->i2c, oled->addr, &reg, 1, true);
        if (ret < 0)
                return ret;

        ret = i2c_read_blocking(oled->i2c, oled->addr, data, len, false);
        return ret;
}

size_t rp2040_i2c_write(rp2040_oled_t *oled, const uint8_t *data, size_t len)
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

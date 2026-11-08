/*
 * SPDX-License-Identifier: MIT
 * 2026, Ramzey Burdette
 */
/**
 * @file spi.c
 * @brief SPI transport between the RP2040 and the display controller.
 */
#include "spi.h"

#include <stdbool.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"

#include "rp2040-oled-types.h"

void rp2040_spi_init(rp2040_oled_t *oled) {
  spi_init(oled->spi, oled->baudrate);

  gpio_set_function(oled->sck_pin, GPIO_FUNC_SPI);
  gpio_set_function(oled->mosi_pin, GPIO_FUNC_SPI);

  gpio_init(oled->dc_pin);
  gpio_put(oled->dc_pin, GPIO_LEVEL_HIGH);
  gpio_set_dir(oled->dc_pin, GPIO_OUT);

  gpio_init(oled->cs_pin);
  gpio_put(oled->cs_pin, GPIO_LEVEL_HIGH);
  gpio_set_dir(oled->cs_pin, GPIO_OUT);

  if (oled->reset_pin != PIN_UNDEF) {
    gpio_init(oled->reset_pin);
    gpio_put(oled->reset_pin, GPIO_LEVEL_HIGH);
    gpio_set_dir(oled->reset_pin, GPIO_OUT);
  }
}

size_t rp2040_spi_write(rp2040_oled_t *oled, const uint8_t *data, size_t len,
                        bool is_command) {

  if (is_command) {
    gpio_put(oled->dc_pin, GPIO_LEVEL_LOW);
    gpio_put(oled->cs_pin, GPIO_LEVEL_LOW);
    int num_bytes_returned = spi_write_blocking(oled->spi, data, len);
    gpio_put(oled->cs_pin, GPIO_LEVEL_HIGH);
    gpio_put(oled->dc_pin, GPIO_LEVEL_HIGH);
    return num_bytes_returned;
  } else {
    gpio_put(oled->dc_pin, GPIO_LEVEL_HIGH);
    gpio_put(oled->cs_pin, GPIO_LEVEL_LOW);

    int count = spi_write_blocking(oled->spi, data, len);

    gpio_put(oled->cs_pin, GPIO_LEVEL_HIGH);
    return count;
  }
}

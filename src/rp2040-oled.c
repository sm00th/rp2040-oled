#include <stdlib.h>
#include <string.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/error.h"

#include "rp2040-oled.h"

static void rp2040_i2c_init(rp2040_oled_t *oled)
{
        i2c_init(oled->i2c, oled->baudrate);

        gpio_set_function(oled->sda_pin, GPIO_FUNC_I2C);
        gpio_set_function(oled->scl_pin, GPIO_FUNC_I2C);

        gpio_pull_up(oled->sda_pin);
        gpio_pull_up(oled->scl_pin);
}

static uint8_t *rp2040_oled_alloc_data_buf(size_t size)
{
        uint8_t *buf;

        buf = malloc(size + 1);
        memset(buf, 0x00, size + 1);
        buf[0] = OLED_CB_DATA_BIT;

        return buf + 1;
}

static void rp2040_oled_free_data_buf(uint8_t *buf)
{
        free(buf - 1);
}

static bool rp2040_i2c_test_addr(rp2040_oled_t *oled, uint8_t addr)
{
        uint8_t buf;
        int ret;

        ret = i2c_read_blocking(oled->i2c, addr, &buf, 1, false);
        return ret != PICO_ERROR_GENERIC;
}

static int rp2040_i2c_read_register(rp2040_oled_t *oled, uint8_t reg, uint8_t *data, size_t len)
{
        int ret;

        ret = i2c_write_blocking(oled->i2c, oled->addr, &reg, 1, true);
        if (ret < 0)
                return ret;

        ret = i2c_read_blocking(oled->i2c, oled->addr, data, len, false);
        return ret;
}

static size_t rp2040_i2c_write(rp2040_oled_t *oled, uint8_t *data, size_t len)
{
        size_t sent = 0;
        size_t ret = 0;
        if (len > 32) {
                len--;
                while (len >= 31) {
                        ret = i2c_write_blocking(oled->i2c, oled->addr, data, 32, true);
                        if (ret != 32)
                                return -1;

                        sent += ret;
                        len -= 31;
                        data += 31;
                        data[0] = OLED_CB_DATA_BIT;
                }
                if (len)
                        len++;
        }

        if (len) {
                ret = i2c_write_blocking(oled->i2c, oled->addr, data, len, true);
                if (ret != len)
                        return -1;

                sent += ret;
        }

        return sent;
}

static bool rp2040_oled_write_command(rp2040_oled_t *oled, uint8_t cmd)
{
        uint8_t buf[] = { 0x00, cmd };
        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_write_command_with_arg(rp2040_oled_t *oled, uint8_t cmd, uint8_t arg)
{
        uint8_t buf[] = { 0x00, cmd, arg };
        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_write_gdram(rp2040_oled_t *oled, uint8_t *buf, size_t size,
                                    bool render)
{
        size_t gdram_offset = oled->cursor.x * oled->cursor.y / 8;
        if (gdram_offset + size > oled->gdram_size)
                return false;

        memcpy(oled->gdram + gdram_offset, buf, size);

        /* TODO: if real screens don't wrap do not either */
        oled->cursor.y += gdram_offset / 8;
        oled->cursor.x += gdram_offset % 8;

        if (oled->cursor.x >= oled->width) {
                oled->cursor.y += 1;
                oled->cursor.x -= oled->width;
        }

        return rp2040_i2c_write(oled, buf - 1, size + 1) != size + 1;
}

static void rp2040_oled_reset(rp2040_oled_t *oled)
{
        gpio_put(oled->reset_pin, GPIO_LEVEL_LOW);
        sleep_ms(50);
        gpio_put(oled->reset_pin, GPIO_LEVEL_HIGH);
        sleep_ms(10);
}

static int rp2040_oled_display_init(rp2040_oled_t *oled)
{
        uint8_t *initbuf;
        size_t initlen;

        switch(oled->size) {
                case OLED_128x128:
                        oled->width =  128;
                        oled->height = 128;
                        initbuf = oled128_initbuf;
                        initlen = sizeof(oled128_initbuf);
                        break;
                case OLED_128x64:
                        oled->width =  128;
                        oled->height = 64;
                        initbuf = oled64_initbuf;
                        initlen = sizeof(oled64_initbuf);
                        break;
                case OLED_128x32:
                        oled->width =  128;
                        oled->height = 32;
                        initbuf = oled32_initbuf;
                        initlen = sizeof(oled32_initbuf);
                        break;
                case OLED_132x64:
                        oled->width =  132;
                        oled->height = 64;
                        initbuf = oled64_initbuf;
                        initlen = sizeof(oled64_initbuf);
                        break;
                case OLED_96x16:
                        oled->width =  96;
                        oled->height = 16;
                        initbuf = oled32_initbuf;
                        initlen = sizeof(oled32_initbuf);
                        break;
                case OLED_72x40:
                        oled->width =  72;
                        oled->height = 40;
                        initbuf = oled72_initbuf;
                        initlen = sizeof(oled72_initbuf);
                        break;
                case OLED_64x128:
                        oled->width =  64;
                        oled->height = 128;
                        initbuf = oled64x128_initbuf;
                        initlen = sizeof(oled64x128_initbuf);
                        break;
                case OLED_64x32:
                        oled->width =  64;
                        oled->height = 32;
                        initbuf = oled64_initbuf;
                        initlen = sizeof(oled64_initbuf);
                        break;
                default:
                        return -1;
        };

        rp2040_i2c_write(oled, initbuf, initlen);

        if (oled->invert)
                rp2040_oled_write_command(oled, OLED_CMD_SET_DISPLAY_INVERSE);

        if (oled->flip & FLIP_HORIZONTAL)
                rp2040_oled_write_command(oled, OLED_CMD_SET_SEGMENT_REMAP_NORMAL);
        else if (oled->flip & FLIP_VERTICAL)
                rp2040_oled_write_command(oled, OLED_CMD_SET_SCAN_DIR_NORMAL);

        oled->gdram_size = oled->width * oled->height / 8;
        oled->gdram = malloc(oled->gdram_size);
        memset(oled->gdram, 0x00, oled->gdram_size);

        oled->cursor.x = 0;
        oled->cursor.y = 0;

        oled->dirty_area.x      = 0;
        oled->dirty_area.y      = 0;
        oled->dirty_area.width  = 0;
        oled->dirty_area.height = 0;

        return 0;
}

static uint8_t rp2040_oled_scan(rp2040_oled_t *oled)
{
        uint8_t i;
        uint8_t addr = 0x00;

        for (i = 0; i < sizeof(scan_addrs); i++) {
                addr = scan_addrs[i];
                if (rp2040_i2c_test_addr(oled, addr))
                        return addr;
        }

        return PIN_UNDEF;
}

bool rp2040_oled_is_sh1106(rp2040_oled_t *oled)
{
        const uint8_t TEST_DATA[] = { 0xf2, 0x3a, 0x45, 0x8b, 0x00 };
        uint8_t buf[4];
        uint8_t i;

        rp2040_oled_set_power(oled, false);

        for (i = 0; i < sizeof(TEST_DATA); i++) {
                buf[0] = OLED_CB_CONTINUATION_BIT;
                buf[1] = OLED_CMD_RMW_START;
                buf[2] = 0xc0;
                if (rp2040_i2c_write(oled, buf, 3) != 3)
                        break;

                if (i > 0 && buf[1] != TEST_DATA[i - 1])
                        break;

                buf[0] = 0xc0;
                buf[1] = TEST_DATA[i];
                buf[2] = OLED_CB_CONTINUATION_BIT;
                buf[3] = OLED_CMD_RMW_END;

                if (rp2040_i2c_write(oled, buf, 4) != 4)
                        break;
        }

        rp2040_oled_set_power(oled, true);

        return i == sizeof(TEST_DATA);
}

rp2040_oled_type_t rp2040_oled_autodetect(rp2040_oled_t *oled)
{
        rp2040_oled_type_t type = OLED_NOT_FOUND;
        uint8_t status = 0x00;

        if (rp2040_i2c_read_register(oled, 0x00, &status, 1) < 0)
                return OLED_NOT_FOUND;

        status &= 0x0f;

        if ((status == 0x07 || status == 0x0f) && oled->size == OLED_128x128) {
                oled->flip = !oled->flip;
                type = OLED_SH1107_3C;
        } else if (status == 0x08) {
                type = OLED_SH1106_3C;
        } else if (status == 0x03 || status == 0x06 || status == 0x07) {
                if (rp2040_oled_is_sh1106(oled)) {
                        type = OLED_SH1106_3C;
                } else {
                        type = OLED_SSD1306_3C;
                }
        }

        if (type != OLED_NOT_FOUND && oled->addr == 0x3d)
                type++;

        return type;
}

rp2040_oled_type_t rp2040_oled_init(rp2040_oled_t *oled)
{
        rp2040_oled_type_t type = OLED_NOT_FOUND;
        rp2040_i2c_init(oled);

        if (oled->reset_pin != PIN_UNDEF) {
                gpio_set_dir(oled->reset_pin, GPIO_OUT);
                rp2040_oled_reset(oled);
        }

        if (oled->addr == PIN_UNDEF || oled->addr == 0x00) {
                oled->addr = rp2040_oled_scan(oled);
                if (oled->addr == PIN_UNDEF)
                        return OLED_NOT_FOUND;
        } else if (!rp2040_i2c_test_addr(oled, oled->addr)) {
                return OLED_NOT_FOUND;
        }

        type = rp2040_oled_autodetect(oled);

        rp2040_oled_display_init(oled);

        return type;
}

static bool rp2040_oled_set_position(rp2040_oled_t *oled, uint8_t x, uint8_t y)
{
        uint8_t buf[4];

        y /= 8;

        if (oled->size == OLED_64x32) {
                x += 32;
                if (oled->flip == 0)
                        y += 4;
        } else if (oled->size == OLED_132x64) {
                x += 2;
        } else if (oled->size == OLED_96x16) {
                if (oled->flip == 0)
                        y += 2;
                else
                        x += 32;
        } else if (oled->size == OLED_72x40) {
                x += 28;
                if (oled->flip == 0)
                        y += 3;
        }

        buf[0] = 0x00;
        buf[1] = OLED_CMD_SET_PAGE_ADDR | y;
        buf[2] = OLED_CMD_SET_LC_ADDR | (x & 0x0f);
        buf[2] = OLED_CMD_SET_HC_ADDR | (x >> 4);

        oled->cursor.x = x;
        oled->cursor.y = y;

        return rp2040_i2c_write(oled, buf, sizeof(buf)) == sizeof(buf);
}

static bool rp2040_oled_fill(rp2040_oled_t *oled, uint8_t fill_byte)
{
        bool ret = true;
        uint8_t *fill_buf;
        size_t fill_buf_size = (oled->height * oled->width) / 8;

        fill_buf = rp2040_oled_alloc_data_buf(fill_buf_size);

        if (!rp2040_oled_set_position(oled, 0, 0)) {
                rp2040_oled_free_data_buf(fill_buf);
                return false;
        }

        /* TODO: figure out if it will wrap on its own or we need to swith pages manually */
        if (!rp2040_oled_write_gdram(oled, fill_buf, fill_buf_size, true)) {
                rp2040_oled_free_data_buf(fill_buf);
                return false;
        }

        rp2040_oled_free_data_buf(fill_buf);
        return ret;
}

bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast)
{
        return rp2040_oled_write_command_with_arg(oled, OLED_CMD_SET_CONTRAST, contrast);
}

bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled)
{
        return rp2040_oled_write_command(oled, enabled ? OLED_CMD_DISPLAY_ON : OLED_CMD_DISPLAY_OFF);
}

bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y, char *msg, size_t len)
{
        uint8_t *buf;
        size_t buf_size = len * 6;
        size_t i = 0;
        if (x >= oled->width || y >= oled->height)
                return false;

        if (!rp2040_oled_set_position(oled, x, y))
                return false;

        buf = rp2040_oled_alloc_data_buf(buf_size);

        for (i = 0; i < len; i++) {
                uint8_t font_index = msg[i] - 32;

                buf[i * 6] = 0x00;
                memcpy(buf + (1 + (i * 6)), font_6x8 + (font_index * 5), 5);
        }

        if (!rp2040_oled_write_gdram(oled, buf, buf_size, true)) {
                rp2040_oled_free_data_buf(buf);
                return false;
        }

        rp2040_oled_free_data_buf(buf);
        return true;
}

bool rp2040_oled_draw_sprite(rp2040_oled_t *oled, uint8_t *sprite, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
        uint8_t *buf;
        size_t buf_size;
        uint8_t orig_width = width;

        if (x + width > oled->width)
                width = oled->width - x;

        if (y + height > oled->height)
                height = oled->height - y;

        /* TODO: mod8 magic shifts */

        buf_size = width;
        buf = rp2040_oled_alloc_data_buf(buf_size);

        for (uint8_t cury = 0; cury < height; cury += 8) {
                if (!rp2040_oled_set_position(oled, x, cury)) {
                        rp2040_oled_free_data_buf(buf);
                        return false;
                }
                memcpy(buf, sprite + (cury * orig_width), width);
                if (!rp2040_oled_write_gdram(oled, buf, buf_size, true)) {
                        rp2040_oled_free_data_buf(buf);
                        return false;
                }
        }

        rp2040_oled_free_data_buf(buf);
        return true;
}

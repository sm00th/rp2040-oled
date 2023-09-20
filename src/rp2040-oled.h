#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define PIN_UNDEF 0xff

#define GPIO_LEVEL_HIGH 1
#define GPIO_LEVEL_LOW 0

const uint8_t scan_addrs[] = {0x3c, 0x3d};

enum {
        OLED_CB_CONTINUATION_BIT = 0x80,
        OLED_CB_DATA_BIT         = 0x40,
};

typedef enum {
  OLED_128x128 = 1,
  OLED_128x64,
  OLED_128x32,
  OLED_132x64,
  OLED_96x16,
  OLED_64x128,
  OLED_64x32,
  OLED_72x40,
} rp2040_oled_size_t;

typedef enum {
  OLED_NOT_FOUND = -1,
  OLED_SSD1306_3C,
  OLED_SSD1306_3D,
  OLED_SH1106_3C,
  OLED_SH1106_3D,
  OLED_SH1107_3C,
  OLED_SH1107_3D,
} rp2040_oled_type_t;

typedef enum {
        OLED_CMD_SET_LC_ADDR               = 0x00,
        OLED_CMD_SET_HC_ADDR               = 0x10,
        OLED_CMD_SET_SSD1306_ADDR_MODE     = 0x20,
        OLED_CMD_SET_ADDR_PAGE             = 0x20,
        OLED_CMD_SET_ADDR_VERTICAL         = 0x21,
        OLED_CMD_SET_DISPLAY_STARTLINE0    = 0x40,
        OLED_CMD_SET_CONTRAST              = 0x81,
        OLED_CMD_SET_CHARGE_PUMP           = 0x8d,
        OLED_CMD_SET_SEGMENT_REMAP_NORMAL  = 0xa0,
        OLED_CMD_SET_SEGMENT_REMAP_REVERSE = 0xa1,
        OLED_CMD_SET_DISPLAY_RAM           = 0xa4,
        OLED_CMD_SET_DISPLAY_ENTIRE        = 0xa5,
        OLED_CMD_SET_DISPLAY_NORMAL        = 0xa6,
        OLED_CMD_SET_DISPLAY_INVERSE       = 0xa7,
        OLED_CMD_SET_MULTIPLEX_RATIO       = 0xa8,
        OLED_CMD_SET_DC_DC                 = 0xad,
        OLED_CMD_DISPLAY_OFF               = 0xae,
        OLED_CMD_DISPLAY_ON                = 0xaf,
        OLED_CMD_SET_PAGE_ADDR             = 0xb0,
        OLED_CMD_SET_SCAN_DIR_NORMAL       = 0xc0,
        OLED_CMD_SET_SCAN_DIR_REVERSE      = 0xc8,
        OLED_CMD_SET_DISPLAY_OFFSET        = 0xd3,
        OLED_CMD_SET_DISPLAY_CLOCK         = 0xd5,
        OLED_CMD_SET_PRECHARGE_PERIOD      = 0xd9,
        OLED_CMD_SET_COM_PINS              = 0xda,
        OLED_CMD_SET_VCOM_DESELECT_LEVEL   = 0xdb,
        OLED_CMD_SET_DISPLAY_STARTLINE     = 0xdc,
        OLED_CMD_RMW_START                 = 0xe0,
        OLED_CMD_RMW_END                   = 0xee,
} rp2040_oled_cmd_t;

typedef enum {
        FLIP_HORIZONTAL = 0x1,
        FLIP_VERTICAL   = 0x2,
        FLIP_BOTH       = (FLIP_HORIZONTAL | FLIP_VERTICAL)
} rp2040_oled_flip_t;

typedef struct _rp2040_oled {
        i2c_inst_t         *i2c;
        uint8_t            sda_pin;
        uint8_t            scl_pin;
        uint32_t           baudrate;
        uint8_t            addr;
        uint8_t            reset_pin;
        rp2040_oled_size_t size;
        uint8_t            width;
        uint8_t            height;
        bool               invert;
        rp2040_oled_flip_t flip;
} rp2040_oled_t;

const uint8_t font_6x8[] = {
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x06, 0x5f, 0x06, 0x00,
		0x07, 0x03, 0x00, 0x07, 0x03,
		0x24, 0x7e, 0x24, 0x7e, 0x24,
		0x24, 0x2b, 0x6a, 0x12, 0x00,
		0x63, 0x13, 0x08, 0x64, 0x63,
		0x36, 0x49, 0x56, 0x20, 0x50,
		0x00, 0x07, 0x03, 0x00, 0x00,
		0x00, 0x3e, 0x41, 0x00, 0x00,
		0x00, 0x41, 0x3e, 0x00, 0x00,
		0x08, 0x3e, 0x1c, 0x3e, 0x08,
		0x08, 0x08, 0x3e, 0x08, 0x08,
		0x00, 0xe0, 0x60, 0x00, 0x00,
		0x08, 0x08, 0x08, 0x08, 0x08,
		0x00, 0x60, 0x60, 0x00, 0x00,
		0x20, 0x10, 0x08, 0x04, 0x02,
		0x3e, 0x51, 0x49, 0x45, 0x3e,
		0x00, 0x42, 0x7f, 0x40, 0x00,
		0x62, 0x51, 0x49, 0x49, 0x46,
		0x22, 0x49, 0x49, 0x49, 0x36,
		0x18, 0x14, 0x12, 0x7f, 0x10,
		0x2f, 0x49, 0x49, 0x49, 0x31,
		0x3c, 0x4a, 0x49, 0x49, 0x30,
		0x01, 0x71, 0x09, 0x05, 0x03,
		0x36, 0x49, 0x49, 0x49, 0x36,
		0x06, 0x49, 0x49, 0x29, 0x1e,
		0x00, 0x6c, 0x6c, 0x00, 0x00,
		0x00, 0xec, 0x6c, 0x00, 0x00,
		0x08, 0x14, 0x22, 0x41, 0x00,
		0x24, 0x24, 0x24, 0x24, 0x24,
		0x00, 0x41, 0x22, 0x14, 0x08,
		0x02, 0x01, 0x59, 0x09, 0x06,
		0x3e, 0x41, 0x5d, 0x55, 0x1e,
		0x7e, 0x11, 0x11, 0x11, 0x7e,
		0x7f, 0x49, 0x49, 0x49, 0x36,
		0x3e, 0x41, 0x41, 0x41, 0x22,
		0x7f, 0x41, 0x41, 0x41, 0x3e,
		0x7f, 0x49, 0x49, 0x49, 0x41,
		0x7f, 0x09, 0x09, 0x09, 0x01,
		0x3e, 0x41, 0x49, 0x49, 0x7a,
		0x7f, 0x08, 0x08, 0x08, 0x7f,
		0x00, 0x41, 0x7f, 0x41, 0x00,
		0x30, 0x40, 0x40, 0x40, 0x3f,
		0x7f, 0x08, 0x14, 0x22, 0x41,
		0x7f, 0x40, 0x40, 0x40, 0x40,
		0x7f, 0x02, 0x04, 0x02, 0x7f,
		0x7f, 0x02, 0x04, 0x08, 0x7f,
		0x3e, 0x41, 0x41, 0x41, 0x3e,
		0x7f, 0x09, 0x09, 0x09, 0x06,
		0x3e, 0x41, 0x51, 0x21, 0x5e,
		0x7f, 0x09, 0x09, 0x19, 0x66,
		0x26, 0x49, 0x49, 0x49, 0x32,
		0x01, 0x01, 0x7f, 0x01, 0x01,
		0x3f, 0x40, 0x40, 0x40, 0x3f,
		0x1f, 0x20, 0x40, 0x20, 0x1f,
		0x3f, 0x40, 0x3c, 0x40, 0x3f,
		0x63, 0x14, 0x08, 0x14, 0x63,
		0x07, 0x08, 0x70, 0x08, 0x07,
		0x71, 0x49, 0x45, 0x43, 0x00,
		0x00, 0x7f, 0x41, 0x41, 0x00,
		0x02, 0x04, 0x08, 0x10, 0x20,
		0x00, 0x41, 0x41, 0x7f, 0x00,
		0x04, 0x02, 0x01, 0x02, 0x04,
		0x80, 0x80, 0x80, 0x80, 0x80,
		0x00, 0x03, 0x07, 0x00, 0x00,
		0x20, 0x54, 0x54, 0x54, 0x78,
		0x7f, 0x44, 0x44, 0x44, 0x38,
		0x38, 0x44, 0x44, 0x44, 0x28,
		0x38, 0x44, 0x44, 0x44, 0x7f,
		0x38, 0x54, 0x54, 0x54, 0x08,
		0x08, 0x7e, 0x09, 0x09, 0x00,
		0x18, 0xa4, 0xa4, 0xa4, 0x7c,
		0x7f, 0x04, 0x04, 0x78, 0x00,
		0x00, 0x00, 0x7d, 0x40, 0x00,
		0x40, 0x80, 0x84, 0x7d, 0x00,
		0x7f, 0x10, 0x28, 0x44, 0x00,
		0x00, 0x00, 0x7f, 0x40, 0x00,
		0x7c, 0x04, 0x18, 0x04, 0x78,
		0x7c, 0x04, 0x04, 0x78, 0x00,
		0x38, 0x44, 0x44, 0x44, 0x38,
		0xfc, 0x44, 0x44, 0x44, 0x38,
		0x38, 0x44, 0x44, 0x44, 0xfc,
		0x44, 0x78, 0x44, 0x04, 0x08,
		0x08, 0x54, 0x54, 0x54, 0x20,
		0x04, 0x3e, 0x44, 0x24, 0x00,
		0x3c, 0x40, 0x20, 0x7c, 0x00,
		0x1c, 0x20, 0x40, 0x20, 0x1c,
		0x3c, 0x60, 0x30, 0x60, 0x3c,
		0x6c, 0x10, 0x10, 0x6c, 0x00,
		0x9c, 0xa0, 0x60, 0x3c, 0x00,
		0x64, 0x54, 0x54, 0x4c, 0x00,
		0x08, 0x3e, 0x41, 0x41, 0x00,
		0x00, 0x00, 0x77, 0x00, 0x00,
		0x00, 0x41, 0x41, 0x3e, 0x08,
		0x02, 0x01, 0x02, 0x01, 0x00,
		0x3c, 0x26, 0x23, 0x26, 0x3c};

uint8_t oled128_initbuf[] = {
        0x00,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_DISPLAY_STARTLINE, 0x00,
        OLED_CMD_SET_CONTRAST, 0x40,
        OLED_CMD_SET_SEGMENT_REMAP_REVERSE, 0xc8,
        OLED_CMD_SET_MULTIPLEX_RATIO, 0x7f,
        OLED_CMD_SET_DISPLAY_CLOCK, 0x50,
        OLED_CMD_SET_PRECHARGE_PERIOD, 0x22,
        OLED_CMD_SET_VCOM_DESELECT_LEVEL, 0x35,
        OLED_CMD_SET_PAGE_ADDR | 0x00,
        OLED_CMD_SET_COM_PINS, 0x12,
        OLED_CMD_SET_DISPLAY_RAM,
        OLED_CMD_SET_DISPLAY_NORMAL,
        OLED_CMD_DISPLAY_ON};

uint8_t oled64x128_initbuf[] = {
        0x00,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_DISPLAY_CLOCK, 0x51,
        OLED_CMD_SET_ADDR_PAGE,
        OLED_CMD_SET_MULTIPLEX_RATIO, 0x3f,
        OLED_CMD_SET_DISPLAY_STARTLINE, 0x00,
        OLED_CMD_SET_DISPLAY_OFFSET, 0x60,
        OLED_CMD_SET_DC_DC, 0x80,
        OLED_CMD_SET_DISPLAY_NORMAL,
        OLED_CMD_SET_DISPLAY_RAM,
        OLED_CMD_SET_SEGMENT_REMAP_NORMAL, 0xc0,
        OLED_CMD_SET_CONTRAST, 0x40,
        OLED_CMD_SET_PRECHARGE_PERIOD, 0x22,
        OLED_CMD_SET_VCOM_DESELECT_LEVEL, 0x35,
        OLED_CMD_DISPLAY_ON};

uint8_t oled132_initbuf[] = {
        0x00,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_LC_ADDR | 0x02,
        OLED_CMD_SET_HC_ADDR | 0x00,
        OLED_CMD_SET_DISPLAY_STARTLINE0,
        OLED_CMD_SET_CONTRAST, 0xa0,
        OLED_CMD_SET_SCAN_DIR_NORMAL,
        OLED_CMD_SET_DISPLAY_NORMAL,
        OLED_CMD_SET_MULTIPLEX_RATIO, 0x3f,
        OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
        OLED_CMD_SET_DISPLAY_CLOCK, 0x80,
        OLED_CMD_SET_PRECHARGE_PERIOD, 0xf1,
        OLED_CMD_SET_COM_PINS, 0x12,
        OLED_CMD_SET_VCOM_DESELECT_LEVEL, 0x40,
        OLED_CMD_SET_SSD1306_ADDR_MODE, 0x02,
        OLED_CMD_SET_DISPLAY_RAM,
        OLED_CMD_SET_DISPLAY_NORMAL};

uint8_t oled64_initbuf[] = {
        0x00,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_MULTIPLEX_RATIO, 0x3f,
        OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
        OLED_CMD_SET_DISPLAY_STARTLINE0,
        OLED_CMD_SET_SEGMENT_REMAP_REVERSE, 0xc8,
        OLED_CMD_SET_COM_PINS, 0x12,
        OLED_CMD_SET_CONTRAST, 0xff,
        OLED_CMD_SET_DISPLAY_RAM,
        OLED_CMD_SET_DISPLAY_NORMAL,
        OLED_CMD_SET_DISPLAY_CLOCK, 0x80,
        OLED_CMD_SET_CHARGE_PUMP, 0x14,
        OLED_CMD_DISPLAY_ON,
        OLED_CMD_SET_SSD1306_ADDR_MODE, 0x02};

uint8_t oled32_initbuf[] = {
        0x00,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_DISPLAY_CLOCK, 0x80,
        OLED_CMD_SET_MULTIPLEX_RATIO, 0x1f,
        OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
        OLED_CMD_SET_DISPLAY_STARTLINE0,
        OLED_CMD_SET_CHARGE_PUMP, 0x14,
        OLED_CMD_SET_SEGMENT_REMAP_REVERSE, 0xc8,
        OLED_CMD_SET_COM_PINS, 0x02,
        OLED_CMD_SET_CONTRAST, 0x7f,
        OLED_CMD_SET_PRECHARGE_PERIOD, 0xf1,
        OLED_CMD_SET_VCOM_DESELECT_LEVEL, 0x40,
        OLED_CMD_SET_DISPLAY_RAM,
        OLED_CMD_SET_DISPLAY_NORMAL,
        OLED_CMD_DISPLAY_ON};

uint8_t oled72_initbuf[] = {
        0x00,
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_MULTIPLEX_RATIO, 0x3f,
        OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
        OLED_CMD_SET_DISPLAY_STARTLINE0,
        OLED_CMD_SET_SEGMENT_REMAP_REVERSE, 0xc8,
        OLED_CMD_SET_COM_PINS, 0x12,
        OLED_CMD_SET_CONTRAST, 0xff,
        OLED_CMD_SET_DC_DC, 0x30,
        OLED_CMD_SET_PRECHARGE_PERIOD, 0xf1,
        OLED_CMD_SET_DISPLAY_RAM,
        OLED_CMD_SET_DISPLAY_NORMAL,
        OLED_CMD_SET_DISPLAY_CLOCK, 0x80,
        OLED_CMD_SET_CHARGE_PUMP, 0x14,
        OLED_CMD_DISPLAY_ON,
        OLED_CMD_SET_SSD1306_ADDR_MODE, 0x02};

rp2040_oled_type_t rp2040_oled_init(rp2040_oled_t *oled);
bool rp2040_oled_set_contrast(rp2040_oled_t *oled, uint8_t contrast);
bool rp2040_oled_set_power(rp2040_oled_t *oled, bool enabled);
bool rp2040_oled_write_string(rp2040_oled_t *oled, uint8_t x, uint8_t y, char *msg, size_t len);

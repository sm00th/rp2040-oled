/*
 * SPDX-License-Identifier: MIT
 * Copyright 2023, Artem Savkov
 */
/**
 * @file display.c
 * @brief SH1106 startup command sequence.
 */
#include "display.h"

#include <stdint.h>
#include <stdlib.h>

#include "rp2040-oled-types.h"

const uint8_t oled_sh1106_init[] = {OLED_CMD_DISPLAY_OFF,
                                    OLED_CMD_SET_DISPLAY_CLOCK,
                                    0x80, // clock divide and oscillator
                                    OLED_CMD_SET_MULTIPLEX_RATIO,
                                    0x3F, // 64 rows
                                    OLED_CMD_SET_DISPLAY_OFFSET,
                                    0x00, // no vertical shift
                                    OLED_CMD_SET_DISPLAY_STARTLINE0,
                                    OLED_CMD_SET_DC_DC,
                                    0x8B, // dc-dc on
                                    OLED_CMD_SET_SEGMENT_REMAP_REVERSE,
                                    OLED_CMD_SET_SCAN_DIR_REVERSE,
                                    OLED_CMD_SET_COM_PINS,
                                    0x12, // com pin layout
                                    OLED_CMD_SET_CONTRAST,
                                    0xFF, // full brightness
                                    OLED_CMD_SET_PRECHARGE_PERIOD,
                                    0x1F, // precharge period
                                    OLED_CMD_SET_VCOM_DESELECT_LEVEL,
                                    0x20, // vcom level
                                    0x33, // pump voltage, 9 v
                                    OLED_CMD_SET_DISPLAY_NORMAL,
                                    OLED_CMD_SET_DISPLAY_RAM,
                                    OLED_CMD_DISPLAY_ON};

const size_t oled_sh1106_init_len = sizeof(oled_sh1106_init);

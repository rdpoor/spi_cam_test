/**
 * @file arducam.c
 *
 * MIT License
 *
 * Copyright (c) 2022 R. Dunbar Poor
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

// *****************************************************************************
// Includes

#include "arducam.h"
#include "bsp_arducam.h"
#include "ov2640.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// *****************************************************************************
// Private types and definitions

#define UART_ID (void *)0
#define WRITE_BIT 0x80

#define I2C_ADDR 0x30

#define ARDUCHIP_FIFO 0x04 // FIFO and I2C control
#define FIFO_CLEAR_MASK 0x01
#define FIFO_START_MASK 0x02
#define FIFO_RDPTR_RST_MASK 0x10
#define FIFO_WRPTR_RST_MASK 0x20
#define ARDUCHIP_GPIO 0x06   // GPIO Write Register
#define GPIO_RESET_MASK 0x01 // 0 = Sensor reset, 1 =  Sensor normal operation
#define GPIO_PWDN_MASK 0x02  // 0 = Sensor normal operation, 1 = Sensor standby
#define GPIO_PWREN_MASK 0x04 // 0 = Sensor LDO disable, 1 = sensor LDO enable

#define BURST_FIFO_READ 0x3C  // Burst FIFO read operation
#define SINGLE_FIFO_READ 0x3D // Single FIFO read operation

#define ARDUCHIP_REV 0x40 // ArduCHIP revision
#define VER_LOW_MASK 0x3F
#define VER_HIGH_MASK 0xC0

#define ARDUCHIP_TRIG 0x41 // Trigger source
#define VSYNC_MASK 0x01
#define SHUTTER_MASK 0x02
#define CAP_DONE_MASK 0x08

#define FIFO_SIZE1 0x42 // Camera write FIFO size[7:0] for burst to read
#define FIFO_SIZE2 0x43 // Camera write FIFO size[15:8]
#define FIFO_SIZE3 0x44 // Camera write FIFO size[18:16]

/**
 * @brief a register / value pair
 */
struct sensor_reg {
    unsigned int reg;
    unsigned int val;
};

// *****************************************************************************
// Private (static, forward) declarations

static void rdSensorReg8_8(uint8_t regID, uint8_t *regDat);

static void wrSensorReg8_8(uint8_t regID, uint8_t regDat);

static void write_reg(uint8_t address, uint8_t value);

static uint8_t read_reg(uint8_t address);

static int wrSensorRegs8_8(const struct sensor_reg reglist[]);

// static unsigned char read_fifo(void);

static void set_fifo_burst(void);

static void flush_fifo(void);

static void start_capture(void);

// static void clear_fifo_flag(void)

static unsigned int read_fifo_length();

__attribute__((unused)) static void set_bit(unsigned char addr,
                                            unsigned char bit);

__attribute__((unused)) static void clear_bit(unsigned char addr,
                                              unsigned char bit);

static unsigned char get_bit(unsigned char addr, unsigned char bit);

// *****************************************************************************
// Private (static) storage

const struct sensor_reg OV2640_YUV_96x96[] = {
    {0xff, 0x0},  {0x2c, 0xff}, {0x2e, 0xdf}, {0xff, 0x1},  {0x3c, 0x32},
    {0x11, 0x0},  {0x9, 0x2},   {0x4, 0xa8},  {0x13, 0xe5}, {0x14, 0x48},
    {0x2c, 0xc},  {0x33, 0x78}, {0x3a, 0x33}, {0x3b, 0xfb}, {0x3e, 0x0},
    {0x43, 0x11}, {0x16, 0x10}, {0x39, 0x2},  {0x35, 0x88},

    {0x22, 0xa},  {0x37, 0x40}, {0x23, 0x0},  {0x34, 0xa0}, {0x6, 0x2},
    {0x6, 0x88},  {0x7, 0xc0},  {0xd, 0xb7},  {0xe, 0x1},   {0x4c, 0x0},
    {0x4a, 0x81}, {0x21, 0x99}, {0x24, 0x40}, {0x25, 0x38}, {0x26, 0x82},
    {0x5c, 0x0},  {0x63, 0x0},  {0x46, 0x22}, {0xc, 0x3a},  {0x5d, 0x55},
    {0x5e, 0x7d}, {0x5f, 0x7d}, {0x60, 0x55}, {0x61, 0x70}, {0x62, 0x80},
    {0x7c, 0x5},  {0x20, 0x80}, {0x28, 0x30}, {0x6c, 0x0},  {0x6d, 0x80},
    {0x6e, 0x0},  {0x70, 0x2},  {0x71, 0x94}, {0x73, 0xc1}, {0x3d, 0x34},
    {0x12, 0x4},  {0x5a, 0x57}, {0x4f, 0xbb}, {0x50, 0x9c}, {0xff, 0x0},
    {0xe5, 0x7f}, {0xf9, 0xc0}, {0x41, 0x24}, {0xe0, 0x14}, {0x76, 0xff},
    {0x33, 0xa0}, {0x42, 0x20}, {0x43, 0x18}, {0x4c, 0x0},  {0x87, 0xd0},
    {0x88, 0x3f}, {0xd7, 0x3},  {0xd9, 0x10}, {0xd3, 0x82}, {0xc8, 0x8},
    {0xc9, 0x80}, {0x7c, 0x0},  {0x7d, 0x0},  {0x7c, 0x3},  {0x7d, 0x48},
    {0x7d, 0x48}, {0x7c, 0x8},  {0x7d, 0x20}, {0x7d, 0x10}, {0x7d, 0xe},
    {0x90, 0x0},  {0x91, 0xe},  {0x91, 0x1a}, {0x91, 0x31}, {0x91, 0x5a},
    {0x91, 0x69}, {0x91, 0x75}, {0x91, 0x7e}, {0x91, 0x88}, {0x91, 0x8f},
    {0x91, 0x96}, {0x91, 0xa3}, {0x91, 0xaf}, {0x91, 0xc4}, {0x91, 0xd7},
    {0x91, 0xe8}, {0x91, 0x20}, {0x92, 0x0},

    {0x93, 0x6},  {0x93, 0xe3}, {0x93, 0x3},  {0x93, 0x3},  {0x93, 0x0},
    {0x93, 0x2},  {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},
    {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},  {0x96, 0x0},  {0x97, 0x8},
    {0x97, 0x19}, {0x97, 0x2},  {0x97, 0xc},  {0x97, 0x24}, {0x97, 0x30},
    {0x97, 0x28}, {0x97, 0x26}, {0x97, 0x2},  {0x97, 0x98}, {0x97, 0x80},
    {0x97, 0x0},  {0x97, 0x0},  {0xa4, 0x0},  {0xa8, 0x0},  {0xc5, 0x11},
    {0xc6, 0x51}, {0xbf, 0x80}, {0xc7, 0x10}, {0xb6, 0x66}, {0xb8, 0xa5},
    {0xb7, 0x64}, {0xb9, 0x7c}, {0xb3, 0xaf}, {0xb4, 0x97}, {0xb5, 0xff},
    {0xb0, 0xc5}, {0xb1, 0x94}, {0xb2, 0xf},  {0xc4, 0x5c}, {0xa6, 0x0},
    {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x1b}, {0xa7, 0x31}, {0xa7, 0x0},
    {0xa7, 0x18}, {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x19}, {0xa7, 0x31},
    {0xa7, 0x0},  {0xa7, 0x18}, {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x19},
    {0xa7, 0x31}, {0xa7, 0x0},  {0xa7, 0x18}, {0x7f, 0x0},  {0xe5, 0x1f},
    {0xe1, 0x77}, {0xdd, 0x7f}, {0xc2, 0xe},

    {0xff, 0x0},  {0xe0, 0x4},  {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x3d},
    {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x0},  {0x54, 0x0},  {0x55, 0x88},
    {0x57, 0x0},

    {0x50, 0x92}, {0x5a, 0x50}, {0x5b, 0x3c}, {0x5c, 0x0},  {0xd3, 0x4},
    {0xe0, 0x0},

    {0xff, 0x0},  {0x5, 0x0},

    {0xda, 0x8},  {0xd7, 0x3},  {0xe0, 0x0},

    {0x5, 0x00},  {0xDA, 0x0},  {0x5A, 0x18}, {0x5B, 0x18},

    {0xff, 0xff},

};
const struct sensor_reg OV2640_QVGA[] = {
    {0xff, 0x0},  {0x2c, 0xff}, {0x2e, 0xdf}, {0xff, 0x1},  {0x3c, 0x32},
    {0x11, 0x0},  {0x9, 0x2},   {0x4, 0xa8},  {0x13, 0xe5}, {0x14, 0x48},
    {0x2c, 0xc},  {0x33, 0x78}, {0x3a, 0x33}, {0x3b, 0xfb}, {0x3e, 0x0},
    {0x43, 0x11}, {0x16, 0x10}, {0x39, 0x2},  {0x35, 0x88},

    {0x22, 0xa},  {0x37, 0x40}, {0x23, 0x0},  {0x34, 0xa0}, {0x6, 0x2},
    {0x6, 0x88},  {0x7, 0xc0},  {0xd, 0xb7},  {0xe, 0x1},   {0x4c, 0x0},
    {0x4a, 0x81}, {0x21, 0x99}, {0x24, 0x40}, {0x25, 0x38}, {0x26, 0x82},
    {0x5c, 0x0},  {0x63, 0x0},  {0x46, 0x22}, {0xc, 0x3a},  {0x5d, 0x55},
    {0x5e, 0x7d}, {0x5f, 0x7d}, {0x60, 0x55}, {0x61, 0x70}, {0x62, 0x80},
    {0x7c, 0x5},  {0x20, 0x80}, {0x28, 0x30}, {0x6c, 0x0},  {0x6d, 0x80},
    {0x6e, 0x0},  {0x70, 0x2},  {0x71, 0x94}, {0x73, 0xc1}, {0x3d, 0x34},
    {0x12, 0x4},  {0x5a, 0x57}, {0x4f, 0xbb}, {0x50, 0x9c}, {0xff, 0x0},
    {0xe5, 0x7f}, {0xf9, 0xc0}, {0x41, 0x24}, {0xe0, 0x14}, {0x76, 0xff},
    {0x33, 0xa0}, {0x42, 0x20}, {0x43, 0x18}, {0x4c, 0x0},  {0x87, 0xd0},
    {0x88, 0x3f}, {0xd7, 0x3},  {0xd9, 0x10}, {0xd3, 0x82}, {0xc8, 0x8},
    {0xc9, 0x80}, {0x7c, 0x0},  {0x7d, 0x0},  {0x7c, 0x3},  {0x7d, 0x48},
    {0x7d, 0x48}, {0x7c, 0x8},  {0x7d, 0x20}, {0x7d, 0x10}, {0x7d, 0xe},
    {0x90, 0x0},  {0x91, 0xe},  {0x91, 0x1a}, {0x91, 0x31}, {0x91, 0x5a},
    {0x91, 0x69}, {0x91, 0x75}, {0x91, 0x7e}, {0x91, 0x88}, {0x91, 0x8f},
    {0x91, 0x96}, {0x91, 0xa3}, {0x91, 0xaf}, {0x91, 0xc4}, {0x91, 0xd7},
    {0x91, 0xe8}, {0x91, 0x20}, {0x92, 0x0},

    {0x93, 0x6},  {0x93, 0xe3}, {0x93, 0x3},  {0x93, 0x3},  {0x93, 0x0},
    {0x93, 0x2},  {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},
    {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},  {0x96, 0x0},  {0x97, 0x8},
    {0x97, 0x19}, {0x97, 0x2},  {0x97, 0xc},  {0x97, 0x24}, {0x97, 0x30},
    {0x97, 0x28}, {0x97, 0x26}, {0x97, 0x2},  {0x97, 0x98}, {0x97, 0x80},
    {0x97, 0x0},  {0x97, 0x0},  {0xa4, 0x0},  {0xa8, 0x0},  {0xc5, 0x11},
    {0xc6, 0x51}, {0xbf, 0x80}, {0xc7, 0x10}, {0xb6, 0x66}, {0xb8, 0xa5},
    {0xb7, 0x64}, {0xb9, 0x7c}, {0xb3, 0xaf}, {0xb4, 0x97}, {0xb5, 0xff},
    {0xb0, 0xc5}, {0xb1, 0x94}, {0xb2, 0xf},  {0xc4, 0x5c}, {0xa6, 0x0},
    {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x1b}, {0xa7, 0x31}, {0xa7, 0x0},
    {0xa7, 0x18}, {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x19}, {0xa7, 0x31},
    {0xa7, 0x0},  {0xa7, 0x18}, {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x19},
    {0xa7, 0x31}, {0xa7, 0x0},  {0xa7, 0x18}, {0x7f, 0x0},  {0xe5, 0x1f},
    {0xe1, 0x77}, {0xdd, 0x7f}, {0xc2, 0xe},

    {0xff, 0x0},  {0xe0, 0x4},  {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x3d},
    {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x0},  {0x54, 0x0},  {0x55, 0x88},
    {0x57, 0x0},

    {0x50, 0x92}, {0x5a, 0x50}, {0x5b, 0x3c}, {0x5c, 0x0},  {0xd3, 0x4},
    {0xe0, 0x0},

    {0xff, 0x0},  {0x5, 0x0},

    {0xda, 0x8},  {0xd7, 0x3},  {0xe0, 0x0},

    {0x5, 0x0},

    {0xff, 0xff},
};

const struct sensor_reg OV2640_JPEG_INIT[] = {
    {0xff, 0x00}, {0x2c, 0xff}, {0x2e, 0xdf}, {0xff, 0x01}, {0x3c, 0x32},
    {0x11, 0x00}, {0x09, 0x02}, {0x04, 0x28}, {0x13, 0xe5}, {0x14, 0x48},
    {0x2c, 0x0c}, {0x33, 0x78}, {0x3a, 0x33}, {0x3b, 0xfB}, {0x3e, 0x00},
    {0x43, 0x11}, {0x16, 0x10}, {0x39, 0x92}, {0x35, 0xda}, {0x22, 0x1a},
    {0x37, 0xc3}, {0x23, 0x00}, {0x34, 0xc0}, {0x36, 0x1a}, {0x06, 0x88},
    {0x07, 0xc0}, {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00}, {0x48, 0x00},
    {0x5B, 0x00}, {0x42, 0x03}, {0x4a, 0x81}, {0x21, 0x99}, {0x24, 0x40},
    {0x25, 0x38}, {0x26, 0x82}, {0x5c, 0x00}, {0x63, 0x00}, {0x61, 0x70},
    {0x62, 0x80}, {0x7c, 0x05}, {0x20, 0x80}, {0x28, 0x30}, {0x6c, 0x00},
    {0x6d, 0x80}, {0x6e, 0x00}, {0x70, 0x02}, {0x71, 0x94}, {0x73, 0xc1},
    {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00}, {0x1a, 0x4b},
    {0x32, 0x09}, {0x37, 0xc0}, {0x4f, 0x60}, {0x50, 0xa8}, {0x6d, 0x00},
    {0x3d, 0x38}, {0x46, 0x3f}, {0x4f, 0x60}, {0x0c, 0x3c}, {0xff, 0x00},
    {0xe5, 0x7f}, {0xf9, 0xc0}, {0x41, 0x24}, {0xe0, 0x14}, {0x76, 0xff},
    {0x33, 0xa0}, {0x42, 0x20}, {0x43, 0x18}, {0x4c, 0x00}, {0x87, 0xd5},
    {0x88, 0x3f}, {0xd7, 0x03}, {0xd9, 0x10}, {0xd3, 0x82}, {0xc8, 0x08},
    {0xc9, 0x80}, {0x7c, 0x00}, {0x7d, 0x00}, {0x7c, 0x03}, {0x7d, 0x48},
    {0x7d, 0x48}, {0x7c, 0x08}, {0x7d, 0x20}, {0x7d, 0x10}, {0x7d, 0x0e},
    {0x90, 0x00}, {0x91, 0x0e}, {0x91, 0x1a}, {0x91, 0x31}, {0x91, 0x5a},
    {0x91, 0x69}, {0x91, 0x75}, {0x91, 0x7e}, {0x91, 0x88}, {0x91, 0x8f},
    {0x91, 0x96}, {0x91, 0xa3}, {0x91, 0xaf}, {0x91, 0xc4}, {0x91, 0xd7},
    {0x91, 0xe8}, {0x91, 0x20}, {0x92, 0x00}, {0x93, 0x06}, {0x93, 0xe3},
    {0x93, 0x05}, {0x93, 0x05}, {0x93, 0x00}, {0x93, 0x04}, {0x93, 0x00},
    {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00},
    {0x93, 0x00}, {0x96, 0x00}, {0x97, 0x08}, {0x97, 0x19}, {0x97, 0x02},
    {0x97, 0x0c}, {0x97, 0x24}, {0x97, 0x30}, {0x97, 0x28}, {0x97, 0x26},
    {0x97, 0x02}, {0x97, 0x98}, {0x97, 0x80}, {0x97, 0x00}, {0x97, 0x00},
    {0xc3, 0xed}, {0xa4, 0x00}, {0xa8, 0x00}, {0xc5, 0x11}, {0xc6, 0x51},
    {0xbf, 0x80}, {0xc7, 0x10}, {0xb6, 0x66}, {0xb8, 0xA5}, {0xb7, 0x64},
    {0xb9, 0x7C}, {0xb3, 0xaf}, {0xb4, 0x97}, {0xb5, 0xFF}, {0xb0, 0xC5},
    {0xb1, 0x94}, {0xb2, 0x0f}, {0xc4, 0x5c}, {0xc0, 0x64}, {0xc1, 0x4B},
    {0x8c, 0x00}, {0x86, 0x3D}, {0x50, 0x00}, {0x51, 0xC8}, {0x52, 0x96},
    {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x5a, 0xC8}, {0x5b, 0x96},
    {0x5c, 0x00}, {0xd3, 0x00}, //{ 0xd3, 0x7f },
    {0xc3, 0xed}, {0x7f, 0x00}, {0xda, 0x00}, {0xe5, 0x1f}, {0xe1, 0x67},
    {0xe0, 0x00}, {0xdd, 0x7f}, {0x05, 0x00},

    {0x12, 0x40}, {0xd3, 0x04}, //{ 0xd3, 0x7f },
    {0xc0, 0x16}, {0xC1, 0x12}, {0x8c, 0x00}, {0x86, 0x3d}, {0x50, 0x00},
    {0x51, 0x2C}, {0x52, 0x24}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00},
    {0x5A, 0x2c}, {0x5b, 0x24}, {0x5c, 0x00}, {0xff, 0xff},
};

const struct sensor_reg OV2640_YUV422[] = {
    {0xFF, 0x00}, {0x05, 0x00}, {0xDA, 0x10}, {0xD7, 0x03}, {0xDF, 0x00},
    {0x33, 0x80}, {0x3C, 0x40}, {0xe1, 0x77}, {0x00, 0x00}, {0xff, 0xff},
};

const struct sensor_reg OV2640_JPEG[] = {
    {0xe0, 0x14}, {0xe1, 0x77}, {0xe5, 0x1f}, {0xd7, 0x03}, {0xda, 0x10},
    {0xe0, 0x00}, {0xFF, 0x01}, {0x04, 0x08}, {0xff, 0xff},
};

/* JPG 160x120 */
const struct sensor_reg OV2640_160x120_JPEG[] = {
    {0xff, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1a, 0x4b}, {0x32, 0x09}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23},
    {0x6d, 0x00}, {0x39, 0x12}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3},
    {0x23, 0x00}, {0x34, 0xc0}, {0x36, 0x1a}, {0x06, 0x88}, {0x07, 0xc0},
    {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00}, {0xff, 0x00}, {0xe0, 0x04},
    {0xc0, 0x64}, {0xc1, 0x4b}, {0x86, 0x35}, {0x50, 0x92}, {0x51, 0xc8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5a, 0x28}, {0x5b, 0x1e}, {0x5c, 0x00}, {0xe0, 0x00}, {0xff, 0xff},
};

/* JPG, 0x176x144 */

const struct sensor_reg OV2640_176x144_JPEG[] = {
    {0xff, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1a, 0x4b}, {0x32, 0x09}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23},
    {0x6d, 0x00}, {0x39, 0x12}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3},
    {0x23, 0x00}, {0x34, 0xc0}, {0x36, 0x1a}, {0x06, 0x88}, {0x07, 0xc0},
    {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00}, {0xff, 0x00}, {0xe0, 0x04},
    {0xc0, 0x64}, {0xc1, 0x4b}, {0x86, 0x35}, {0x50, 0x92}, {0x51, 0xc8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5a, 0x2c}, {0x5b, 0x24}, {0x5c, 0x00}, {0xe0, 0x00}, {0xff, 0xff},
};

/* JPG 320x240 */

const struct sensor_reg OV2640_320x240_JPEG[] = {
    {0xff, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1a, 0x4b}, {0x32, 0x09}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23},
    {0x6d, 0x00}, {0x39, 0x12}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3},
    {0x23, 0x00}, {0x34, 0xc0}, {0x36, 0x1a}, {0x06, 0x88}, {0x07, 0xc0},
    {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00}, {0xff, 0x00}, {0xe0, 0x04},
    {0xc0, 0x64}, {0xc1, 0x4b}, {0x86, 0x35}, {0x50, 0x89}, {0x51, 0xc8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5a, 0x50}, {0x5b, 0x3c}, {0x5c, 0x00}, {0xe0, 0x00}, {0xff, 0xff},
};

/* JPG 352x288 */

const struct sensor_reg OV2640_352x288_JPEG[] = {
        {0xff, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
        {0x1a, 0x4b}, {0x32, 0x09}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23},
        {0x6d, 0x00}, {0x39, 0x12}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3},
        {0x23, 0x00}, {0x34, 0xc0}, {0x36, 0x1a}, {0x06, 0x88}, {0x07, 0xc0},
        {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00}, {0xff, 0x00}, {0xe0, 0x04},
        {0xc0, 0x64}, {0xc1, 0x4b}, {0x86, 0x35}, {0x50, 0x89}, {0x51, 0xc8},
        {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
        {0x5a, 0x58}, {0x5b, 0x48}, {0x5c, 0x00}, {0xe0, 0x00}, {0xff, 0xff},
};

/* JPG 640x480 */
const struct sensor_reg OV2640_640x480_JPEG[] = {
    {0xff, 0x01}, {0x11, 0x01}, {0x12, 0x00}, // Bit[6:4]: Resolution
                                              // selection//0x02Ϊ����
    {0x17, 0x11},                             // HREFST[10:3]
    {0x18, 0x75},                             // HREFEND[10:3]
    {0x32, 0x36}, // Bit[5:3]: HREFEND[2:0]; Bit[2:0]: HREFST[2:0]
    {0x19, 0x01}, // VSTRT[9:2]
    {0x1a, 0x97}, // VEND[9:2]
    {0x03, 0x0f}, // Bit[3:2]: VEND[1:0]; Bit[1:0]: VSTRT[1:0]
    {0x37, 0x40}, {0x4f, 0xbb}, {0x50, 0x9c}, {0x5a, 0x57}, {0x6d, 0x80},
    {0x3d, 0x34}, {0x39, 0x02}, {0x35, 0x88}, {0x22, 0x0a}, {0x37, 0x40},
    {0x34, 0xa0}, {0x06, 0x02}, {0x0d, 0xb7}, {0x0e, 0x01},

    {0xff, 0x00}, {0xe0, 0x04}, {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x3d},
    {0x50, 0x89}, {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x00}, {0x54, 0x00},
    {0x55, 0x88}, {0x57, 0x00}, {0x5a, 0xa0}, {0x5b, 0x78}, {0x5c, 0x00},
    {0xd3, 0x04}, {0xe0, 0x00},

    {0xff, 0xff},
};

/* JPG 800x600 */
const struct sensor_reg OV2640_800x600_JPEG[] = {
    {0xff, 0x01}, {0x11, 0x01}, {0x12, 0x00}, // Bit[6:4]: Resolution
                                              // selection//0x02Ϊ����
    {0x17, 0x11},                             // HREFST[10:3]
    {0x18, 0x75},                             // HREFEND[10:3]
    {0x32, 0x36}, // Bit[5:3]: HREFEND[2:0]; Bit[2:0]: HREFST[2:0]
    {0x19, 0x01}, // VSTRT[9:2]
    {0x1a, 0x97}, // VEND[9:2]
    {0x03, 0x0f}, // Bit[3:2]: VEND[1:0]; Bit[1:0]: VSTRT[1:0]
    {0x37, 0x40}, {0x4f, 0xbb}, {0x50, 0x9c}, {0x5a, 0x57}, {0x6d, 0x80},
    {0x3d, 0x34}, {0x39, 0x02}, {0x35, 0x88}, {0x22, 0x0a}, {0x37, 0x40},
    {0x34, 0xa0}, {0x06, 0x02}, {0x0d, 0xb7}, {0x0e, 0x01},

    {0xff, 0x00}, {0xe0, 0x04}, {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x35},
    {0x50, 0x89}, {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x00}, {0x54, 0x00},
    {0x55, 0x88}, {0x57, 0x00}, {0x5a, 0xc8}, {0x5b, 0x96}, {0x5c, 0x00},
    {0xd3, 0x02}, {0xe0, 0x00},

    {0xff, 0xff},
};

/* JPG 1024x768 */
const struct sensor_reg OV2640_1024x768_JPEG[] = {
    {0xff, 0x01}, {0x11, 0x01}, {0x12, 0x00}, // Bit[6:4]: Resolution
                                              // selection//0x02Ϊ����
    {0x17, 0x11},                             // HREFST[10:3]
    {0x18, 0x75},                             // HREFEND[10:3]
    {0x32, 0x36}, // Bit[5:3]: HREFEND[2:0]; Bit[2:0]: HREFST[2:0]
    {0x19, 0x01}, // VSTRT[9:2]
    {0x1a, 0x97}, // VEND[9:2]
    {0x03, 0x0f}, // Bit[3:2]: VEND[1:0]; Bit[1:0]: VSTRT[1:0]
    {0x37, 0x40}, {0x4f, 0xbb}, {0x50, 0x9c}, {0x5a, 0x57}, {0x6d, 0x80},
    {0x3d, 0x34}, {0x39, 0x02}, {0x35, 0x88}, {0x22, 0x0a}, {0x37, 0x40},
    {0x34, 0xa0}, {0x06, 0x02}, {0x0d, 0xb7}, {0x0e, 0x01},

    {0xff, 0x00}, {0xc0, 0xC8}, {0xc1, 0x96}, {0x8c, 0x00}, {0x86, 0x3D},
    {0x50, 0x00}, {0x51, 0x90}, {0x52, 0x2C}, {0x53, 0x00}, {0x54, 0x00},
    {0x55, 0x88}, {0x5a, 0x00}, {0x5b, 0xC0}, {0x5c, 0x01}, {0xd3, 0x02},

    {0xff, 0xff},
};

/* JPG 1280x1024 */
const struct sensor_reg OV2640_1280x1024_JPEG[] = {
    {0xff, 0x01}, {0x11, 0x01}, {0x12, 0x00}, // Bit[6:4]: Resolution
                                              // selection//0x02Ϊ����
    {0x17, 0x11},                             // HREFST[10:3]
    {0x18, 0x75},                             // HREFEND[10:3]
    {0x32, 0x36}, // Bit[5:3]: HREFEND[2:0]; Bit[2:0]: HREFST[2:0]
    {0x19, 0x01}, // VSTRT[9:2]
    {0x1a, 0x97}, // VEND[9:2]
    {0x03, 0x0f}, // Bit[3:2]: VEND[1:0]; Bit[1:0]: VSTRT[1:0]
    {0x37, 0x40}, {0x4f, 0xbb}, {0x50, 0x9c}, {0x5a, 0x57}, {0x6d, 0x80},
    {0x3d, 0x34}, {0x39, 0x02}, {0x35, 0x88}, {0x22, 0x0a}, {0x37, 0x40},
    {0x34, 0xa0}, {0x06, 0x02}, {0x0d, 0xb7}, {0x0e, 0x01},

    {0xff, 0x00}, {0xe0, 0x04}, {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x3d},
    {0x50, 0x00}, {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x00}, {0x54, 0x00},
    {0x55, 0x88}, {0x57, 0x00}, {0x5a, 0x40}, {0x5b, 0xf0}, {0x5c, 0x01},
    {0xd3, 0x02}, {0xe0, 0x00},

    {0xff, 0xff},
};

/* JPG 1600x1200 */
const struct sensor_reg OV2640_1600x1200_JPEG[] = {
    {0xff, 0x01}, {0x11, 0x01}, {0x12, 0x00}, // Bit[6:4]: Resolution
                                              // selection//0x02Ϊ����
    {0x17, 0x11},                             // HREFST[10:3]
    {0x18, 0x75},                             // HREFEND[10:3]
    {0x32, 0x36}, // Bit[5:3]: HREFEND[2:0]; Bit[2:0]: HREFST[2:0]
    {0x19, 0x01}, // VSTRT[9:2]
    {0x1a, 0x97}, // VEND[9:2]
    {0x03, 0x0f}, // Bit[3:2]: VEND[1:0]; Bit[1:0]: VSTRT[1:0]
    {0x37, 0x40}, {0x4f, 0xbb}, {0x50, 0x9c}, {0x5a, 0x57},
    {0x6d, 0x80}, {0x3d, 0x34}, {0x39, 0x02}, {0x35, 0x88},
    {0x22, 0x0a}, {0x37, 0x40}, {0x34, 0xa0}, {0x06, 0x02},
    {0x0d, 0xb7}, {0x0e, 0x01},

    {0xff, 0x00}, {0xe0, 0x04}, {0xc0, 0xc8}, {0xc1, 0x96},
    {0x86, 0x3d}, {0x50, 0x00}, {0x51, 0x90}, {0x52, 0x2c},
    {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x88}, {0x57, 0x00},
    {0x5a, 0x90}, {0x5b, 0x2C}, {0x5c, 0x05}, // bit2->1;bit[1:0]->1
    {0xd3, 0x02}, {0xe0, 0x00},

    {0xff, 0xff},
};

// *****************************************************************************
// Public code

void arducam_system_init(void) {
    // All of this is handled by SYS_Initialize() and Hamony 3
    // // This example will use I2C0 on GPIO4 (SDA) and GPIO5 (SCL)
    // bsp_arducam_i2c_init(I2C_PORT, 100 * 1000);
    // gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    // gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    // gpio_pull_up(PIN_SDA);
    // gpio_pull_up(PIN_SCL);
    // // Make the I2C pins available to picotool
    // bi_decl(bi_2pins_with_func(PIN_SDA, PIN_SCL, GPIO_FUNC_I2C));
    // // This example will use SPI0 at 0.5MHz.
    // spi_init(SPI_PORT, 8 * 1000 * 1000);
    // gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    // gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    // gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    // // Chip select is active-low, so we'll initialise it to a driven-high
    // state gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT); gpio_put(PIN_CS,
    // 1);
}

uint8_t arducam_bus_detect(void) {
    write_reg(0x00, 0x55);
    if (read_reg(0x00) == 0x55) {
        printf("\nSPI bus normal");
        return 0;
    } else {
        printf("\nSPI bus error");
        return 1;
    }
}

uint8_t arducam_camera_probe(void) {
    uint8_t id_H, id_L;
    rdSensorReg8_8(0x0A, &id_H);
    rdSensorReg8_8(0x0B, &id_L);
    printf("\narducam_camera_probe: id_H:id_L = 0x%02x:%02x", id_H, id_L);
    if (id_H == 0x26 && (id_L == 0x40 || id_L == 0x41 || id_L == 0x42)) {
        printf("\nov2640 detected");
        return 0;
    } else {
        printf("\nCan't find ov2640 sensor");
        return 1;
    }
}

void arducam_camera_init(arducam_fmt_t format) {
    switch (format) {
    case ARDUCAM_FMT_JPEG: {
        wrSensorReg8_8(0xff, 0x01);
        wrSensorReg8_8(0x12, 0x80);
        wrSensorRegs8_8(OV2640_JPEG_INIT);
        wrSensorRegs8_8(OV2640_YUV422);
        wrSensorRegs8_8(OV2640_JPEG);
        wrSensorReg8_8(0xff, 0x01);
        wrSensorReg8_8(0x15, 0x00);
        wrSensorRegs8_8(OV2640_320x240_JPEG);
        break;
    }
    case ARDUCAM_FMT_RGB565: {
        wrSensorReg8_8(0xff, 0x01);
        wrSensorReg8_8(0x12, 0x80);
        bsp_arducam_sleep_ms(100);
        wrSensorRegs8_8(OV2640_QVGA);
        break;
    }
    case ARDUCAM_FMT_YUV: {
        wrSensorReg8_8(0xff, 0x01);
        wrSensorReg8_8(0x12, 0x80);
        bsp_arducam_sleep_ms(100);
        wrSensorRegs8_8(OV2640_YUV_96x96);
        break;
    }
    }

    // Flush the FIFO
    flush_fifo();
    // Start capture
    start_capture();
}

void arducam_set_jpeg_size(arducam_res_t resolution) {
    switch (resolution) {
    case res_160x120:
        wrSensorRegs8_8(OV2640_160x120_JPEG);
        break;
    case res_176x144:
        wrSensorRegs8_8(OV2640_176x144_JPEG);
        break;
    case res_320x240:
        wrSensorRegs8_8(OV2640_320x240_JPEG);
        break;
    case res_352x288:
        wrSensorRegs8_8(OV2640_352x288_JPEG);
        break;
    case res_640x480:
        wrSensorRegs8_8(OV2640_640x480_JPEG);
        break;
    case res_800x600:
        wrSensorRegs8_8(OV2640_800x600_JPEG);
        break;
    case res_1024x768:
        wrSensorRegs8_8(OV2640_1024x768_JPEG);
        break;
    case res_1280x1024:
        wrSensorRegs8_8(OV2640_1280x1024_JPEG);
        break;
    case res_1600x1200:
        wrSensorRegs8_8(OV2640_1600x1200_JPEG);
        break;
    default:
        wrSensorRegs8_8(OV2640_320x240_JPEG);
        break;
    }
}

void arducam_capture(uint8_t *imageDat) {
    uint8_t yuv_data[96 * 96 * 2 + 8]; // = 18432 + 8
    uint16_t index = 0;
    // wait for a full image to be read
    while (!get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
      asm("nop");
    }
    int length = read_fifo_length();
    // ASSERT(length == 96*96*2);

    // printf("the data length: %d\r\n", length);
    bsp_arducam_spi_cs_enable();
    set_fifo_burst(); // Set fifo burst mode
    bsp_arducam_spi_read(SPI_PORT, BURST_FIFO_READ, yuv_data, length);
    bsp_arducam_spi_cs_disable();

    // Flush the FIFO
    flush_fifo();
    // Start capture
    start_capture();
    for (uint16_t i = 0; i < length - 8; i += 2) {
        // extract even number bytes from the yuv data.  I *think* this is the
        // Y component (brightness or luminance), which is usually all that's
        // needed for image processing.
        imageDat[index++] = yuv_data[i];
    }
}

void arducam_capture_single(void) {
    uint8_t value[1024 * 40];
    // Flush the FIFO
    flush_fifo();
    // Start capture
    start_capture();
    while (!get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
        ;
    }
    int length = read_fifo_length();
    bsp_arducam_spi_cs_enable();
    set_fifo_burst(); // Set fifo burst mode
    bsp_arducam_spi_read(SPI_PORT, BURST_FIFO_READ, value, length);
    // insert callback here.
    // bsp_arducam_uart_write(UART_ID, value, length);
    bsp_arducam_spi_cs_disable();
}

// *****************************************************************************
// Private (static) codeSPI_PORT

static void rdSensorReg8_8(uint8_t regID, uint8_t *regDat) {
    bsp_arducam_i2c_write(I2C_PORT, I2C_ADDR, &regID, 1, true);
    bsp_arducam_i2c_read(I2C_PORT, I2C_ADDR, regDat, 1, false);
}

static void wrSensorReg8_8(uint8_t regID, uint8_t regDat) {
    uint8_t buf[2];
    buf[0] = regID;
    buf[1] = regDat;
    bsp_arducam_i2c_write(I2C_PORT, I2C_ADDR, buf, 2, true);
}

static void write_reg(uint8_t address, uint8_t value) {
    uint8_t buf[2];
    buf[0] = address | WRITE_BIT; // remove read bit as this is a write
    buf[1] = value;
    bsp_arducam_spi_cs_enable();
    bsp_arducam_spi_write(SPI_PORT, buf, 2);
    bsp_arducam_spi_cs_disable();
    bsp_arducam_sleep_ms(10);
}

static uint8_t read_reg(uint8_t address) {
    uint8_t value = 0;
    address = address & 0x7f;
    bsp_arducam_spi_cs_enable();
    bsp_arducam_spi_write(SPI_PORT, &address, 1);
    bsp_arducam_sleep_ms(10);
    bsp_arducam_spi_read(SPI_PORT, 0, &value, 1);
    bsp_arducam_spi_cs_disable();
    bsp_arducam_sleep_ms(10);
    return value;
}

static int wrSensorRegs8_8(const struct sensor_reg reglist[]) {
    int err = 0;
    unsigned int reg_addr = 0;
    unsigned int reg_val = 0;
    const struct sensor_reg *next = reglist;
    while ((reg_addr != 0xff) | (reg_val != 0xff)) {
        reg_addr = next->reg;
        reg_val = next->val;
        // TODO: wrSensorReg8_8 should return error value
        wrSensorReg8_8(reg_addr, reg_val);
        bsp_arducam_sleep_ms(10);
        next++;
    }
    return err;
}

// static unsigned char read_fifo(void)
// {
//     unsigned char data;
//     data = read_reg(SINGLE_FIFO_READ);
//     return data;
// }

static void set_fifo_burst(void) {
    uint8_t value;
    bsp_arducam_spi_read(SPI_PORT, BURST_FIFO_READ, &value, 1);
}

static void flush_fifo(void) { write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK); }

static void start_capture(void) { write_reg(ARDUCHIP_FIFO, FIFO_START_MASK); }

// static void clear_fifo_flag(void )
// {
//     write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
// }

static unsigned int read_fifo_length() {
    unsigned int len1, len2, len3, len = 0;
    len1 = read_reg(FIFO_SIZE1);
    len2 = read_reg(FIFO_SIZE2);
    len3 = read_reg(FIFO_SIZE3) & 0x7f;
    len = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;
    return len;
}

// Set corresponding bit
static void set_bit(unsigned char addr, unsigned char bit) {
    unsigned char temp;
    temp = read_reg(addr);
    write_reg(addr, temp | bit);
}
// Clear corresponding bit
static void clear_bit(unsigned char addr, unsigned char bit) {
    unsigned char temp;
    temp = read_reg(addr);
    write_reg(addr, temp & (~bit));
}

// Get corresponding bit status
static unsigned char get_bit(unsigned char addr, unsigned char bit) {
    unsigned char temp;
    temp = read_reg(addr);
    temp = temp & bit;
    return temp;
}
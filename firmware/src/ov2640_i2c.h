/**
 * @file ov2640_i2c.h
 *
 * MIT License
 *
 * Copyright (c) 2023 BrainChip, Inc
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
 */

/**
 * @brief Read and write registers in the OV2640 camera via I2C operations.
 *
 * Note The OV2640 has two banks of SPI registers, here called
 * OV2640_I2C_DSP_BANK and OV2640_I2C_SENSOR_BANK.  OV2640_I2C_DSP_BANK is
 * selected by writing a 0 to register address 0xff, OV2640_I2C_SENSOR_BANK is
 * selected by writing a 1 to register address 0xff.
 *
 * The I2C read and write routines here track which bank is in effect and emit
 * bank switching commands as needed.  Registers in the OV2640_I2C_SENSOR_BANK
 * are defined with the 0x100 bit turned on; this is stripped off before a read
 * or write operation.
 */

#ifndef _OV2640_SPI_H_
#define _OV2640_SPI_H_

// *****************************************************************************
// Includes

#include "definitions.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// *****************************************************************************
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

typedef struct {
	uint8_t addr;
	uint8_t data;
} ov2640_i2c_pair_t;

typedef enum {
	OV2640_I2C_DSP_BANK,
	OV2640_I2C_SENSOR_BANK
} ov2640_i2c_bank_t;

// Currently unused, but these macros define which bank the register is in.
#define OV2640_I2C_DSP_BANK(reg) (reg)
#define OV2640_I2C_SENSOR_BANK(reg) (reg)

// clang-format off
#define OV2640_I2C_R_BYPASS OV2640_I2C_DSP_BANK(0x05)
#define OV2640_I2C_QUANTIZE_SCALE_FACTOR OV2640_I2C_DSP_BANK(0x44)
#define OV2640_I2C_CTRLI OV2640_I2C_DSP_BANK(0x50)
#define OV2640_I2C_HSIZE OV2640_I2C_DSP_BANK(0x51)
#define OV2640_I2C_VSIZE OV2640_I2C_DSP_BANK(0x52)
#define OV2640_I2C_XOFFL OV2640_I2C_DSP_BANK(0x53)
#define OV2640_I2C_YOFFL OV2640_I2C_DSP_BANK(0x54)
#define OV2640_I2C_VHYX OV2640_I2C_DSP_BANK(0x55)
#define OV2640_I2C_DPRP OV2640_I2C_DSP_BANK(0x56)
#define OV2640_I2C_TEST OV2640_I2C_DSP_BANK(0x57)
#define OV2640_I2C_ZMOW OV2640_I2C_DSP_BANK(0x5a)
#define OV2640_I2C_ZMOH OV2640_I2C_DSP_BANK(0x5b)
#define OV2640_I2C_ZMHH OV2640_I2C_DSP_BANK(0x5c)
#define OV2640_I2C_BPADDR OV2640_I2C_DSP_BANK(0x7c)
#define OV2640_I2C_BPDATA OV2640_I2C_DSP_BANK(0x7d)
#define OV2640_I2C_CTRL2 OV2640_I2C_DSP_BANK(0x86)
#define OV2640_I2C_CTRL3 OV2640_I2C_DSP_BANK(0x87)
#define OV2640_I2C_SIZEL OV2640_I2C_DSP_BANK(0x8c)
#define OV2640_I2C_HSIZE8 OV2640_I2C_DSP_BANK(0xc0)
#define OV2640_I2C_VSIZE8 OV2640_I2C_DSP_BANK(0xc1)
#define OV2640_I2C_CTRL0 OV2640_I2C_DSP_BANK(0xc2)
#define OV2640_I2C_CTRL1 OV2640_I2C_DSP_BANK(0xc3)
#define OV2640_I2C_R_DVP_SP OV2640_I2C_DSP_BANK(0xd3)
#define OV2640_I2C_IMAGE_MODE OV2640_I2C_DSP_BANK(0xda)
#define OV2640_I2C_RESET OV2640_I2C_DSP_BANK(0xe0)
#define OV2640_I2C_MS_SP OV2640_I2C_DSP_BANK(0xf0)
#define OV2640_I2C_SS_ID OV2640_I2C_DSP_BANK(0xf7)
#define OV2640_I2C_SS_CTRL OV2640_I2C_DSP_BANK(0xf8)
#define OV2640_I2C_MC_BIST OV2640_I2C_DSP_BANK(0xf9)
#define OV2640_I2C_MC_AL OV2640_I2C_DSP_BANK(0xfa)
#define OV2640_I2C_MC_AH OV2640_I2C_DSP_BANK(0xfb)
#define OV2640_I2C_MC_D OV2640_I2C_DSP_BANK(0xfc)
#define OV2640_I2C_P_CMD OV2640_I2C_DSP_BANK(0xfd)
#define OV2640_I2C_P_STATUS OV2640_I2C_DSP_BANK(0xfe)
#define OV2640_I2C_RA_DLMT OV2640_I2C_DSP_BANK(0xff)

#define OV2640_I2C_GAIN OV2640_I2C_SENSOR_BANK(0x00)
#define OV2640_I2C_COM1 OV2640_I2C_SENSOR_BANK(0x03)
#define OV2640_I2C_REG04 OV2640_I2C_SENSOR_BANK(0x04)
#define OV2640_I2C_REG08 OV2640_I2C_SENSOR_BANK(0x08)
#define OV2640_I2C_COM2 OV2640_I2C_SENSOR_BANK(0x09)
#define OV2640_I2C_PIDH OV2640_I2C_SENSOR_BANK(0x0a)
#define OV2640_I2C_PIDL OV2640_I2C_SENSOR_BANK(0x0b)
#define OV2640_I2C_COM3 OV2640_I2C_SENSOR_BANK(0x0c)
#define OV2640_I2C_COM4 OV2640_I2C_SENSOR_BANK(0x0d)
#define OV2640_I2C_AEC OV2640_I2C_SENSOR_BANK(0x10)
#define OV2640_I2C_CLKRC OV2640_I2C_SENSOR_BANK(0x11)
#define OV2640_I2C_COM7 OV2640_I2C_SENSOR_BANK(0x12)
#define OV2640_I2C_COM8 OV2640_I2C_SENSOR_BANK(0x13)
#define OV2640_I2C_COM9 OV2640_I2C_SENSOR_BANK(0x14)
#define OV2640_I2C_COM10 OV2640_I2C_SENSOR_BANK(0x15)
#define OV2640_I2C_HREFST OV2640_I2C_SENSOR_BANK(0x17)
#define OV2640_I2C_HREFEND OV2640_I2C_SENSOR_BANK(0x18)
#define OV2640_I2C_VSTRT OV2640_I2C_SENSOR_BANK(0x19)
#define OV2640_I2C_VEND OV2640_I2C_SENSOR_BANK(0x1a)
#define OV2640_I2C_MIDH OV2640_I2C_SENSOR_BANK(0x1c)
#define OV2640_I2C_MIDL OV2640_I2C_SENSOR_BANK(0x1d)
#define OV2640_I2C_AEW OV2640_I2C_SENSOR_BANK(0x24)
#define OV2640_I2C_AEB OV2640_I2C_SENSOR_BANK(0x25)
#define OV2640_I2C_VV OV2640_I2C_SENSOR_BANK(0x26)
#define OV2640_I2C_REG2A OV2640_I2C_SENSOR_BANK(0x2a)
#define OV2640_I2C_FRARL OV2640_I2C_SENSOR_BANK(0x2b)
#define OV2640_I2C_ADDVSL OV2640_I2C_SENSOR_BANK(0x2d)
#define OV2640_I2C_ADDVSH OV2640_I2C_SENSOR_BANK(0x2e)
#define OV2640_I2C_YAVG OV2640_I2C_SENSOR_BANK(0x2f)
#define OV2640_I2C_HSDY OV2640_I2C_SENSOR_BANK(0x30)
#define OV2640_I2C_HEDY OV2640_I2C_SENSOR_BANK(0x31)
#define OV2640_I2C_REG32 OV2640_I2C_SENSOR_BANK(0x32)
#define OV2640_I2C_ARCOM2 OV2640_I2C_SENSOR_BANK(0x34)
#define OV2640_I2C_REG45 OV2640_I2C_SENSOR_BANK(0x45)
#define OV2640_I2C_FLL OV2640_I2C_SENSOR_BANK(0x46)
#define OV2640_I2C_FLH OV2640_I2C_SENSOR_BANK(0x47)
#define OV2640_I2C_COM19 OV2640_I2C_SENSOR_BANK(0x48)
#define OV2640_I2C_ZOOMS OV2640_I2C_SENSOR_BANK(0x49)
#define OV2640_I2C_COM22 OV2640_I2C_SENSOR_BANK(0x4b)
#define OV2640_I2C_COM25 OV2640_I2C_SENSOR_BANK(0x4e)
#define OV2640_I2C_BD50 OV2640_I2C_SENSOR_BANK(0x4f)
#define OV2640_I2C_BD60 OV2640_I2C_SENSOR_BANK(0x50)
#define OV2640_I2C_REG5D OV2640_I2C_SENSOR_BANK(0x5d)
#define OV2640_I2C_REG5E OV2640_I2C_SENSOR_BANK(0x5e)
#define OV2640_I2C_REG5F OV2640_I2C_SENSOR_BANK(0x5f)
#define OV2640_I2C_REG60 OV2640_I2C_SENSOR_BANK(0x60)
#define OV2640_I2C_HISTO_LOW OV2640_I2C_SENSOR_BANK(0x61)
#define OV2640_I2C_HISTO_HIGH OV2640_I2C_SENSOR_BANK(0x62)
// clang-format on

// *****************************************************************************
// Public declarations

/**
 * @brief One-time initialization of ov2640_i2c module.
 */
void ov2640_i2c_init(DRV_HANDLE i2c_handle);

bool ov2640_i2c_read_byte(uint8_t addr, uint8_t *data);
bool ov2640_i2c_write_byte(uint8_t addr, uint8_t data);
bool ov2640_i2c_write_pairs(const ov2640_i2c_pair_t *pairs, size_t count);

bool ov2640_i2c_select_bank(ov2640_i2c_bank_t bank);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _OV2640_SPI_H_ */

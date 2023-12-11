/**
 * @file ov2640_i2c.c
 *
 * MIT License
 *
 * Copyright (c) 2023 BrainChip, Inc.
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

// *****************************************************************************
// Includes

#include "ov2640_i2c.h"

#include <stdbool.h>
#include <stdint.h>

// *****************************************************************************
// Private types and definitions

#define OV2640_I2C_ADDR (0x60 >> 1)

// *****************************************************************************
// Private (static, forward) declarations

// *****************************************************************************
// Private (static) storage

static DRV_HANDLE s_i2c_handle;

// *****************************************************************************
// Public code

void ov2640_i2c_init(DRV_HANDLE i2c_handle) {
	s_i2c_handle = i2c_handle;
}

bool ov2640_i2c_read_byte(uint8_t addr, uint8_t *data) {
	bool success = DRV_I2C_WriteReadTransfer(s_i2c_handle, OV2640_I2C_ADDR,
                                     (void *)&addr, sizeof(addr),
                                     (void *const)data, sizeof(uint8_t));
    printf("# ov2640_i2c_read_byte(%02x, %02x) => %d.  handle = %d\r\n", addr, *data, success, s_i2c_handle);
	return success;
}

bool ov2640_i2c_write_byte(uint8_t addr, uint8_t data) {
    uint8_t tx_buf[] = {addr, data};
    bool success = DRV_I2C_WriteTransfer(s_i2c_handle, OV2640_I2C_ADDR, (void *)tx_buf,
                                 sizeof(tx_buf));
    printf("# ov2640_i2c_write_byte(%02x, %02x) => %d.  handle = %d\r\n", addr, data, success, s_i2c_handle);
    return success;
}

bool ov2640_i2c_write_pairs(const ov2640_i2c_pair_t *pairs, size_t count) {
	for (int i=0; i<count; i++) {
		const ov2640_i2c_pair_t *pair = &pairs[i];
		if (!ov2640_i2c_write_byte(pair->addr, pair->data)) {
			return false;
		}
	}
    return true;
}

bool ov2640_i2c_select_bank(ov2640_i2c_bank_t bank) {
	// writing a 0 to address 0xff selects "DSP" bank,
	// writing a 1 selects "SENSOR" bank
	return ov2640_i2c_write_byte(0xff, (bank == OV2640_I2C_DSP_BANK ? 0 : 1));
}

// *****************************************************************************
// Private (static) code

// *****************************************************************************
// End of file


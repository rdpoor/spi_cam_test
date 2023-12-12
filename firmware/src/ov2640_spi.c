/**
 * @file arducam.c
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

// *****************************************************************************
// Includes

#include "ov2640_spi.h"

#include "definitions.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// *****************************************************************************
// Private types and definitions

// MSB in byte 0 signifies a write operation
#define WRITE_OP 0x80

// *****************************************************************************
// Private (static) storage

// *****************************************************************************
// Private (static, forward) declarations

/**
 * Write bytes to SPI bus and read back values.  rx_buf may be NULL, and rx_size
 * may be zero.
 *
 * Return true on a successful operation.
 */
static bool spi_xfer(void *tx_buf, size_t tx_size, void *rx_buf,
                     size_t rx_size);

// *****************************************************************************
// Public code

bool ov2640_spi_read_byte(uint8_t addr, uint8_t *data) {
    uint8_t rx_buf[2];
    if (!spi_xfer(&addr, sizeof(addr), rx_buf, sizeof(rx_buf))) {
        return false;
    }
    *data = rx_buf[1];
    return true;
}

bool ov2640_spi_write_byte(uint8_t addr, uint8_t data) {
    uint8_t tx_buf[] = {addr | WRITE_OP, data};
    return spi_xfer(tx_buf, sizeof(tx_buf), NULL, 0);
}

bool ov2640_spi_read_bytes(uint8_t command, uint8_t *rx_buf, size_t rx_buflen) {
    uint8_t tx_buf[] = {command};
    return spi_xfer(tx_buf, sizeof(tx_buf), (void *)rx_buf, rx_buflen);
}

bool ov2640_spi_set_bit(uint8_t addr, uint8_t bitmask) {
    uint8_t data;
    if (!ov2640_spi_read_byte(addr, &data)) {
        return false;
    } else if (!ov2640_spi_write_byte(addr, data | bitmask)) {
        return false;
    }
    return true;
}

bool ov2640_spi_clear_bit(uint8_t addr, uint8_t bitmask) {
    uint8_t data;
    if (!ov2640_spi_read_byte(addr, &data)) {
        return false;
    } else if (!ov2640_spi_write_byte(addr, data & ~bitmask)) {
        return false;
    }
    return true;
}

bool ov2640_spi_test_bit(uint8_t addr, uint8_t bitmask, bool *value) {
    uint8_t data;
    if (!ov2640_spi_read_byte(addr, &data)) {
        return false;
    }
    SYSTICK_DelayUs(10);
    *value = (data & bitmask) ? true : false;
    return true;
}

// *****************************************************************************
// Private (static) code

static bool spi_xfer(void *tx_buf, size_t tx_size, void *rx_buf,
                     size_t rx_size) {
    while (SPI0_IsTransmitterBusy()) {
        asm("nop");
    }
    return SPI0_WriteRead(tx_buf, tx_size, rx_buf, rx_size);
}

// *****************************************************************************
// End of file

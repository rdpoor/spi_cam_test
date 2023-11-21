/**
 * @file bsp_arducam.c
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

#include "bsp_arducam.h"

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>

// *****************************************************************************
// Private types and definitions

// *****************************************************************************
// Private (static, forward) declarations

// *****************************************************************************
// Private (static) storage

// *****************************************************************************
// Public code

void bsp_arducam_init(void) {
    // already performed in config/default/initialization.c
    asm("nop");
}

// *****************************************************************************
// SPI

void bsp_arducam_spi_cs_enable(void) { SPI0_CS__Clear(); }

void bsp_arducam_spi_cs_disable(void) { SPI0_CS__Set(); }

bool bsp_arducam_spi_read_byte(uint8_t addr, uint8_t *val) {
    return bsp_arducam_spi_xfer(&addr, 1, val, 1);
}

bool bsp_arducam_spi_write_byte(uint8_t addr, uint8_t val) {
    uint8_t txd[] = {addr, val};
    return bsp_arducam_spi_xfer(txd, 2, NULL, 0);
}

bool bsp_arducam_spi_xfer(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    bool ret;
    // TODO: check len argument
    bsp_arducam_spi_cs_enable();
    ret = SPI0_WriteRead((uint8_t *)src, src_len, dst, dst_len);
    bsp_arducam_spi_cs_disable();
    return ret;
}

// *****************************************************************************
// I2C

unsigned int bsp_arducam_i2c_init(void *i2c, unsigned int baudrate) {
    // already performed in config/default/initialization.c
    asm("nop");
    return 0;
}

bool bsp_arducam_i2c_read_byte(const DRV_HANDLE handle, uint8_t addr,
                               uint8_t *val) {
    uint8_t rxd[4];
    bool ret = DRV_I2C_ReadTransfer(handle, addr, rxd, sizeof(rxd));
    *val = rxd[1];
    return ret;
}

bool bsp_arducam_i2c_write_byte(const DRV_HANDLE handle, uint8_t addr,
                                uint8_t val) {
    return DRV_I2C_WriteTransfer(handle, addr, &val, 1);
}

bool bsp_arducam_i2c_xfer(const DRV_HANDLE handle, uint16_t address,
                          const uint8_t *src, const size_t src_len,
                          uint8_t *dst, const size_t dst_len) {
    return DRV_I2C_WriteReadTransfer(handle, address, (uint8_t *)src, src_len,
                                     dst, dst_len);
}

// *****************************************************************************
// Other

void bsp_arducam_sleep_ms(int ms) {
    SYS_TIME_HANDLE timer = SYS_TIME_HANDLE_INVALID;

    SYS_TIME_DelayMS(100, &timer);
    while (SYS_TIME_DelayIsComplete(timer) == false) {
        asm("nop");
    }
}

// *****************************************************************************
// Private (static) code

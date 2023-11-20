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

void bsp_arducam_spi_cs_enable(void) {
    SPI0_CS__Clear();
}

void bsp_arducam_spi_cs_disable(void) {
    SPI0_CS__Set();
}

// int bsp_arducam_spi_xfer(void *spi, const uint8_t *src, uint8_t *dst,
//                          size_t len) {
//     // TODO: check len argument
//     return !SPI0_WriteRead(src, len, dst, len);
// }

int bsp_arducam_spi_read(void *spi, uint8_t repeated_tx_data, uint8_t *dst,
                         size_t len) {
    return !SPI0_WriteRead(NULL, 0, dst, len);
}

int bsp_arducam_spi_write(void *spi, const uint8_t *src, size_t len) {
    return !SPI0_WriteRead((void *)src, len, NULL, 0);
}

unsigned int bsp_arducam_i2c_init(void *i2c, unsigned int baudrate) {
    // already performed in config/default/initialization.c
    asm("nop");
    return 0;
}

int bsp_arducam_i2c_write(void *i2c, uint8_t addr, const uint8_t *src,
                          size_t len, bool nostop) {
    // TODO: honor nostop?
    return !TWIHS0_Write(addr, (uint8_t *)src, len);
}

int bsp_arducam_i2c_read(void *i2c, uint8_t addr, uint8_t *dst, size_t len,
                         bool nostop) {
    // TODO: honor nostop?
    return !TWIHS0_Read(addr, dst, len);
}

void bsp_arducam_sleep_ms(int ms) {
    SYS_TIME_HANDLE timer = SYS_TIME_HANDLE_INVALID;

    SYS_TIME_DelayMS(100, &timer);
    while (SYS_TIME_DelayIsComplete(timer) == false) {
        asm("nop");
    }
}

// *****************************************************************************
// Private (static) code

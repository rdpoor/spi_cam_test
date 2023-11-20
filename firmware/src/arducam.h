/**
 * @file arducam.h
 *
 * MIT License
 *
 * Copyright (c) 2023 R. Dunbar Poor
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
 * @brief Platform specific functions for the Arducam OV2460 SPI-based camera.
 */

#ifndef _ARDUCAM_H_
#define _ARDUCAM_H_

// *****************************************************************************
// Includes

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

typedef enum {
    ARDUCAM_FMT_JPEG,
    ARDUCAM_FMT_RGB565,
    ARDUCAM_FMT_YUV,
} arducam_fmt_t;

typedef enum {
    res_160x120,   // 160x120
    res_176x144,   // 176x144
    res_320x240,   // 320x240
    res_352x288,   // 352x288
    res_640x480,   // 640x480
    res_800x600,   // 800x600
    res_1024x768,  // 1024x768
    res_1280x1024, // 1280x1024
    res_1600x1200, // 1600x1200
} arducam_res_t;

// /*i2c pin source */
// #define I2C_PORT (void *)0
// #define PIN_SDA 8
// #define PIN_SCL 9

// #define UART_ID (void *)0
// #define BAUD_RATE 921600
// #define DATA_BITS 8
// #define STOP_BITS 1
// #define PARITY UART_PARITY_NONE
// #define UART_TX_PIN 0
// #define UART_RX_PIN 1

// struct sensor_info {
//     uint8_t sensor_slave_address;
//     uint8_t address_size;
//     uint8_t data_size;
//     uint16_t sensor_id;
// };

// *****************************************************************************
// Public declarations

void arducam_system_init(void);

uint8_t arducam_bus_detect(void);

uint8_t arducam_camera_probe(void);

void arducam_camera_init(arducam_fmt_t format);

void arducam_set_jpeg_size(arducam_res_t resolution);;

void arducam_capture(uint8_t *imageDat);

void arducam_capture_single(void);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _ARDUCAM_H_ */

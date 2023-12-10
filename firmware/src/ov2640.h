/**
 * @file ov2640.h
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
 * @brief Interface to the OV2640 camera via the I2C bus
 */

#ifndef _OV2640_H_
#define _OV2640_H_

// *****************************************************************************
// Includes

#include <stdbool.h>

// *****************************************************************************
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

typedef enum {
    OV2640_FORMAT_YUV,
    OV2640_FORMAT_JPEG,
} ov2640_format_t;

// *****************************************************************************
// Public declarations

void ov2640_init(void);
void ov2640_step(void);

/**
 * @brief Probe I2C bus to verify the VID and PID of the camera.
 *
 * Note: This is an asynchronous call -- poll ov2640_succeeded() and
 * ov2640_had_error() until one of them returns true.
 *
 * @return true if the probe process started.
 */
bool ov2640_probe_i2c(void);

/**
 * @brief Set the format of the camera.
 *
 * NOTE: This can only be called after a successful return from ov2640_probe_i2c
 *
 * Note: This is an asynchronous call -- poll ov2640_succeeded() and
 * ov2640_had_error() until one of them returns true.
 */
bool ov2640_set_format(ov2640_format_t format);

bool ov2640_succeeded(void);
bool ov2640_had_error(void);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _OV2640_H_ */

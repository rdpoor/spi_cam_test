/**
 * @file cam_data_task.h
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
 * @brief Interface to the ARDUCAM camera via the SPI bus (data channel)
 */

#ifndef _CAM_DATA_TASK_H_
#define _CAM_DATA_TASK_H_

// *****************************************************************************
// Includes

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// *****************************************************************************
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

// *****************************************************************************
// Public declarations

/**
 * @brief one-time initialization, to be called at startup.
 *
 * Pass in two equal sized buffer to receive image data (double buffered).
 */
void cam_data_task_init(uint8_t *yuv_buf_a, uint8_t *yuv_buf_b, size_t buflen);

/**
 * @brief Run the cam_data_task state machine.  Call repeatedly from the
 * main superloop.
 */
void cam_data_task_step(void);

/**
 * @brief Initiate a probe on the SPI bus to check for a response.
 *
 * After calling this, poll cam_data_task_succeeded() and
 * cam_data_task_had_error() until one of them returns true.
 *
 * Returns true if the call succeeded.
 */
bool cam_data_task_probe_spi(void);

/**
 * @brief Initiate a setup of camera data.
 *
 * After calling this, poll cam_data_task_succeeded() and
 * cam_data_task_had_error() until one of them returns true.
 *
 * Returns true if the call succeeded.
 */
bool cam_data_task_setup_camera(void);

/**
 * @brief Initiate continuous image capture, writing data to the given buffer.
 *
 * TODO: cam_data_task should provide double buffering.
 */
bool cam_data_task_start_capture(void);

/**
 * @brief Following any async operation above, call cam_data_task_succeeded()
 * and cam_data_task_had_error() until either of them returns true.  Otherwise
 * remain in the current state and continue calling cam_data_task_step()
 */
bool cam_data_task_succeeded(void);
bool cam_data_task_had_error(void);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _CAM_DATA_TASK_H_ */

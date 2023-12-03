/**
 * @file app_ardu_cam.h
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
 * @brief read images from an OV2640 camera via SPI bus operations.
 */

#ifndef _APP_ARDU_CAM_H_
#define _APP_ARDU_CAM_H_

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

/**
 * @brief APP_ARDU_CAM_Initialize initializes the ARDU_CAM task state.  It gets
 * called from SYS_Initialize()
 */

void APP_ARDU_CAM_Initialize(void);

/**
 * @brief Run the state machine for the ARDU_CAM task.  It is called repeatedly\
 * from SYS_Tasks()
 */
void APP_ARDU_CAM_Tasks(void);

/**
 * @brief Return true if the ARDU_CAM task has completed SPI initialization.
 */
bool APP_ARDU_CAM_Task_SPIIsReady(void);

/**
 * @brief Return true if ARDU_CAM task enountered an error.
 */
bool APP_ARDU_CAM_Task_Failed(void);


// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _APP_ARDU_CAM_H_ */

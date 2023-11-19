/**
 * @file camera.h
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
 * @brief Definitions for a generic video camera
 */

#ifndef _CAMERA_H_
#define _CAMERA_H_

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

#define CAMERA_R160x120 0x00 // QQVGA Resolution
#define CAMERA_R320x240 0x01 // QVGA Resolution
#define CAMERA_R480x272 0x02 // 480x272 Resolution
#define CAMERA_R640x480 0x03 // VGA Resolution

#define CAMERA_CONTRAST_BRIGHTNESS 0x00 // Camera contrast brightness features
#define CAMERA_BLACK_WHITE 0x01         // Camera black white feature
#define CAMERA_COLOR_EFFECT 0x03        // Camera color effect feature

#define CAMERA_BRIGHTNESS_LEVEL0 0x00 // Brightness level -2
#define CAMERA_BRIGHTNESS_LEVEL1 0x01 // Brightness level -1
#define CAMERA_BRIGHTNESS_LEVEL2 0x02 // Brightness level 0
#define CAMERA_BRIGHTNESS_LEVEL3 0x03 // Brightness level +1
#define CAMERA_BRIGHTNESS_LEVEL4 0x04 // Brightness level +2

#define CAMERA_CONTRAST_LEVEL0 0x05 // Contrast level -2
#define CAMERA_CONTRAST_LEVEL1 0x06 // Contrast level -1
#define CAMERA_CONTRAST_LEVEL2 0x07 // Contrast level  0
#define CAMERA_CONTRAST_LEVEL3 0x08 // Contrast level +1
#define CAMERA_CONTRAST_LEVEL4 0x09 // Contrast level +2

#define CAMERA_BLACK_WHITE_BW 0x00          // Black and white effect
#define CAMERA_BLACK_WHITE_NEGATIVE 0x01    // Negative effect
#define CAMERA_BLACK_WHITE_BW_NEGATIVE 0x02 // BW and Negative effect
#define CAMERA_BLACK_WHITE_NORMAL 0x03      // Normal effect

#define CAMERA_COLOR_EFFECT_NONE 0x00    // No effects
#define CAMERA_COLOR_EFFECT_BLUE 0x01    // Blue effect
#define CAMERA_COLOR_EFFECT_GREEN 0x02   // Green effect
#define CAMERA_COLOR_EFFECT_RED 0x03     // Red effect
#define CAMERA_COLOR_EFFECT_ANTIQUE 0x04 // Antique effect

// *****************************************************************************
// Public declarations

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif // #ifndef _CAMERA_H_

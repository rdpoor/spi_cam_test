/**
 * @file app.h
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
 * @brief Main code for reading ArduCam OV2640 camera.
 */

#ifndef _APP_H
#define _APP_H

// *****************************************************************************
// Includes

// *****************************************************************************
// C++ compatibility

#ifdef __cplusplus // C++ Compatibility
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

#define APP_VERSION "0.0.2"

/**
 * @brief Initialize the application.  Called once at startup.
 */
void APP_Initialize(void);

/**
 * @brief Run the main application's state machine, called frequently from
 * the main loop.
 */
void APP_Tasks(void);

// __attribute__((format(printf, 1, 2))) _Noreturn
// void APP_panic(const char *format, ...);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _APP_H */

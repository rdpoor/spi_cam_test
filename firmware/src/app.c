/**
 * @file app.c
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

#include "app.h"
#include "arducam.h"
#include "definitions.h"
#include "ov2640.h"
#include <stdarg.h>
#include <stdio.h>

// *****************************************************************************
// Private types and definitions

#define IMAGE_WIDTH 96
#define IMAGE_HEIGHT 96
#define YUV_DEPTH 2
#define RGB_DEPTH 3

// The camera fifo generates an extra 8 bytes (not sure why)
#define YUV_BUFFER_SIZE ((IMAGE_WIDTH * IMAGE_HEIGHT * YUV_DEPTH) + 8)
#define RGB_BUFFER_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT * RGB_DEPTH)

typedef enum {
    APP_STATE_INIT,
    APP_STATE_START_PROBE_I2C,
    APP_STATE_AWAIT_PROBE_I2C,
    APP_STATE_START_PROBE_SPI,
    APP_STATE_AWAIT_PROBE_SPI,
    APP_STATE_PROBE_COMPLETE,
    APP_STATE_START_CONFIGURE_CAMERA,
    APP_STATE_AWAIT_CONFIGURE_CAMERA,
    APP_STATE_START_CAPTURE_IMAGE,
    APP_STATE_AWAIT_CAPTURE_IMAGE,
    APP_STATE_LOAD_IMAGE,
    APP_STATE_CONVERT_YUV_TO_RGB,
    APP_STATE_EMIT_RGB,
    APP_STATE_SUCCESS,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    app_state_t state;      // current application state
    uint32_t timestamp_sys; // for capturing Frames Per Second
} app_ctx_t;

// *****************************************************************************
// Private (static) storage

/**
 * @buffer to hold YUV data captured from camera
 */
static uint8_t s_yuv_buf[YUV_BUFFER_SIZE];

/**
 * @buffer to hold RGB data converted from YUV
 */
static uint8_t s_rgb_buf[RGB_BUFFER_SIZE];

static app_ctx_t s_app;

// *****************************************************************************
// Private (static, forward) declarations

/**
 * @brief Convert the YUV pixels in s_yuv_buf to RGB pixels in s_rgb_buf.
 *
 * Note that this reads four bytes at a time (y0, u, y1, v) and writes six
 * (r0, g0, b0, r1, g1, b1)
 */
static void convert_yuv_to_rgb(void);

static uint8_t clamp(float v);

// *****************************************************************************
// Public code

void APP_Initialize(void) {
    printf("\n# =========================="
           "\n# ArduCam OV2460 Test v%s\r\n",
           APP_VERSION);
    s_app.state = APP_STATE_INIT;
    ov2640_init();
    arducam_init();
}

void APP_Tasks(void) {
    ov2640_step();
    arducam_step();

    switch (s_app.state) {

    case APP_STATE_INIT: {
        s_app.state = APP_STATE_START_PROBE_I2C;
    } break;

    case APP_STATE_START_PROBE_I2C: {
        // Make sure we can communicate with the camera via I2C (for control)
        ov2640_probe_i2c();
        s_app.state = APP_STATE_AWAIT_PROBE_I2C;
    } break;

    case APP_STATE_AWAIT_PROBE_I2C: {
        if (ov2640_succeeded()) {
            s_app.state = APP_STATE_START_PROBE_SPI;
        } else if (ov2640_had_error()) {
            printf("# Probe for OV2640 failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_PROBE_I2C;
        }
    } break;

    case APP_STATE_START_PROBE_SPI: {
        // Make sure we can communicate with the camera via SPI (for data)
        arducam_probe_spi();
        s_app.state = APP_STATE_AWAIT_PROBE_SPI;
    } break;

    case APP_STATE_AWAIT_PROBE_SPI: {
        if (arducam_succeeded()) {
            s_app.state = APP_STATE_PROBE_COMPLETE;
        } else if (arducam_had_error()) {
            printf("# Probe for ArduCam failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_PROBE_SPI;
        }
    } break;

    case APP_STATE_PROBE_COMPLETE: {
        // Here, both I2C and SPI operations have been tested and verified
        printf("# ArduCam detected\r\n");
        s_app.state = APP_STATE_START_CONFIGURE_CAMERA;
    } break;

    case APP_STATE_START_CONFIGURE_CAMERA: {
        // Configure the camera to capture YUV 96 x 96
        if (!ov2640_set_format(OV2640_FORMAT_YUV)) {
            printf("# ov2640_configure(OV2640_MODE_YUV) failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CONFIGURE_CAMERA;
        }
        break;
    }

    case APP_STATE_AWAIT_CONFIGURE_CAMERA: {
        if (ov2640_succeeded()) {
            printf("# Cconfigured OV2640 for YUV mode\r\n");
            s_app.state = APP_STATE_START_CAPTURE_IMAGE;
            break;
        } else if (ov2640_had_error()) {
            printf("# Could not configure OV2640 for YUV mode\r\n");
            s_app.state = APP_STATE_ERROR;
            break;
        } else {
            // remain in this state until configuration completes
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_CONFIGURE_CAMERA;
            break;
        }
    }

    case APP_STATE_START_CAPTURE_IMAGE: {
        // Capture YUV 96 x 96
        if (!arducam_start_capture()) {
            printf("# arducam_start_capture() YUV failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CAPTURE_IMAGE;
        }
        break;
    }

    case APP_STATE_AWAIT_CAPTURE_IMAGE: {
        // Check if capture is complete
        if (arducam_succeeded()) {
            s_app.state = APP_STATE_LOAD_IMAGE;
        } else if (arducam_had_error()) {
            printf("# Arducam capture of YUV failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // remain in this state until configuration completes
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_CAPTURE_IMAGE;
        }
    } break;

    case APP_STATE_LOAD_IMAGE: {
        if (arducam_read_fifo(s_yuv_buf, sizeof(s_yuv_buf))) {
            s_app.state = APP_STATE_CONVERT_YUV_TO_RGB;
        } else {
            printf("# Unable to read YUV bytes\r\n");
            s_app.state = APP_STATE_ERROR;
        }
    } break;

    case APP_STATE_CONVERT_YUV_TO_RGB: {
        convert_yuv_to_rgb();
        s_app.state = APP_STATE_EMIT_RGB;
    } break;

    case APP_STATE_EMIT_RGB: {
        // STUB
        s_app.state = APP_STATE_SUCCESS;
    } break;

    case APP_STATE_SUCCESS: {
        // Success.  Compute FPS and loop back to capture another frame
        uint32_t now_sys = SYS_TIME_CounterGet();
        uint32_t dt_us = SYS_TIME_CountToUS(now_sys - s_app.timestamp_sys);
        s_app.timestamp_sys = now_sys;
        printf("# FPS: %f\n", 1000000.0 / dt_us);
        // printf("."); fflush(stdout);
        s_app.state = APP_STATE_START_CAPTURE_IMAGE;
    } break;

    case APP_STATE_ERROR: {
        // Unrecoverable error.  Stop.
    }

    } // switch
}

// *****************************************************************************
// Private (static) code

static void convert_yuv_to_rgb(void) {
    uint8_t *yuv = s_yuv_buf;
    uint8_t *rgb = s_rgb_buf;

    for (int i=0; i<sizeof(s_yuv_buf); i+=4) {
        // read 4 bytes: [y0, u, y1, v]
        uint8_t y0 = *yuv++;
        uint8_t u = *yuv++;
        uint8_t y1 = *yuv++;
        uint8_t v = *yuv++;

        // emit six bytes: [r0, g0, b0, r1, g1, b1]
        *rgb++ = clamp(y0 + 1.4075 * (v - 128));
        *rgb++ = clamp(y0 - 0.3455 * (u - 128) - (0.7169 * (v - 128)));
        *rgb++ = clamp(y0 + 1.7790 * (u - 128));
        *rgb++ = clamp(y1 + 1.4075 * (v - 128));
        *rgb++ = clamp(y1 - 0.3455 * (u - 128) - (0.7169 * (v - 128)));
        *rgb++ = clamp(y1 + 1.7790 * (u - 128));
    }
}

static uint8_t clamp(float v) {
    if (v < 0.0) {
        return 0;
    } else if (v > 255.0) {
        return 255;
    } else {
        return v;
    }
}

/*******************************************************************************
 End of File
 */

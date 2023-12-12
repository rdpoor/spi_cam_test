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
#include "cam_ctrl_task.h"
#include "cam_data_task.h"
#include "definitions.h"
#include "ov2640_i2c.h"
#include "ov2640_spi.h"
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
    APP_STATE_START_RESET_CAMERA,
    APP_STATE_AWAIT_RESET_CAMERA,
    APP_STATE_START_PROBE_SPI,
    APP_STATE_AWAIT_PROBE_SPI,
    APP_STATE_START_PROBE_I2C,
    APP_STATE_AWAIT_PROBE_I2C,
    APP_STATE_START_SETUP_CAMERA_CONTROL,
    APP_STATE_AWAIT_SETUP_CAMERA_CONTROL,
    APP_STATE_START_SETUP_CAMERA_DATA,
    APP_STATE_AWAIT_SETUP_CAMERA_DATA,
    APP_STATE_CAMERA_READY,
    APP_STATE_START_CAPTURE_IMAGE,
    APP_STATE_AWAIT_CAPTURE_IMAGE,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    app_state_t state;         // current application state
    DRV_HANDLE i2c_drv_handle; // handle for I2C interface
    uint32_t timestamp_sys;    // for capturing Frames Per Second
} app_ctx_t;

// *****************************************************************************
// Private (static) storage

/**
 * @buffer to hold YUV data captured from camera
 */
static uint8_t s_buf_a[YUV_BUFFER_SIZE];
static uint8_t s_buf_b[YUV_BUFFER_SIZE];

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
__attribute__((unused)) static void convert_yuv_to_rgb(uint8_t *yuv_buf);

__attribute__((unused)) static uint8_t clamp(float v);

// *****************************************************************************
// Public code

void APP_Initialize(void) {
    printf("\n# =========================="
           "\n# ArduCam OV2640 Test v%s\r\n",
           APP_VERSION);
    s_app.state = APP_STATE_INIT;
    s_app.i2c_drv_handle = DRV_HANDLE_INVALID;
    cam_ctrl_task_init();
    cam_data_task_init(s_buf_a, s_buf_b, YUV_BUFFER_SIZE);
}

void APP_Tasks(void) {
    if (s_app.i2c_drv_handle != DRV_HANDLE_INVALID) {
        // only run sub-tasks if I2C driver is open
        cam_ctrl_task_step();
        cam_data_task_step();
    }

    switch (s_app.state) {

    case APP_STATE_INIT: {
        s_app.i2c_drv_handle =
            DRV_I2C_Open(DRV_I2C_INDEX_0, DRV_IO_INTENT_READWRITE);
        if (s_app.i2c_drv_handle != DRV_HANDLE_INVALID) {
            ov2640_i2c_init(s_app.i2c_drv_handle);
            s_app.state = APP_STATE_START_RESET_CAMERA;
        } else {
            // remain in this state until open succeeds
        }
    } break;

    case APP_STATE_START_RESET_CAMERA: {
        if (!cam_ctrl_reset_camera()) {
            printf("# Failed to initiate camera reset\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_RESET_CAMERA;
        }
    } break;

    case APP_STATE_AWAIT_RESET_CAMERA: {
        if (cam_ctrl_task_succeeded()) {
            s_app.state = APP_STATE_START_PROBE_I2C;
        } else if (cam_ctrl_task_had_error()) {
            printf("# Reset ArduCam failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_RESET_CAMERA;
        }
    } break;

    case APP_STATE_START_PROBE_SPI: {
        if (!cam_data_task_probe_spi()) {
            printf("# Call to probe spi bus failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_PROBE_SPI;
        }
    } break;

    case APP_STATE_AWAIT_PROBE_SPI: {
        if (cam_data_task_succeeded()) {
            s_app.state = APP_STATE_START_PROBE_I2C;
        } else if (cam_data_task_had_error()) {
            printf("# Probe spi bus failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_PROBE_SPI;
        }
    } break;

    case APP_STATE_START_PROBE_I2C: {
        // Make sure we can communicate with the camera via I2C (for control)
        printf("# ==== APP_STATE_START_PROBE_I2C 1\r\n");
        if (!cam_ctrl_task_probe_i2c()) {
            printf("# Call to probe I2C failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            printf("# ==== APP_STATE_START_PROBE_I2C 2\r\n");
            s_app.state = APP_STATE_AWAIT_PROBE_I2C;
        }
    } break;

    case APP_STATE_AWAIT_PROBE_I2C: {
        printf("# ==== APP_STATE_AWAIT_PROBE_I2C 1, state = %d\r\n", s_app.state);
        if (cam_ctrl_task_succeeded()) {
            printf("# ==== Probe for OV2640 succeeded\r\n");
            s_app.state = APP_STATE_START_SETUP_CAMERA_CONTROL;
        } else if (cam_ctrl_task_had_error()) {
            printf("# Probe for OV2640 failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_PROBE_I2C;
        }
    } break;

    case APP_STATE_START_SETUP_CAMERA_CONTROL: {
        printf("# ==== APP_STATE_START_SETUP_CAMERA_CONTROL 1\r\n");
        if (!cam_ctrl_task_setup_camera()) {
            printf("# Call to setup camera failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            printf("# ==== APP_STATE_START_SETUP_CAMERA_CONTROL 2\r\n");
            s_app.state = APP_STATE_AWAIT_SETUP_CAMERA_CONTROL;
        }
    } break;

    case APP_STATE_AWAIT_SETUP_CAMERA_CONTROL: {
        if (cam_ctrl_task_succeeded()) {
            printf("# ==== APP_STATE_AWAIT_SETUP_CAMERA_CONTROL 1\r\n");
            s_app.state = APP_STATE_START_SETUP_CAMERA_DATA;
        } else if (cam_ctrl_task_had_error()) {
            printf("# Setup of camera control failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_START_SETUP_CAMERA_DATA;
        }
    } break;

    case APP_STATE_START_SETUP_CAMERA_DATA: {
        printf("# ==== APP_STATE_START_SETUP_CAMERA_DATA 1\r\n");
        if (!cam_data_task_setup_camera()) {
            printf("# Call to setup camera data failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            printf("# ==== APP_STATE_START_SETUP_CAMERA_DATA 2\r\n");
            s_app.state = APP_STATE_AWAIT_SETUP_CAMERA_DATA;
        }
    } break;

    case APP_STATE_AWAIT_SETUP_CAMERA_DATA: {
        if (cam_data_task_succeeded()) {
            printf("# ==== APP_STATE_AWAIT_SETUP_CAMERA_DATA 1\r\n");
            s_app.state = APP_STATE_CAMERA_READY;
        } else if (cam_data_task_had_error()) {
            printf("# Setup camera bus failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // probe still pending -- remain in this state
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_SETUP_CAMERA_DATA;
        }
    } break;

    case APP_STATE_CAMERA_READY: {
        // Camera is initialized and ready to start capturing.
        // Here, both I2C and SPI operations have been tested and verified
        printf("# ArduCam ready\r\n");
        s_app.state = APP_STATE_START_CAPTURE_IMAGE;
    } break;

    case APP_STATE_START_CAPTURE_IMAGE: {
        // Capture YUV 96 x 96
        if (!cam_data_task_start_capture()) {
            printf("# failed to start capture\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CAPTURE_IMAGE;
        }
        break;
    }

    case APP_STATE_AWAIT_CAPTURE_IMAGE: {
        // cam_data_task will loop itself -- remain in this state...
        s_app.state = APP_STATE_AWAIT_CAPTURE_IMAGE;
    } break;

    case APP_STATE_ERROR: {
        // Unrecoverable error.  Stop.
    }

    } // switch
}

// *****************************************************************************
// Private (static) code

static inline uint8_t clamp(float v) {
    if (v < 0.0) {
        return 0;
    } else if (v > 255.0) {
        return 255;
    } else {
        return v;
    }
}

static void convert_yuv_to_rgb(uint8_t *yuv_buf) {
    uint8_t *yuv = yuv_buf;
    uint8_t *rgb = s_rgb_buf;

    for (int i=0; i<YUV_BUFFER_SIZE; i+=4) {
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

/*******************************************************************************
 End of File
 */

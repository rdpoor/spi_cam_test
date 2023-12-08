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

#define YUV_WIDTH 96
#define YUV_HEIGHT 96
#define YUV_DEPTH 2

typedef enum {
    APP_STATE_INIT,
    APP_STATE_START_PROBE_I2C,
    APP_STATE_AWAIT_PROBE_I2C,
    APP_STATE_START_PROBE_SPI,
    APP_STATE_AWAIT_PROBE_SPI,
    APP_STATE_PROBE_COMPLETE,
    APP_STATE_START_CONFIGURE_YUV,
    APP_STATE_AWAIT_CONFIGURE_YUV,
    APP_STATE_START_CAPTURE_YUV,
    APP_STATE_AWAIT_CAPTURE_YUV,
    APP_STATE_LOAD_YUV,
    APP_STATE_START_EMIT_YUV,
    APP_STATE_AWAIT_EMIT_YUV,
    APP_STATE_START_CONFIGURE_JPG,
    APP_STATE_AWAIT_CONFIGURE_JPG,
    APP_STATE_START_CAPTURE_JPG,
    APP_STATE_AWAIT_CAPTURE_JPG,
    APP_STATE_START_EMIT_JPG,
    APP_STATE_AWAIT_EMIT_JPG,
    APP_STATE_SUCCESS,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    app_state_t state;      // current application state
    uint32_t timestamp_sys; // for capturing Frames Per Second
} app_ctx_t;

// *****************************************************************************
// Private (static) storage

static uint8_t s_yuv_buf[YUV_WIDTH * YUV_HEIGHT * YUV_DEPTH];

static app_ctx_t s_app;

// *****************************************************************************
// Private (static, forward) declarations

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
        s_app.state = APP_STATE_START_CONFIGURE_YUV;
    } break;

    case APP_STATE_START_CONFIGURE_YUV: {
        // Configure the camera to capture YUV 96 x 96
        if (!ov2640_set_format(OV2640_FORMAT_YUV)) {
            printf("# ov2640_configure(OV2640_MODE_YUV) failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CONFIGURE_YUV;
        }
    } break;

    case APP_STATE_AWAIT_CONFIGURE_YUV: {
        if (ov2640_succeeded()) {
            s_app.state = APP_STATE_START_CAPTURE_YUV;
        } else if (ov2640_had_error()) {
            printf("# Could not configure OV2640 for YUV mode");
            s_app.state = APP_STATE_ERROR;
        } else {
            // remain in this state until configuration completes
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_CONFIGURE_YUV;
        }
    } break;

    case APP_STATE_START_CAPTURE_YUV: {
        // Capture YUV 96 x 96
        if (!arducam_start_capture()) {
            printf("# arducam_start_capture() YUV failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CAPTURE_YUV;
        }
    } break;

    case APP_STATE_AWAIT_CAPTURE_YUV: {
        // Check if capture is complete
        if (arducam_succeeded()) {
            s_app.state = APP_STATE_LOAD_YUV;
        } else if (arducam_had_error()) {
            printf("# Arducam capture of YUV failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // remain in this state until configuration completes
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_CAPTURE_YUV;
        }
    } break;

    case APP_STATE_LOAD_YUV: {
        // Read the YUV 96 x 96 image into in-memory buffer
        uint32_t n_bytes = arducam_read_fifo_length();
        if (arducam_read_fifo(s_yuv_buf, sizeof(s_yuv_buf), n_bytes)) {
            s_app.state = APP_STATE_START_EMIT_YUV;
        } else {
            printf("# Unable to read %ld YUV bytes\r\n", n_bytes);
            s_app.state = APP_STATE_ERROR;
        }
    } break;

    case APP_STATE_START_EMIT_YUV: {
        // Send the YUV 96 x 96 image to the inference engine
        // STUB
        s_app.state = APP_STATE_AWAIT_EMIT_YUV;
    } break;

    case APP_STATE_AWAIT_EMIT_YUV: {
        // Have we finished sending the image to the inference engine?
        // NOTE: this can be overlapped with other operations...
        // STUB
        s_app.state = APP_STATE_START_CONFIGURE_JPG;
    } break;

    case APP_STATE_START_CONFIGURE_JPG: {
        // Configure the camera to capture JPEG
        if (!ov2640_set_format(OV2640_FORMAT_JPEG)) {
            printf("# ov2640_configure(OV2640_MODE_JPEG) failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CONFIGURE_JPG;
        }
    } break;

    case APP_STATE_AWAIT_CONFIGURE_JPG: {
        if (ov2640_succeeded()) {
            s_app.state = APP_STATE_START_CAPTURE_JPG;
        } else if (ov2640_had_error()) {
            printf("# Could not configure OV2640 for YUV mode");
            s_app.state = APP_STATE_ERROR;
        } else {
            // remain in this state until configuration completes
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_CONFIGURE_JPG;
        }
    } break;

    case APP_STATE_START_CAPTURE_JPG: {
        // Capture JPEG image
        if (!arducam_start_capture()) {
            printf("# arducam_start_capture() JPG failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_AWAIT_CAPTURE_JPG;
        }
    } break;

    case APP_STATE_AWAIT_CAPTURE_JPG: {
        // Check if capture is complete
        if (arducam_succeeded()) {
            s_app.state = APP_STATE_START_EMIT_JPG;
        } else if (arducam_had_error()) {
            printf("# Arducam capture of JPG failed\r\n");
            s_app.state = APP_STATE_ERROR;
        } else {
            // remain in this state until configuration completes
            // TODO: add timeout?
            s_app.state = APP_STATE_AWAIT_CAPTURE_JPG;
        }
    } break;

    case APP_STATE_START_EMIT_JPG: {
        // Send the JPG image to the host computer for previewing
        // STUB
        s_app.state = APP_STATE_AWAIT_EMIT_JPG;
    } break;

    case APP_STATE_AWAIT_EMIT_JPG: {
        // Send the JPG image to the host computer for previewing
        // STUB
        s_app.state = APP_STATE_SUCCESS;
    } break;

    case APP_STATE_SUCCESS: {
        // Success.  Compute FPS and loop back to capture another frame
        uint32_t now_sys = SYS_TIME_CounterGet();
        uint32_t dt_us = SYS_TIME_CountToUS(now_sys - s_app.timestamp_sys);
        s_app.timestamp_sys = now_sys;
        printf("# FPS: %f\n", 1000000.0 / dt_us);

        s_app.state = APP_STATE_START_CONFIGURE_YUV;
    } break;

    case APP_STATE_ERROR: {
        // Unrecoverable error.  Stop.
    }

    } // switch
}

// __attribute__((format(printf, 1, 2))) _Noreturn void
// APP_panic(const char *format, ...) {
//     va_list args;
//
//     va_start(args, format);
//     vprintf(format, args);
//     va_end(args);
//     // TODO: if needed, fflush(stdout) before going into infinite loop
//     while (1) {
//         asm("nop");
//     }
// }

/*******************************************************************************
 End of File
 */

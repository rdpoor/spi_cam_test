/**
 * @file cam_ctrl_task.c
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

#include "cam_ctrl_task.h"
#include "ov2640_i2c.h"
#include "definitions.h"
#include <stdarg.h>
#include <stdio.h>

// *****************************************************************************
// Private types and definitions

#define MAX_RETRY_COUNT 5
#define RESET_HOLDOFF_MS 100
#define RETRY_DELAY_MS 100
#define I2C_OP_HOLDOFF_MS 100

#define CAM_CTRL_TASK_I2C_ADDR (0x60 >> 1)

#define CAM_CTRL_TASK_CHIPID_HIGH 0x0A
#define CAM_CTRL_TASK_CHIPID_LOW 0x0B
#define CAM_CTRL_TASK_DEV_CTRL_REG 0xFF
#define CAM_CTRL_TASK_DEV_CTRL_REG_COM7 0x12
#define CAM_CTRL_TASK_DEV_CTRL_REG_COM10 0x15

/**
 * @brief cam_ctrl_task states.
 */
typedef enum {
    CAM_CTRL_TASK_STATE_INIT,
    CAM_CTRL_TASK_STATE_START_ASSERT_RESET,
    CAM_CTRL_TASK_STATE_AWAIT_ASSERT_RESET,
    CAM_CTRL_TASK_STATE_START_DEASSERT_RESET,
    CAM_CTRL_TASK_STATE_AWAIT_DEASSERT_RESET,
    CAM_CTRL_TASK_STATE_CHECK_VID_PID,
    CAM_CTRL_TASK_STATE_RETRY_WAIT,
    CAM_CTRL_TASK_STATE_START_FORMAT_RESET,
    CAM_CTRL_TASK_STATE_AWAIT_FORMAT_RESET,
    CAM_CTRL_TASK_STATE_FORMAT_LOAD,
    CAM_CTRL_TASK_STATE_SUCCESS,
    CAM_CTRL_TASK_STATE_ERROR,
} cam_ctrl_task_state_t;

typedef struct {
    cam_ctrl_task_state_t state; // current state
    SYS_TIME_HANDLE delay;       // general delay timer
    int retry_count;             // retry chip id
} cam_ctrl_task_ctx_t;

// *****************************************************************************
// Private (static) storage

#if 0
static const ov2640_i2c_pair_t CAM_CTRL_TASK_YUV_96x96[] = {
    {0xff, 0x00}, {0x2c, 0xff}, {0x2e, 0xdf}, {0xff, 0x01}, {0x3c, 0x32},
    {0x11, 0x00}, {0x09, 0x02}, {0x04, 0xa8}, {0x13, 0xe5}, {0x14, 0x48},
    {0x2c, 0x0c}, {0x33, 0x78}, {0x3a, 0x33}, {0x3b, 0xfb}, {0x3e, 0x00},
    {0x43, 0x11}, {0x16, 0x10}, {0x39, 0x02}, {0x35, 0x88}, {0x22, 0x0a},
    {0x37, 0x40}, {0x23, 0x00}, {0x34, 0xa0}, {0x06, 0x02}, {0x06, 0x88},
    {0x07, 0xc0}, {0x0d, 0xb7}, {0x0e, 0x01}, {0x4c, 0x00}, {0x4a, 0x81},
    {0x21, 0x99}, {0x24, 0x40}, {0x25, 0x38}, {0x26, 0x82}, {0x5c, 0x00},
    {0x63, 0x00}, {0x46, 0x22}, {0x0c, 0x3a}, {0x5d, 0x55}, {0x5e, 0x7d},
    {0x5f, 0x7d}, {0x60, 0x55}, {0x61, 0x70}, {0x62, 0x80}, {0x7c, 0x05},
    {0x20, 0x80}, {0x28, 0x30}, {0x6c, 0x00}, {0x6d, 0x80}, {0x6e, 0x00},
    {0x70, 0x02}, {0x71, 0x94}, {0x73, 0xc1}, {0x3d, 0x34}, {0x12, 0x04},
    {0x5a, 0x57}, {0x4f, 0xbb}, {0x50, 0x9c}, {0xff, 0x00}, {0xe5, 0x7f},
    {0xf9, 0xc0}, {0x41, 0x24}, {0xe0, 0x14}, {0x76, 0xff}, {0x33, 0xa0},
    {0x42, 0x20}, {0x43, 0x18}, {0x4c, 0x00}, {0x87, 0xd0}, {0x88, 0x3f},
    {0xd7, 0x03}, {0xd9, 0x10}, {0xd3, 0x82}, {0xc8, 0x08}, {0xc9, 0x80},
    {0x7c, 0x00}, {0x7d, 0x00}, {0x7c, 0x03}, {0x7d, 0x48}, {0x7d, 0x48},
    {0x7c, 0x08}, {0x7d, 0x20}, {0x7d, 0x10}, {0x7d, 0x0e}, {0x90, 0x00},
    {0x91, 0x0e}, {0x91, 0x1a}, {0x91, 0x31}, {0x91, 0x5a}, {0x91, 0x69},
    {0x91, 0x75}, {0x91, 0x7e}, {0x91, 0x88}, {0x91, 0x8f}, {0x91, 0x96},
    {0x91, 0xa3}, {0x91, 0xaf}, {0x91, 0xc4}, {0x91, 0xd7}, {0x91, 0xe8},
    {0x91, 0x20}, {0x92, 0x00}, {0x93, 0x06}, {0x93, 0xe3}, {0x93, 0x03},
    {0x93, 0x03}, {0x93, 0x00}, {0x93, 0x02}, {0x93, 0x00}, {0x93, 0x00},
    {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00},
    {0x96, 0x00}, {0x97, 0x08}, {0x97, 0x19}, {0x97, 0x02}, {0x97, 0x0c},
    {0x97, 0x24}, {0x97, 0x30}, {0x97, 0x28}, {0x97, 0x26}, {0x97, 0x02},
    {0x97, 0x98}, {0x97, 0x80}, {0x97, 0x00}, {0x97, 0x00}, {0xa4, 0x00},
    {0xa8, 0x00}, {0xc5, 0x11}, {0xc6, 0x51}, {0xbf, 0x80}, {0xc7, 0x10},
    {0xb6, 0x66}, {0xb8, 0xa5}, {0xb7, 0x64}, {0xb9, 0x7c}, {0xb3, 0xaf},
    {0xb4, 0x97}, {0xb5, 0xff}, {0xb0, 0xc5}, {0xb1, 0x94}, {0xb2, 0x0f},
    {0xc4, 0x5c}, {0xa6, 0x00}, {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x1b},
    {0xa7, 0x31}, {0xa7, 0x00}, {0xa7, 0x18}, {0xa7, 0x20}, {0xa7, 0xd8},
    {0xa7, 0x19}, {0xa7, 0x31}, {0xa7, 0x00}, {0xa7, 0x18}, {0xa7, 0x20},
    {0xa7, 0xd8}, {0xa7, 0x19}, {0xa7, 0x31}, {0xa7, 0x00}, {0xa7, 0x18},
    {0x7f, 0x00}, {0xe5, 0x1f}, {0xe1, 0x77}, {0xdd, 0x7f}, {0xc2, 0x0e},
    {0xff, 0x00}, {0xe0, 0x04}, {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x3d},
    {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x88},
    {0x57, 0x00}, {0x50, 0x92}, {0x5a, 0x50}, {0x5b, 0x3c}, {0x5c, 0x00},
    {0xd3, 0x04}, {0xe0, 0x00}, {0xff, 0x00}, {0x05, 0x00}, {0xda, 0x08},
    {0xd7, 0x03}, {0xe0, 0x00}, {0x05, 0x00}, {0xda, 0x00}, {0x5a, 0x18},
    {0x5b, 0x18}, {0xff, 0xff}};
#endif

static const ov2640_i2c_pair_t CAM_CTRL_TASK_YUV_96x96[] = {
    {0xff, 0x0},
    {0x2c, 0xff},
    {0x2e, 0xdf},
    {0xff, 0x1},
    {0x3c, 0x32},
    {0x11, 0x0},
    {0x9, 0x2},
    {0x4, 0xa8},
    {0x13, 0xe5},
    {0x14, 0x48},
    {0x2c, 0xc},
    {0x33, 0x78},
    {0x3a, 0x33},
    {0x3b, 0xfb},
    {0x3e, 0x0},
    {0x43, 0x11},
    {0x16, 0x10},
    {0x39, 0x2},
    {0x35, 0x88},

    {0x22, 0xa},
    {0x37, 0x40},
    {0x23, 0x0},
    {0x34, 0xa0},
    {0x6, 0x2},
    {0x6, 0x88},
    {0x7, 0xc0},
    {0xd, 0xb7},
    {0xe, 0x1},
    {0x4c, 0x0},
    {0x4a, 0x81},
    {0x21, 0x99},
    {0x24, 0x40},
    {0x25, 0x38},
    {0x26, 0x82},
    {0x5c, 0x0},
    {0x63, 0x0},
    {0x46, 0x22},
    {0xc, 0x3a},
    {0x5d, 0x55},
    {0x5e, 0x7d},
    {0x5f, 0x7d},
    {0x60, 0x55},
    {0x61, 0x70},
    {0x62, 0x80},
    {0x7c, 0x5},
    {0x20, 0x80},
    {0x28, 0x30},
    {0x6c, 0x0},
    {0x6d, 0x80},
    {0x6e, 0x0},
    {0x70, 0x2},
    {0x71, 0x94},
    {0x73, 0xc1},
    {0x3d, 0x34},
    {0x12, 0x4},
    {0x5a, 0x57},
    {0x4f, 0xbb},
    {0x50, 0x9c},
    {0xff, 0x0},
    {0xe5, 0x7f},
    {0xf9, 0xc0},
    {0x41, 0x24},
    {0xe0, 0x14},
    {0x76, 0xff},
    {0x33, 0xa0},
    {0x42, 0x20},
    {0x43, 0x18},
    {0x4c, 0x0},
    {0x87, 0xd0},
    {0x88, 0x3f},
    {0xd7, 0x3},
    {0xd9, 0x10},
    {0xd3, 0x82},
    {0xc8, 0x8},
    {0xc9, 0x80},
    {0x7c, 0x0},
    {0x7d, 0x0},
    {0x7c, 0x3},
    {0x7d, 0x48},
    {0x7d, 0x48},
    {0x7c, 0x8},
    {0x7d, 0x20},
    {0x7d, 0x10},
    {0x7d, 0xe},
    {0x90, 0x0},
    {0x91, 0xe},
    {0x91, 0x1a},
    {0x91, 0x31},
    {0x91, 0x5a},
    {0x91, 0x69},
    {0x91, 0x75},
    {0x91, 0x7e},
    {0x91, 0x88},
    {0x91, 0x8f},
    {0x91, 0x96},
    {0x91, 0xa3},
    {0x91, 0xaf},
    {0x91, 0xc4},
    {0x91, 0xd7},
    {0x91, 0xe8},
    {0x91, 0x20},
    {0x92, 0x0},

    {0x93, 0x6},
    {0x93, 0xe3},
    {0x93, 0x3},
    {0x93, 0x3},
    {0x93, 0x0},
    {0x93, 0x2},
    {0x93, 0x0},
    {0x93, 0x0},
    {0x93, 0x0},
    {0x93, 0x0},
    {0x93, 0x0},
    {0x93, 0x0},
    {0x93, 0x0},
    {0x96, 0x0},
    {0x97, 0x8},
    {0x97, 0x19},
    {0x97, 0x2},
    {0x97, 0xc},
    {0x97, 0x24},
    {0x97, 0x30},
    {0x97, 0x28},
    {0x97, 0x26},
    {0x97, 0x2},
    {0x97, 0x98},
    {0x97, 0x80},
    {0x97, 0x0},
    {0x97, 0x0},
    {0xa4, 0x0},
    {0xa8, 0x0},
    {0xc5, 0x11},
    {0xc6, 0x51},
    {0xbf, 0x80},
    {0xc7, 0x10},
    {0xb6, 0x66},
    {0xb8, 0xa5},
    {0xb7, 0x64},
    {0xb9, 0x7c},
    {0xb3, 0xaf},
    {0xb4, 0x97},
    {0xb5, 0xff},
    {0xb0, 0xc5},
    {0xb1, 0x94},
    {0xb2, 0xf},
    {0xc4, 0x5c},
    {0xa6, 0x0},
    {0xa7, 0x20},
    {0xa7, 0xd8},
    {0xa7, 0x1b},
    {0xa7, 0x31},
    {0xa7, 0x0},
    {0xa7, 0x18},
    {0xa7, 0x20},
    {0xa7, 0xd8},
    {0xa7, 0x19},
    {0xa7, 0x31},
    {0xa7, 0x0},
    {0xa7, 0x18},
    {0xa7, 0x20},
    {0xa7, 0xd8},
    {0xa7, 0x19},
    {0xa7, 0x31},
    {0xa7, 0x0},
    {0xa7, 0x18},
    {0x7f, 0x0},
    {0xe5, 0x1f},
    {0xe1, 0x77},
    {0xdd, 0x7f},
    {0xc2, 0xe},

    {0xff, 0x0},
    {0xe0, 0x4},
    {0xc0, 0xc8},
    {0xc1, 0x96},
    {0x86, 0x3d},
    {0x51, 0x90},
    {0x52, 0x2c},
    {0x53, 0x0},
    {0x54, 0x0},
    {0x55, 0x88},
    {0x57, 0x0},

    {0x50, 0x92},
    {0x5a, 0x50},
    {0x5b, 0x3c},
    {0x5c, 0x0},
    {0xd3, 0x4},
    {0xe0, 0x0},

    {0xff, 0x0},
    {0x5, 0x0},

    {0xda, 0x8},
    {0xd7, 0x3},
    {0xe0, 0x0},

    {0x5, 0x00},
    {0xDA, 0x0},
    {0x5A, 0x18},
    {0x5B, 0x18},

    {0xff,0xff},

};

static const uint32_t CAM_CTRL_TASK_YUV_96x96_count =
    sizeof(CAM_CTRL_TASK_YUV_96x96) / sizeof(CAM_CTRL_TASK_YUV_96x96[0]);

static cam_ctrl_task_ctx_t s_cam_ctrl_task;

// *****************************************************************************
// Private (static, forward) declarations

/**
 * @brief Return true if the given VID refers to an ARDUCAM CAM_CTRL_TASK camera
 */
static bool is_valid_vid(uint8_t vid);

/**
 * @brief Return true if the given PID refers to an ARDUCAM CAM_CTRL_TASK camera
 */
static bool is_valid_pid(uint8_t pid);

/**
 * @brief Set a holdoff timer for the given number of milliseconds.
 *
 * See also: await_holdoff()
 */
static void set_holdoff(uint32_t ms);

/**
 * @brief Set s_cam_ctrl_task to next_state when a previously set holdoff expires, else
 * do nothing.
 */
static void await_holdoff(cam_ctrl_task_state_t next_state);

// *****************************************************************************
// Public code

void cam_ctrl_task_init(void) {
    s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_INIT;
}

bool cam_ctrl_reset_camera(void) {
    s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_START_ASSERT_RESET;
    return true;
}

bool cam_ctrl_task_probe_i2c(void) {
    s_cam_ctrl_task.retry_count = 0;
    s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_CHECK_VID_PID;
    return true;
}

bool cam_ctrl_task_setup_camera(void) {
    s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_START_FORMAT_RESET;
    return true;
}

bool cam_ctrl_task_succeeded(void) {
    return s_cam_ctrl_task.state == CAM_CTRL_TASK_STATE_SUCCESS;
}

bool cam_ctrl_task_had_error(void) {
    return s_cam_ctrl_task.state == CAM_CTRL_TASK_STATE_ERROR;
}

void cam_ctrl_task_step(void) {
    switch (s_cam_ctrl_task.state) {

    case CAM_CTRL_TASK_STATE_INIT: {
        // remain in this state until a call to cam_ctrl_task_probe_i2c advances state
    } break;

    case CAM_CTRL_TASK_STATE_START_ASSERT_RESET: {
        // reset the controller chip: assert reset bit and wait 100 mSec
        if (!ov2640_i2c_select_bank(OV2640_I2C_SENSOR_BANK)) {
            printf("# CAM_CTRL_TASK_STATE_START_ASSERT_RESET failed to select bank\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
        } else if (!ov2640_i2c_write_byte(OV2640_I2C_COM7, 0x80)) {
            printf("# CAM_CTRL_TASK_STATE_START_ASSERT_RESET failed to write to COM7\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
        } else {
            set_holdoff(RESET_HOLDOFF_MS);
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_AWAIT_ASSERT_RESET;
        }
    } break;

    case CAM_CTRL_TASK_STATE_AWAIT_ASSERT_RESET: {
        await_holdoff(CAM_CTRL_TASK_STATE_START_DEASSERT_RESET);
    } break;

    case CAM_CTRL_TASK_STATE_START_DEASSERT_RESET: {
        // de-assert reset bit and wait another 100 mSec
        if (!ov2640_i2c_write_byte(OV2640_I2C_COM7, 0x00)) {
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
        } else {
            set_holdoff(RESET_HOLDOFF_MS);
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_AWAIT_DEASSERT_RESET;
        }
    } break;

    case CAM_CTRL_TASK_STATE_AWAIT_DEASSERT_RESET: {
        await_holdoff(CAM_CTRL_TASK_STATE_CHECK_VID_PID);
    } break;

    case CAM_CTRL_TASK_STATE_CHECK_VID_PID: {
        // Issue tI2C commands to read the VID and PID of the camera and confirm
        // that it is compatible.  Retry read up to MAX_RETRY_COUNT times if
        // needed.
        uint8_t vid = 0x55;
        uint8_t pid = 0xaa;

        if (s_cam_ctrl_task.retry_count++ > MAX_RETRY_COUNT) {
            printf("# too many retries\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
            break;
        }
        if (!ov2640_i2c_select_bank(OV2640_I2C_SENSOR_BANK)) {
            printf("# could not write CAM_CTRL_TASK_DEV_CTRL_REG\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
            break;
        }
        if (!ov2640_i2c_read_byte(CAM_CTRL_TASK_CHIPID_HIGH, &vid)) {
            printf("# could not read vid\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
            break;
        }
        printf("# --- vid = %d\r\n", vid);
        if (!is_valid_vid(vid)) {
            printf("# vid mismatch (0x%02x) - retrying\r\n", vid);
            set_holdoff(RETRY_DELAY_MS);
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_RETRY_WAIT;
            break;
        }
        if (!ov2640_i2c_read_byte(CAM_CTRL_TASK_CHIPID_LOW, &pid)) {
            printf("# could not read pid\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
            break;
        }
        printf("# --- pid = %d\r\n", pid);
        if (!is_valid_pid(pid)) {
            printf("# pid mismatch (0x%02x) - retrying\r\n", pid);
            set_holdoff(RETRY_DELAY_MS);
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_RETRY_WAIT;
            break;
        }
        // success...
        printf("# --- vid:pid success\r\n");
        s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_SUCCESS;
        break;
    }

    case CAM_CTRL_TASK_STATE_RETRY_WAIT: {
        // An attempt at reading the VID or PID failed.  Pause briefly before
        // trying again.
        await_holdoff(CAM_CTRL_TASK_STATE_CHECK_VID_PID);
    } break;

    case CAM_CTRL_TASK_STATE_START_FORMAT_RESET: {
        if (!ov2640_i2c_select_bank(OV2640_I2C_SENSOR_BANK)) {
            printf("# Failed to select sensor bank in set_format\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
        } else if (!ov2640_i2c_write_byte(OV2640_I2C_COM7, 0x80)) {
            printf("# Failed to reset processor in set_format\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
        } else {
            // reset has started.  hold off for 100 mSec
            set_holdoff(I2C_OP_HOLDOFF_MS);
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_AWAIT_FORMAT_RESET;
        }
    } break;

    case CAM_CTRL_TASK_STATE_AWAIT_FORMAT_RESET: {
        // Remain in this state until holdoff timer expires.
        await_holdoff(CAM_CTRL_TASK_STATE_FORMAT_LOAD);
    } break;

    case CAM_CTRL_TASK_STATE_FORMAT_LOAD: {
        // Load YUV program into camera
        const ov2640_i2c_pair_t *pairs = CAM_CTRL_TASK_YUV_96x96;
        size_t count = CAM_CTRL_TASK_YUV_96x96_count;
        if (!ov2640_i2c_write_pairs(pairs, count)) {
            printf("# Failed to load camera format\r\n");
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_ERROR;
        } else {
            set_holdoff(I2C_OP_HOLDOFF_MS);
            s_cam_ctrl_task.state = CAM_CTRL_TASK_STATE_SUCCESS;
        }
    } break;

    case CAM_CTRL_TASK_STATE_SUCCESS: {
        // remain in this state until a call to cam_ctrl_task_probe_i2c or
        // cam_ctrl_task_set_format advances the state
        asm("nop");
    } break;

    case CAM_CTRL_TASK_STATE_ERROR: {
        // remain in this state
        asm("nop");
    } break;

    } // switch
}

// *****************************************************************************
// Private (static) code

static bool is_valid_vid(uint8_t vid) { return vid == 0x26; }

static bool is_valid_pid(uint8_t pid) { return (pid >= 0x40) && (pid <= 0x42); }

static void set_holdoff(uint32_t ms) { SYS_TIME_DelayMS(ms, &s_cam_ctrl_task.delay); }

static void await_holdoff(cam_ctrl_task_state_t next_state) {
    if (SYS_TIME_DelayIsComplete(s_cam_ctrl_task.delay)) {
        s_cam_ctrl_task.state = next_state;
    } // else remain in current state...
}

// *****************************************************************************
// End of file

/**
 * @file ov2640.c
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

#include "ov2640.h"
#include "arducam.h"
#include "definitions.h"
#include "ov2640.h"
#include <stdarg.h>
#include <stdio.h>

// *****************************************************************************
// Private types and definitions

#define MAX_RETRY_COUNT 5
#define RESET_HOLDOFF_MS 100
#define RETRY_DELAY_MS 100
#define I2C_OP_HOLDOFF_MS 100

#define OV2640_I2C_ADDR (0x60 >> 1)

#define OV2640_CHIPID_HIGH 0x00A
#define OV2640_CHIPID_LOW 0x00B
#define OV2640_DEV_CTRL_REG 0xFF
#define OV2640_DEV_CTRL_REG_COM7 0x12
#define OV2640_DEV_CTRL_REG_COM10 0x15

/**
 * @brief A container for a register address / register value pair.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} reg_val_t;

/**
 * @brief ov2640 states.
 */
typedef enum {
    OV2640_STATE_INIT,
    OV2640_STATE_START_ASSERT_RESET,
    OV2640_STATE_AWAIT_ASSERT_RESET,
    OV2640_STATE_START_DEASSERT_RESET,
    OV2640_STATE_AWAIT_DEASSERT_RESET,
    OV2640_STATE_CHECK_VID_PID,
    OV2640_STATE_RETRY_WAIT,
    OV2640_STATE_START_SET_FORMAT,
    OV2640_STATE_AWAIT_SET_FORMAT,
    OV2640_STATE_SUCCESS,
    OV2640_STATE_ERROR,
} ov2640_state_t;

typedef struct {
    ov2640_state_t state;    // current state
    DRV_HANDLE drvI2CHandle; // I2C handle
    ov2640_format_t format;  // selected image format
    SYS_TIME_HANDLE delay;   // general delay timer
    int retry_count;         // retry chip id
} ov2640_ctx_t;

// *****************************************************************************
// Private (static) storage

static const reg_val_t OV2640_YUV_96x96[] = {
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

static const uint32_t OV2640_YUV_96x96_count =
    sizeof(OV2640_YUV_96x96) / sizeof(OV2640_YUV_96x96[0]);

static const reg_val_t OV2640_JPEG_320x240[] = {
    {0xff, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1a, 0x4b}, {0x32, 0x09}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23},
    {0x6d, 0x00}, {0x39, 0x12}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3},
    {0x23, 0x00}, {0x34, 0xc0}, {0x36, 0x1a}, {0x06, 0x88}, {0x07, 0xc0},
    {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00}, {0xff, 0x00}, {0xe0, 0x04},
    {0xc0, 0x64}, {0xc1, 0x4b}, {0x86, 0x35}, {0x50, 0x89}, {0x51, 0xc8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5a, 0x50}, {0x5b, 0x3c}, {0x5c, 0x00}, {0xe0, 0x00}, {0xff, 0xff},
};

static const uint32_t OV2640_JPEG_320x240_count =
    sizeof(OV2640_JPEG_320x240) / sizeof(OV2640_JPEG_320x240[0]);


static ov2640_ctx_t s_ov2640;

// *****************************************************************************
// Private (static, forward) declarations

/**
 * @brief Write {reg, data} to I2C address OV2640_I2C_ADDR
 */
static bool i2c_write_reg(uint8_t reg, uint8_t data);

/**
 * @brief Read {reg} from I2C address OV2640_I2C_ADDR
 */
static bool i2c_read_reg(uint8_t reg, uint8_t *data);

/**
 * @brief Write multiple {reg, data} pairs to I2C OV2640_I2C_ADDR
 */
static bool i2c_write_regs(const reg_val_t pairs[], size_t count);

/**
 * @brief Return true if the given VID refers to an ARDUCAM OV2640 camera
 */
static bool is_valid_vid(uint8_t vid);

/**
 * @brief Return true if the given PID refers to an ARDUCAM OV2640 camera
 */
static bool is_valid_pid(uint8_t pid);

/**
 * @brief Set a holdoff timer for the given number of milliseconds.
 *
 * See also: await_holdoff()
 */
static void set_holdoff(uint32_t ms);

/**
 * @brief Set s_ov2640 to next_state when a previously set holdoff expires, else
 * do nothing.
 */
static void await_holdoff(ov2640_state_t next_state);

// *****************************************************************************
// Public code

void ov2640_init(void) {
    s_ov2640.state = OV2640_STATE_INIT;
    s_ov2640.drvI2CHandle = DRV_HANDLE_INVALID; // I2C driver not yet open
}

void ov2640_step(void) {
    switch (s_ov2640.state) {

    case OV2640_STATE_INIT: {
        // remain in this state until a call to ov2640_probe_i2c advances state
    } break;

    case OV2640_STATE_START_ASSERT_RESET: {
        // reset the controller chip: assert reset bit and wait 100 mSec
        if (!i2c_write_reg(0x07, 0x80)) {
            s_ov2640.state = OV2640_STATE_ERROR;
        } else {
            set_holdoff(RESET_HOLDOFF_MS);
            s_ov2640.state = OV2640_STATE_AWAIT_ASSERT_RESET;
        }
    } break;

    case OV2640_STATE_AWAIT_ASSERT_RESET: {
        await_holdoff(OV2640_STATE_START_DEASSERT_RESET);
    } break;

    case OV2640_STATE_START_DEASSERT_RESET: {
        // de-assert reset bit and wait another 100 mSec
        if (!i2c_write_reg(0x07, 0x00)) {
            s_ov2640.state = OV2640_STATE_ERROR;
        } else {
            set_holdoff(RESET_HOLDOFF_MS);
            s_ov2640.state = OV2640_STATE_AWAIT_DEASSERT_RESET;
        }
    } break;

    case OV2640_STATE_AWAIT_DEASSERT_RESET: {
        await_holdoff(OV2640_STATE_CHECK_VID_PID);
    } break;

    case OV2640_STATE_CHECK_VID_PID: {
        // Issue tI2C commands to read the VID and PID of the camera and confirm
        // that it is compatible.  Retry read up to MAX_RETRY_COUNT times if
        // needed.
        uint8_t vid = 0x55;
        uint8_t pid = 0xaa;

        if (s_ov2640.retry_count++ > MAX_RETRY_COUNT) {
            printf("# too many retries\r\n");
            s_ov2640.state = OV2640_STATE_ERROR;
            break;
        }
        if (!i2c_write_reg(OV2640_DEV_CTRL_REG, 0x01)) {
            printf("# could not write OV2640_DEV_CTRL_REG\r\n");
            s_ov2640.state = OV2640_STATE_ERROR;
            break;
        }
        if (!i2c_read_reg(OV2640_CHIPID_HIGH, &vid)) {
            printf("# could not read vid\r\n");
            s_ov2640.state = OV2640_STATE_ERROR;
            break;
        }
        if (!is_valid_vid(vid)) {
            printf("# vid mismatch (0x%02x) - retrying\r\n", vid);
            set_holdoff(RETRY_DELAY_MS);
            s_ov2640.state = OV2640_STATE_RETRY_WAIT;
            break;
        }
        if (!i2c_read_reg(OV2640_CHIPID_LOW, &pid)) {
            printf("# could not read pid\r\n");
            s_ov2640.state = OV2640_STATE_ERROR;
            break;
        }
        if (!is_valid_pid(pid)) {
            printf("# pid mismatch (0x%02x) - retrying\r\n", pid);
            set_holdoff(RETRY_DELAY_MS);
            s_ov2640.state = OV2640_STATE_RETRY_WAIT;
            break;
        }
        // success...
        s_ov2640.state = OV2640_STATE_SUCCESS;
        break;
    }

    case OV2640_STATE_RETRY_WAIT: {
        // An attempt at reading the VID or PID failed.  Pause briefly before
        // trying again.
        await_holdoff(OV2640_STATE_CHECK_VID_PID);
    } break;

    case OV2640_STATE_START_SET_FORMAT: {
        const reg_val_t *pairs;
        size_t count;

        if (s_ov2640.format == OV2640_FORMAT_YUV) {
            pairs = OV2640_YUV_96x96;
            count = OV2640_YUV_96x96_count;
        } else if (s_ov2640.format == OV2640_FORMAT_JPEG) {
            pairs = OV2640_JPEG_320x240;
            count = OV2640_JPEG_320x240_count;
        } else {
            printf("# Unrecognized image format %d\r\n", s_ov2640.format);
            s_ov2640.state = OV2640_STATE_ERROR;
            break;
        }

        if (!i2c_write_regs(pairs, count)) {
            printf("# Failed to load camera format\r\n");
            s_ov2640.state = OV2640_STATE_ERROR;
        } else {
            set_holdoff(I2C_OP_HOLDOFF_MS);
            s_ov2640.state = OV2640_STATE_AWAIT_SET_FORMAT;
        }
        break;
    }

    case OV2640_STATE_AWAIT_SET_FORMAT: {
        // Remain in this state until holdoff timer expires.
        await_holdoff(OV2640_STATE_SUCCESS);
    } break;

    case OV2640_STATE_SUCCESS: {
        // remain in this state until a call to ov2640_probe_i2c or
        // ov2640_set_format advances the state
        asm("nop");
    } break;

    case OV2640_STATE_ERROR: {
        // remain in this state
        asm("nop");
    } break;

    } // switch
}

bool ov2640_probe_i2c(void) {
    if (s_ov2640.drvI2CHandle == DRV_HANDLE_INVALID) {
        // Need to open I2C driver
        s_ov2640.drvI2CHandle =
            DRV_I2C_Open(DRV_I2C_INDEX_0, DRV_IO_INTENT_READWRITE);
    }

    if (s_ov2640.drvI2CHandle == DRV_HANDLE_INVALID) {
        // I2C open failed
        s_ov2640.state = OV2640_STATE_ERROR;
        return false;
    } else {
        // I2C open succeeded - advance state
        s_ov2640.retry_count = 0;
        // s_ov2640.state = OV2640_STATE_START_ASSERT_RESET;
        s_ov2640.state = OV2640_STATE_CHECK_VID_PID;
        return true;
    }
}

bool ov2640_set_format(ov2640_format_t format) {
    s_ov2640.format = format;
    s_ov2640.state = OV2640_STATE_START_SET_FORMAT;
    return true;
}

bool ov2640_succeeded(void) {
    return s_ov2640.state == OV2640_STATE_SUCCESS;
}

bool ov2640_had_error(void) {
    return s_ov2640.state == OV2640_STATE_ERROR;
}

// *****************************************************************************
// Private (static) code

static bool i2c_write_reg(uint8_t reg, uint8_t data) {
    uint8_t tx_buf[] = {reg, data};
    // NOTE: the call to i2c_write_reg() appears to depend on this
    // printf() to provide a timing delay!
    printf("# i2c_write_reg(0x%02x, 0x%02x)\r\n", tx_buf[0], tx_buf[1]);
    return DRV_I2C_WriteTransfer(s_ov2640.drvI2CHandle,
                                 OV2640_I2C_ADDR, (void *)tx_buf,
                                 sizeof(tx_buf));
}

static bool i2c_read_reg(uint8_t reg, uint8_t *const data) {
    // NOTE: the call to i2c_read_reg() appears to depend on these
    // printf()s to provide timing delay!
    printf("# i2c_read_reg(0x%02x) on entry, data = %02x\r\n", reg, *data);
    bool success = DRV_I2C_WriteReadTransfer(
        s_ov2640.drvI2CHandle, OV2640_I2C_ADDR, (void *)&reg,
        sizeof(reg), (void *const)data, sizeof(uint8_t));
    printf("# i2c_read_reg(0x%02x) => %02x, success = %d\r\n", reg, *data,
           success);
    return success;
}

static bool i2c_write_regs(const reg_val_t pairs[], size_t count) {
    for (int i = 0; i < count; i++) {
        const reg_val_t *pair = &pairs[i];
        if (!DRV_I2C_WriteTransfer(s_ov2640.drvI2CHandle,
                                   OV2640_I2C_ADDR, (void *)pair,
                                   sizeof(reg_val_t))) {
            printf("# At pairs[%d], failed to write [0x%02x, 0x%02x]\r\n", i,
                   pair->reg, pair->val);
            return false;
        } else {
            SYSTICK_DelayMs(1); // TODO: find real value for delay
        }
    }
    return true;
}

static bool is_valid_vid(uint8_t vid) { return vid == 0x26; }

static bool is_valid_pid(uint8_t pid) { return (pid >= 0x40) && (pid <= 0x42); }

static void set_holdoff(uint32_t ms) { SYS_TIME_DelayMS(ms, &s_ov2640.delay); }

static void await_holdoff(ov2640_state_t next_state) {
    if (SYS_TIME_DelayIsComplete(s_ov2640.delay)) {
        s_ov2640.state = next_state;
    } // else remain in current state...
}

// *****************************************************************************
// End of file

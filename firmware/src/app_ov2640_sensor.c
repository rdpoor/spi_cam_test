/**
 * @file template.c
 *
 * MIT License
 *
 * Copyright (c) 2023 BrainChip, Inc.
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
 *
 */

// *****************************************************************************
// Includes

#include "app_ov2640_sensor.h"

#include "configuration.h"
#include "definitions.h"
#include "driver/i2c/drv_i2c.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// *****************************************************************************
// Private types and definitions

#define APP_OV2640_SENSOR_I2C_ADDR (0x60 >> 1)

#define APP_RECEIVE_DATA_LENGTH 2
#define APP_RECEIVE_DUMMY_WRITE_LENGTH 1

#define OV2640_CHIPID_HIGH 0x0A
#define OV2640_CHIPID_LOW 0x0B
#define OV2640_DEV_CTRL_REG 0xFF
#define OV2640_DEV_CTRL_REG_COM7 0x12
#define OV2640_DEV_CTRL_REG_COM10 0x15

#define OV2640_160x120 0   // 160x120
#define OV2640_176x144 1   // 176x144
#define OV2640_320x240 2   // 320x240
#define OV2640_352x288 3   // 352x288
#define OV2640_640x480 4   // 640x480
#define OV2640_800x600 5   // 800x600
#define OV2640_1024x768 6  // 1024x768
#define OV2640_1280x1024 7 // 1280x1024
#define OV2640_1600x1200 8 // 1600x1200

#define MAX_RETRY_COUNT 5
#define APP_OV2640_I2C_OP_DELAY_MS 100
#define APP_OV2640_RETRY_DELAY_MS 100

typedef enum {
    APP_OV2640_SENSOR_STATE_INIT = 0,
    APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE,
    APP_OV2640_SENSOR_STATE_RETRY_WAIT,
    APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7,
    APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF,
    APP_OV2640_SENSOR_STATE_YUV_INIT,
    APP_OV2640_SENSOR_STATE_YUV_INIT_HOLDOFF,
    APP_OV2640_SENSOR_STATE_CLOSE_PORT,
    APP_OV2640_SENSOR_STATE_SUCCESS,
    APP_OV2640_SENSOR_STATE_XFER_ERROR,
} APP_OV2640_SENSOR_STATES;

typedef struct {
    /* The application's current state */
    APP_OV2640_SENSOR_STATES state; // task state
    DRV_HANDLE drvI2CHandle;        // I2C handle
    SYS_TIME_HANDLE delay;          // general delay timer
    bool isInitialized;             // true if OV2640 has been initialized
    int retry_count;                // retry chip id
} APP_OV2640_SENSOR_DATA;

typedef struct {
    uint8_t reg;
    uint8_t val;
} reg_val_t;

// *****************************************************************************
// Private (static, forward) declarations

/**
 * @brief Write {reg, data} to I2C address APP_OV2640_SENSOR_I2C_ADDR
 */
static bool i2c_write_reg(uint8_t reg, uint8_t data);

/**
 * @brief Read {reg} from I2C address APP_OV2640_SENSOR_I2C_ADDR
 */
static bool i2c_read_reg(uint8_t reg, uint8_t *data);

/**
 * @brief Write multiple {reg, data} pairs to I2C APP_OV2640_SENSOR_I2C_ADDR
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
 * @brief Set appData to next_state when a previously set holdoff expires, else
 * do nothing.
 */
static void await_holdoff(APP_OV2640_SENSOR_STATES next_state);

// *****************************************************************************
// Private (static) storage

static const reg_val_t OV2640_YUV_96x96[] = {
    {0xff, 0x0},  {0x2c, 0xff}, {0x2e, 0xdf}, {0xff, 0x1},  {0x3c, 0x32},
    {0x11, 0x0},  {0x9, 0x2},   {0x4, 0xa8},  {0x13, 0xe5}, {0x14, 0x48},
    {0x2c, 0xc},  {0x33, 0x78}, {0x3a, 0x33}, {0x3b, 0xfb}, {0x3e, 0x0},
    {0x43, 0x11}, {0x16, 0x10}, {0x39, 0x2},  {0x35, 0x88}, {0x22, 0xa},
    {0x37, 0x40}, {0x23, 0x0},  {0x34, 0xa0}, {0x6, 0x2},   {0x6, 0x88},
    {0x7, 0xc0},  {0xd, 0xb7},  {0xe, 0x1},   {0x4c, 0x0},  {0x4a, 0x81},
    {0x21, 0x99}, {0x24, 0x40}, {0x25, 0x38}, {0x26, 0x82}, {0x5c, 0x0},
    {0x63, 0x0},  {0x46, 0x22}, {0xc, 0x3a},  {0x5d, 0x55}, {0x5e, 0x7d},
    {0x5f, 0x7d}, {0x60, 0x55}, {0x61, 0x70}, {0x62, 0x80}, {0x7c, 0x5},
    {0x20, 0x80}, {0x28, 0x30}, {0x6c, 0x0},  {0x6d, 0x80}, {0x6e, 0x0},
    {0x70, 0x2},  {0x71, 0x94}, {0x73, 0xc1}, {0x3d, 0x34}, {0x12, 0x4},
    {0x5a, 0x57}, {0x4f, 0xbb}, {0x50, 0x9c}, {0xff, 0x0},  {0xe5, 0x7f},
    {0xf9, 0xc0}, {0x41, 0x24}, {0xe0, 0x14}, {0x76, 0xff}, {0x33, 0xa0},
    {0x42, 0x20}, {0x43, 0x18}, {0x4c, 0x0},  {0x87, 0xd0}, {0x88, 0x3f},
    {0xd7, 0x3},  {0xd9, 0x10}, {0xd3, 0x82}, {0xc8, 0x8},  {0xc9, 0x80},
    {0x7c, 0x0},  {0x7d, 0x0},  {0x7c, 0x3},  {0x7d, 0x48}, {0x7d, 0x48},
    {0x7c, 0x8},  {0x7d, 0x20}, {0x7d, 0x10}, {0x7d, 0xe},  {0x90, 0x0},
    {0x91, 0xe},  {0x91, 0x1a}, {0x91, 0x31}, {0x91, 0x5a}, {0x91, 0x69},
    {0x91, 0x75}, {0x91, 0x7e}, {0x91, 0x88}, {0x91, 0x8f}, {0x91, 0x96},
    {0x91, 0xa3}, {0x91, 0xaf}, {0x91, 0xc4}, {0x91, 0xd7}, {0x91, 0xe8},
    {0x91, 0x20}, {0x92, 0x0},  {0x93, 0x6},  {0x93, 0xe3}, {0x93, 0x3},
    {0x93, 0x3},  {0x93, 0x0},  {0x93, 0x2},  {0x93, 0x0},  {0x93, 0x0},
    {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},  {0x93, 0x0},
    {0x96, 0x0},  {0x97, 0x8},  {0x97, 0x19}, {0x97, 0x2},  {0x97, 0xc},
    {0x97, 0x24}, {0x97, 0x30}, {0x97, 0x28}, {0x97, 0x26}, {0x97, 0x2},
    {0x97, 0x98}, {0x97, 0x80}, {0x97, 0x0},  {0x97, 0x0},  {0xa4, 0x0},
    {0xa8, 0x0},  {0xc5, 0x11}, {0xc6, 0x51}, {0xbf, 0x80}, {0xc7, 0x10},
    {0xb6, 0x66}, {0xb8, 0xa5}, {0xb7, 0x64}, {0xb9, 0x7c}, {0xb3, 0xaf},
    {0xb4, 0x97}, {0xb5, 0xff}, {0xb0, 0xc5}, {0xb1, 0x94}, {0xb2, 0xf},
    {0xc4, 0x5c}, {0xa6, 0x0},  {0xa7, 0x20}, {0xa7, 0xd8}, {0xa7, 0x1b},
    {0xa7, 0x31}, {0xa7, 0x0},  {0xa7, 0x18}, {0xa7, 0x20}, {0xa7, 0xd8},
    {0xa7, 0x19}, {0xa7, 0x31}, {0xa7, 0x0},  {0xa7, 0x18}, {0xa7, 0x20},
    {0xa7, 0xd8}, {0xa7, 0x19}, {0xa7, 0x31}, {0xa7, 0x0},  {0xa7, 0x18},
    {0x7f, 0x0},  {0xe5, 0x1f}, {0xe1, 0x77}, {0xdd, 0x7f}, {0xc2, 0xe},
    {0xff, 0x0},  {0xe0, 0x4},  {0xc0, 0xc8}, {0xc1, 0x96}, {0x86, 0x3d},
    {0x51, 0x90}, {0x52, 0x2c}, {0x53, 0x0},  {0x54, 0x0},  {0x55, 0x88},
    {0x57, 0x0},  {0x50, 0x92}, {0x5a, 0x50}, {0x5b, 0x3c}, {0x5c, 0x0},
    {0xd3, 0x4},  {0xe0, 0x0},  {0xff, 0x0},  {0x5, 0x0},   {0xda, 0x8},
    {0xd7, 0x3},  {0xe0, 0x0},  {0x5, 0x00},  {0xDA, 0x0},  {0x5A, 0x18},
    {0x5B, 0x18}, {0xff, 0xff}};

const uint32_t OV2640_YUV_96x96_count =
    sizeof(OV2640_YUV_96x96) / sizeof(OV2640_YUV_96x96[0]);

static APP_OV2640_SENSOR_DATA appData;

// *****************************************************************************
// Public code

void APP_OV2640_SENSOR_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_OV2640_SENSOR_STATE_INIT;
    appData.isInitialized = false;
}

void APP_OV2640_SENSOR_Tasks(void) {

    switch (appData.state) {

    case APP_OV2640_SENSOR_STATE_INIT: {
        /* Open I2C driver instance */
        appData.drvI2CHandle =
            DRV_I2C_Open(DRV_I2C_INDEX_0, DRV_IO_INTENT_READWRITE);

        if (appData.drvI2CHandle != DRV_HANDLE_INVALID) {
            appData.retry_count = 0;
            appData.state = APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE;
        } else {
            printf("# could not open I2C driver\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE: {
        // Issue tI2C commands to read the VID and PID of the camera and confirm
        // that it is compatible.  Retry read up to MAX_RETRY_COUNT times if
        // needed.
        uint8_t vid = 0x55;
        uint8_t pid = 0xaa;

        if (appData.retry_count++ > MAX_RETRY_COUNT) {
            printf("# too many retries\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!i2c_write_reg(OV2640_DEV_CTRL_REG, 0x01)) {
            printf("# could not write OV2640_DEV_CTRL_REG\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!i2c_read_reg(OV2640_CHIPID_HIGH, &vid)) {
            printf("# could not read vid\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!is_valid_vid(vid)) {
            printf("# vid mismatch (0x%02x) - retrying\r\n", vid);
            set_holdoff(APP_OV2640_RETRY_DELAY_MS);
            appData.state = APP_OV2640_SENSOR_STATE_RETRY_WAIT;
            break;
        }
        if (!i2c_read_reg(OV2640_CHIPID_LOW, &pid)) {
            printf("# could not read pid\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!is_valid_pid(pid)) {
            printf("# pid mismatch (0x%02x) - retrying\r\n", pid);
            set_holdoff(APP_OV2640_RETRY_DELAY_MS);
            appData.state = APP_OV2640_SENSOR_STATE_RETRY_WAIT;
            break;
        }
        // success...
        printf("# Verified OV2640 vid:pid = 0x%02x:0x%02x\r\n", vid, pid);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7;
        break;
    };

    case APP_OV2640_SENSOR_STATE_RETRY_WAIT: {
        // An attempt at reading the VID or PID failed.  Pause briefly before
        // trying again.
        await_holdoff(APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE);
    } break;

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7: {
        if (!i2c_write_reg(OV2640_DEV_CTRL_REG, 0x01)) {
            printf("# Failed to write OV2640_DEV_CTRL_REG\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!i2c_write_reg(OV2640_DEV_CTRL_REG_COM7, 0x80)) {
            printf("# Failed to write OV2640_DEV_CTRL_REG_COM7\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        set_holdoff(APP_OV2640_I2C_OP_DELAY_MS);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF;
        break;
    }

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF: {
        // Delay before APP_OV2640_SENSOR_STATE_YUV_INIT
        await_holdoff(APP_OV2640_SENSOR_STATE_YUV_INIT);
    } break;

    case APP_OV2640_SENSOR_STATE_YUV_INIT: {
        // Configure the camera for YUV_96x96 (2 bytes per pixel)
        printf("# APP_OV2640_SENSOR_STATE_YUV_INIT\r\n");
        if (!i2c_write_regs(OV2640_YUV_96x96, OV2640_YUV_96x96_count)) {
            printf("# Failed to load OV2640_YUV_96x96\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        } else {
            set_holdoff(APP_OV2640_I2C_OP_DELAY_MS);
            appData.state = APP_OV2640_SENSOR_STATE_YUV_INIT_HOLDOFF;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_YUV_INIT_HOLDOFF: {
        await_holdoff(APP_OV2640_SENSOR_STATE_SUCCESS);
    } break;

    case APP_OV2640_SENSOR_STATE_CLOSE_PORT: {
        // The camera has been configured for YUV 96 x 96
        // Close the I2C port and remain in this state.
        DRV_I2C_Close(appData.drvI2CHandle);
        appData.state = APP_OV2640_SENSOR_STATE_SUCCESS;
    } break;

    case APP_OV2640_SENSOR_STATE_SUCCESS: {
        // Configuration succeeded.  Remain in this state
        appData.isInitialized = true;
    } break;

    case APP_OV2640_SENSOR_STATE_XFER_ERROR: {
        // Configuraton failed.  Remain in this state
    } break;

    } // switch
}

bool APP_OV2640_SENSOR_Task_IsInitialized(void) {
    return appData.isInitialized;
}

bool APP_OV2640_SENSOR_Task_Failed(void) {
    return appData.state == APP_OV2640_SENSOR_STATE_XFER_ERROR;
}

// *****************************************************************************
// Private (static) code

static bool i2c_write_reg(uint8_t reg, uint8_t data) {
    uint8_t tx_buf[] = {reg, data};
    // NOTE: the call to i2c_write_reg() appears to depend on this
    // printf() to provide a timing delay!
    printf("# i2c_write_reg(0x%02x, 0x%02x)\r\n", tx_buf[0], tx_buf[1]);
    return DRV_I2C_WriteTransfer(appData.drvI2CHandle,
                                 APP_OV2640_SENSOR_I2C_ADDR, (void *)tx_buf,
                                 sizeof(tx_buf));
}

static bool i2c_read_reg(uint8_t reg, uint8_t *const data) {
    // NOTE: the call to i2c_read_reg() appears to depend on these
    // printf()s to provide timing delay!
    printf("# i2c_read_reg(0x%02x) on entry, data = %02x\r\n", reg, *data);
    bool success = DRV_I2C_WriteReadTransfer(
        appData.drvI2CHandle, APP_OV2640_SENSOR_I2C_ADDR, (void *)&reg,
        sizeof(reg), (void *const)data, sizeof(uint8_t));
    printf("# i2c_read_reg(0x%02x) => %02x, success = %d\r\n", reg, *data,
           success);
    return success;
}

static bool i2c_write_regs(const reg_val_t pairs[], size_t count) {
    for (int i = 0; i < count; i++) {
        const reg_val_t *pair = &pairs[i];
        if (!DRV_I2C_WriteTransfer(appData.drvI2CHandle,
                                   APP_OV2640_SENSOR_I2C_ADDR, (void *)pair,
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

static void set_holdoff(uint32_t ms) { SYS_TIME_DelayMS(ms, &appData.delay); }

static void await_holdoff(APP_OV2640_SENSOR_STATES next_state) {
    if (SYS_TIME_DelayIsComplete(appData.delay)) {
        appData.state = next_state;
    } // else remain in current state...
}

// *****************************************************************************
// End of file

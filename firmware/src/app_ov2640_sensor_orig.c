/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_ov2640_sensor.c

  Summary:
    This file contains the source code for the OV2640 Sensor module..

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic to configure the OV2640 sensor module.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * Subject to your compliance with these terms, you may use Microchip software
 * and any derivatives exclusively with Microchip products. It is your
 * responsibility to comply with third party license terms applicable to your
 * use of third party software (including open source software) that may
 * accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
 * ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app_ov2640_sensor.h"

#include "configuration.h"
#include "definitions.h"
#include "driver/i2c/drv_i2c.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// *****************************************************************************
// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
// *****************************************************************************

#define APP_OV2640_SENSOR_I2C_ADDR (0x60 >> 1)

#define APP_RECEIVE_DATA_LENGTH 2
#define APP_RECEIVE_DUMMY_WRITE_LENGTH 1

#define OV2640_CHIPID_HIGH          0x0A
#define OV2640_CHIPID_LOW           0x0B
#define OV2640_DEV_CTRL_REG         0xFF
#define OV2640_DEV_CTRL_REG_COM7    0x12
#define OV2640_DEV_CTRL_REG_COM10   0x15

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

// *****************************************************************************
/* Application states

  Summary:
    Application states enumeration

  Description:
    This enumeration defines the valid application states.  These states
    determine the behavior of the application at various times.
*/

typedef enum {
    /* Application's state machine's initial state. */
    APP_OV2640_SENSOR_STATE_INIT = 0,
    APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE,
    APP_OV2640_SENSOR_STATE_RETRY_WAIT,
    APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7,
    APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF,
    APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT,
    APP_OV2640_SENSOR_STATE_MEDIA_YUV422,
    APP_OV2640_SENSOR_STATE_MEDIA_JPEG,
    APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10,
    APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10_HOLDOFF,
    APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG,
    APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG_HOLDOFF,
    APP_OV2640_SENSOR_STATE_SUCCESS,
    APP_OV2640_SENSOR_STATE_XFER_ERROR,
} APP_OV2640_SENSOR_STATES;

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    Application strings and buffers are be defined outside this structure.
 */

typedef struct {
    /* The application's current state */
    APP_OV2640_SENSOR_STATES state; // task state
    DRV_HANDLE drvI2CHandle;        // I2C handle
    SYS_TIME_HANDLE delay;          // general delay timer
    bool isInitialized;             // true if OV2640 has been initialized
    int retry_count;                // retry chip id
} APP_OV2640_SENSOR_DATA;

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

static const uint8_t OV2640_JPEG_INIT_Reg[][2] =
{
  { 0xff, 0x00 },
  { 0x2c, 0xff },
  { 0x2e, 0xdf },
  { 0xff, 0x01 },
  { 0x3c, 0x32 },
  { 0x11, 0x00 },
  { 0x09, 0x02 },
  { 0x04, 0x28 },
  { 0x13, 0xe5 },
  { 0x14, 0x48 },
  { 0x2c, 0x0c },
  { 0x33, 0x78 },
  { 0x3a, 0x33 },
  { 0x3b, 0xfB },
  { 0x3e, 0x00 },
  { 0x43, 0x11 },
  { 0x16, 0x10 },
  { 0x39, 0x92 },
  { 0x35, 0xda },
  { 0x22, 0x1a },
  { 0x37, 0xc3 },
  { 0x23, 0x00 },
  { 0x34, 0xc0 },
  { 0x36, 0x1a },
  { 0x06, 0x88 },
  { 0x07, 0xc0 },
  { 0x0d, 0x87 },
  { 0x0e, 0x41 },
  { 0x4c, 0x00 },
  { 0x48, 0x00 },
  { 0x5B, 0x00 },
  { 0x42, 0x03 },
  { 0x4a, 0x81 },
  { 0x21, 0x99 },
  { 0x24, 0x40 },
  { 0x25, 0x38 },
  { 0x26, 0x82 },
  { 0x5c, 0x00 },
  { 0x63, 0x00 },
  { 0x61, 0x70 },
  { 0x62, 0x80 },
  { 0x7c, 0x05 },
  { 0x20, 0x80 },
  { 0x28, 0x30 },
  { 0x6c, 0x00 },
  { 0x6d, 0x80 },
  { 0x6e, 0x00 },
  { 0x70, 0x02 },
  { 0x71, 0x94 },
  { 0x73, 0xc1 },
  { 0x12, 0x40 },
  { 0x17, 0x11 },
  { 0x18, 0x43 },
  { 0x19, 0x00 },
  { 0x1a, 0x4b },
  { 0x32, 0x09 },
  { 0x37, 0xc0 },
  { 0x4f, 0x60 },
  { 0x50, 0xa8 },
  { 0x6d, 0x00 },
  { 0x3d, 0x38 },
  { 0x46, 0x3f },
  { 0x4f, 0x60 },
  { 0x0c, 0x3c },
  { 0xff, 0x00 },
  { 0xe5, 0x7f },
  { 0xf9, 0xc0 },
  { 0x41, 0x24 },
  { 0xe0, 0x14 },
  { 0x76, 0xff },
  { 0x33, 0xa0 },
  { 0x42, 0x20 },
  { 0x43, 0x18 },
  { 0x4c, 0x00 },
  { 0x87, 0xd5 },
  { 0x88, 0x3f },
  { 0xd7, 0x03 },
  { 0xd9, 0x10 },
  { 0xd3, 0x82 },
  { 0xc8, 0x08 },
  { 0xc9, 0x80 },
  { 0x7c, 0x00 },
  { 0x7d, 0x00 },
  { 0x7c, 0x03 },
  { 0x7d, 0x48 },
  { 0x7d, 0x48 },
  { 0x7c, 0x08 },
  { 0x7d, 0x20 },
  { 0x7d, 0x10 },
  { 0x7d, 0x0e },
  { 0x90, 0x00 },
  { 0x91, 0x0e },
  { 0x91, 0x1a },
  { 0x91, 0x31 },
  { 0x91, 0x5a },
  { 0x91, 0x69 },
  { 0x91, 0x75 },
  { 0x91, 0x7e },
  { 0x91, 0x88 },
  { 0x91, 0x8f },
  { 0x91, 0x96 },
  { 0x91, 0xa3 },
  { 0x91, 0xaf },
  { 0x91, 0xc4 },
  { 0x91, 0xd7 },
  { 0x91, 0xe8 },
  { 0x91, 0x20 },
  { 0x92, 0x00 },
  { 0x93, 0x06 },
  { 0x93, 0xe3 },
  { 0x93, 0x05 },
  { 0x93, 0x05 },
  { 0x93, 0x00 },
  { 0x93, 0x04 },
  { 0x93, 0x00 },
  { 0x93, 0x00 },
  { 0x93, 0x00 },
  { 0x93, 0x00 },
  { 0x93, 0x00 },
  { 0x93, 0x00 },
  { 0x93, 0x00 },
  { 0x96, 0x00 },
  { 0x97, 0x08 },
  { 0x97, 0x19 },
  { 0x97, 0x02 },
  { 0x97, 0x0c },
  { 0x97, 0x24 },
  { 0x97, 0x30 },
  { 0x97, 0x28 },
  { 0x97, 0x26 },
  { 0x97, 0x02 },
  { 0x97, 0x98 },
  { 0x97, 0x80 },
  { 0x97, 0x00 },
  { 0x97, 0x00 },
  { 0xc3, 0xed },
  { 0xa4, 0x00 },
  { 0xa8, 0x00 },
  { 0xc5, 0x11 },
  { 0xc6, 0x51 },
  { 0xbf, 0x80 },
  { 0xc7, 0x10 },
  { 0xb6, 0x66 },
  { 0xb8, 0xA5 },
  { 0xb7, 0x64 },
  { 0xb9, 0x7C },
  { 0xb3, 0xaf },
  { 0xb4, 0x97 },
  { 0xb5, 0xFF },
  { 0xb0, 0xC5 },
  { 0xb1, 0x94 },
  { 0xb2, 0x0f },
  { 0xc4, 0x5c },
  { 0xc0, 0x64 },
  { 0xc1, 0x4B },
  { 0x8c, 0x00 },
  { 0x86, 0x3D },
  { 0x50, 0x00 },
  { 0x51, 0xC8 },
  { 0x52, 0x96 },
  { 0x53, 0x00 },
  { 0x54, 0x00 },
  { 0x55, 0x00 },
  { 0x5a, 0xC8 },
  { 0x5b, 0x96 },
  { 0x5c, 0x00 },
  { 0xd3, 0x00 },   //{ 0xd3, 0x7f },
  { 0xc3, 0xed },
  { 0x7f, 0x00 },
  { 0xda, 0x00 },
  { 0xe5, 0x1f },
  { 0xe1, 0x67 },
  { 0xe0, 0x00 },
  { 0xdd, 0x7f },
  { 0x05, 0x00 },

  { 0x12, 0x40 },
  { 0xd3, 0x04 },   //{ 0xd3, 0x7f },
  { 0xc0, 0x16 },
  { 0xC1, 0x12 },
  { 0x8c, 0x00 },
  { 0x86, 0x3d },
  { 0x50, 0x00 },
  { 0x51, 0x2C },
  { 0x52, 0x24 },
  { 0x53, 0x00 },
  { 0x54, 0x00 },
  { 0x55, 0x00 },
  { 0x5A, 0x2c },
  { 0x5b, 0x24 },
  { 0x5c, 0x00 },
  { 0xff, 0xff },
};

static const uint8_t OV2640_YUV422_Reg[][2] =
{
  { 0xFF, 0x00 },
  { 0x05, 0x00 },
  { 0xDA, 0x10 },
  { 0xD7, 0x03 },
  { 0xDF, 0x00 },
  { 0x33, 0x80 },
  { 0x3C, 0x40 },
  { 0xe1, 0x77 },
  { 0x00, 0x00 },
  { 0xff, 0xff },
};

static const uint8_t OV2640_JPEG_Reg[][2] =
{
  { 0xe0, 0x14 },
  { 0xe1, 0x77 },
  { 0xe5, 0x1f },
  { 0xd7, 0x03 },
  { 0xda, 0x10 },
  { 0xe0, 0x00 },
  { 0xFF, 0x01 },
  { 0x04, 0x08 },
  { 0xff, 0xff },
};

static const uint8_t OV2640_320x240_JPEG_Reg[][2] =
{
  { 0xff, 0x01 },
  { 0x12, 0x40 },
  { 0x17, 0x11 },
  { 0x18, 0x43 },
  { 0x19, 0x00 },
  { 0x1a, 0x4b },
  { 0x32, 0x09 },
  { 0x4f, 0xca },
  { 0x50, 0xa8 },
  { 0x5a, 0x23 },
  { 0x6d, 0x00 },
  { 0x39, 0x12 },
  { 0x35, 0xda },
  { 0x22, 0x1a },
  { 0x37, 0xc3 },
  { 0x23, 0x00 },
  { 0x34, 0xc0 },
  { 0x36, 0x1a },
  { 0x06, 0x88 },
  { 0x07, 0xc0 },
  { 0x0d, 0x87 },
  { 0x0e, 0x41 },
  { 0x4c, 0x00 },
  { 0xff, 0x00 },
  { 0xe0, 0x04 },
  { 0xc0, 0x64 },
  { 0xc1, 0x4b },
  { 0x86, 0x35 },
  { 0x50, 0x89 },
  { 0x51, 0xc8 },
  { 0x52, 0x96 },
  { 0x53, 0x00 },
  { 0x54, 0x00 },
  { 0x55, 0x00 },
  { 0x57, 0x00 },
  { 0x5a, 0x50 },
  { 0x5b, 0x3c },
  { 0x5c, 0x00 },
  { 0xe0, 0x00 },
  { 0xff, 0xff },
};

const uint32_t OV2640_JPEG_INIT_Reg_count =
    sizeof(OV2640_JPEG_INIT_Reg) / sizeof(OV2640_JPEG_INIT_Reg[0]);
const uint32_t OV2640_YUV422_Reg_count =
    sizeof(OV2640_YUV422_Reg) / sizeof(OV2640_YUV422_Reg[0]);
const uint32_t OV2640_JPEG_Reg_count =
    sizeof(OV2640_JPEG_Reg) / sizeof(OV2640_JPEG_Reg[0]);
const uint32_t OV2640_320x240_JPEG_Reg_count =
    sizeof(OV2640_320x240_JPEG_Reg) / sizeof(OV2640_320x240_JPEG_Reg[0]);

static APP_OV2640_SENSOR_DATA appData;

// forward declarations for local functions
static bool APP_OV2640_SENSOR_Write_Reg(uint8_t reg, uint8_t data);

static bool APP_OV2640_SENSOR_Read_Reg(uint8_t reg, uint8_t *data);

static bool APP_OV2640_SENSOR_Write_Regs(const uint8_t pairs[][2], size_t count);

static bool is_valid_vid(uint8_t vid);

static bool is_valid_pid(uint8_t pid);

static void set_holdoff(uint32_t ms);

static void await_holdoff(APP_OV2640_SENSOR_STATES next_state);

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_OV2640_SENSOR_Initialize ( void )

  Remarks:
    See prototype in app_ov2640_sensor.h.
 */

void APP_OV2640_SENSOR_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_OV2640_SENSOR_STATE_INIT;
    appData.isInitialized = false;
}

/******************************************************************************
  Function:
    void APP_OV2640_SENSOR_Tasks ( void )

  Remarks:
    See prototype in app_ov2640_sensor.h.
 */

void APP_OV2640_SENSOR_Tasks(void) {

    /* dispatch on the application's current state. */
    switch (appData.state) {

    case APP_OV2640_SENSOR_STATE_INIT: {
        /* Open I2C driver instance */
        appData.drvI2CHandle =
            DRV_I2C_Open(DRV_I2C_INDEX_0, DRV_IO_INTENT_READWRITE);

        if (appData.drvI2CHandle != DRV_HANDLE_INVALID) {
            appData.retry_count = 0;
            appData.state = APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE;
        } else {
            printf("could not open I2C driver\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE: {
        // Issue tI2C commands to read the VID and PID of the camera and confirm
        // that it is compatible.
        uint8_t vid = 0x55;
        uint8_t pid = 0xaa;

        if (appData.retry_count++ > MAX_RETRY_COUNT) {
            printf("too many retries\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG, 0x01)) {
            printf("could not write OV2640_DEV_CTRL_REG\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!APP_OV2640_SENSOR_Read_Reg(OV2640_CHIPID_HIGH, &vid)) {
            printf("could not read vid\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!is_valid_vid(vid)) {
            printf("vid mismatch (0x%02x) - retrying\r\n", vid);
            set_holdoff(APP_OV2640_RETRY_DELAY_MS);
            appData.state = APP_OV2640_SENSOR_STATE_RETRY_WAIT;
            break;
        }
        if (!APP_OV2640_SENSOR_Read_Reg(OV2640_CHIPID_LOW, &pid)) {
            printf("could not read pid\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!is_valid_pid(pid)) {
            printf("pid mismatch (0x%02x) - retrying\r\n", pid);
            set_holdoff(APP_OV2640_RETRY_DELAY_MS);
            appData.state = APP_OV2640_SENSOR_STATE_RETRY_WAIT;
            break;
        }
        // success...
        printf("Verified OV2640 vid:pid = 0x%02x:0x%02x\r\n", vid, pid);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7;
        break;
    };

    case APP_OV2640_SENSOR_STATE_RETRY_WAIT: {
        // An attempt at reading the VID or PID failed.  Pause briefly before
        // trying again.
        await_holdoff(APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE);
    } break;

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7: {
        if (!APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG, 0x01)) {
            printf("Failed to write OV2640_DEV_CTRL_REG\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG_COM7, 0x80)) {
            printf("Failed to write OV2640_DEV_CTRL_REG_COM7\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        set_holdoff(APP_OV2640_I2C_OP_DELAY_MS);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF;
        break;
    }

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF: {
        // Delay before APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT
        await_holdoff(APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT);
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT: {
        printf("APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT\r\n");
        if (!APP_OV2640_SENSOR_Write_Regs(OV2640_JPEG_INIT_Reg, OV2640_JPEG_INIT_Reg_count)) {
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        } else {
            appData.state = APP_OV2640_SENSOR_STATE_MEDIA_YUV422;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_YUV422: {
        printf("APP_OV2640_SENSOR_STATE_MEDIA_YUV422\r\n");
        if (!APP_OV2640_SENSOR_Write_Regs(OV2640_YUV422_Reg, OV2640_YUV422_Reg_count)) {
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        } else {
            appData.state = APP_OV2640_SENSOR_STATE_MEDIA_JPEG;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_JPEG: {
        printf("APP_OV2640_SENSOR_STATE_MEDIA_JPEG\r\n");
        if (!APP_OV2640_SENSOR_Write_Regs(OV2640_JPEG_Reg, OV2640_JPEG_Reg_count)) {
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        } else {
            appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10: {
        if (!APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG, 0x01)) {
            printf("Failed to write OV2640_DEV_CTRL_REG\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG_COM10, 0x00)) {
            printf("Failed to write OV2640_DEV_CTRL_REG_COM7\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        set_holdoff(APP_OV2640_I2C_OP_DELAY_MS);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10_HOLDOFF;
        break;
    }

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10_HOLDOFF: {
        // Delay before APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG
        await_holdoff(APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG);
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG: {
        printf("APP_OV2640_SENSOR_STATE_MEDIA_JPEG\r\n");
        if (!APP_OV2640_SENSOR_Write_Regs(OV2640_320x240_JPEG_Reg, OV2640_320x240_JPEG_Reg_count)) {
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        } else {
            set_holdoff(APP_OV2640_I2C_OP_DELAY_MS);
            appData.state = APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG_HOLDOFF;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG_HOLDOFF: {
        await_holdoff(APP_OV2640_SENSOR_STATE_SUCCESS);
    } break;

    case APP_OV2640_SENSOR_STATE_SUCCESS: {
        // The required configuration has been sent via I2C to the camera.
        // Close the I2C port and remain in this state.
        DRV_I2C_Close(appData.drvI2CHandle);
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
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

static bool APP_OV2640_SENSOR_Write_Reg(uint8_t reg, uint8_t data) {
    uint8_t tx_buf[] = {reg, data};
    // NOTE: the call to APP_OV2640_SENSOR_Write_Reg() appears to depend on this
    // printf() to provide a timing delay!
    printf("APP_OV2640_SENSOR_Write_Reg(0x%02x, 0x%02x)\r\n", tx_buf[0],
           tx_buf[1]);
    return DRV_I2C_WriteTransfer(appData.drvI2CHandle,
                                 APP_OV2640_SENSOR_I2C_ADDR, (void *)tx_buf,
                                 sizeof(tx_buf));
}

static bool APP_OV2640_SENSOR_Read_Reg(uint8_t reg, uint8_t *const data) {
    // NOTE: the call to APP_OV2640_SENSOR_Read_Reg() appears to depend on these
    // printf()s to provide timing delay!
    printf("APP_OV2640_SENSOR_Read_Reg(0x%02x) on entry, data = %02x\r\n", reg,
           *data);
    bool success = DRV_I2C_WriteReadTransfer(
        appData.drvI2CHandle, APP_OV2640_SENSOR_I2C_ADDR, (void *)&reg,
        sizeof(reg), (void *const)data, sizeof(uint8_t));
    printf("APP_OV2640_SENSOR_Read_Reg(0x%02x) => %02x, success = %d\r\n", reg,
           *data, success);
    return success;
}

static bool APP_OV2640_SENSOR_Write_Regs(const uint8_t pairs[][2], size_t count) {
    for (int i=0; i<count; i++) {
        const uint8_t *pair = pairs[i];
        if (!DRV_I2C_WriteTransfer(appData.drvI2CHandle,
                                   APP_OV2640_SENSOR_I2C_ADDR,
                                   (void *)pair, 2)) {
            printf("At pairs[%d], failed to write [0x%02x, 0x%02x]\r\n", i, pair[0], pair[1]);
            return false;
        } else {
            SYSTICK_DelayMs(1);   // TODO: find real value for delay
        }
    }
    return true;
}

static bool is_valid_vid(uint8_t vid) { return vid == 0x26; }

static bool is_valid_pid(uint8_t pid) { return (pid >= 0x40) && (pid <= 0x42); }

static void set_holdoff(uint32_t ms) {
    SYS_TIME_DelayMS(ms, &appData.delay);
}

static void await_holdoff(APP_OV2640_SENSOR_STATES next_state) {
    if (SYS_TIME_DelayIsComplete(appData.delay)) {
        appData.state = next_state;
    } // else remain in current state...
}

/*******************************************************************************
 End of File
 */

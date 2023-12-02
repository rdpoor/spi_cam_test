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
#include "ov2640_regs.h"
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
    APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG,
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

static bool is_valid_vid(uint8_t vid);

static bool is_valid_pid(uint8_t pid);

// stubs
static inline void vTaskDelay(int ms) {
    (void)ms;
    asm("nop");
}
#define portTICK_PERIOD_MS 1

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
        uint8_t vid = 0x55;
        uint8_t pid = 0xaa;

        if (appData.retry_count++ > MAX_RETRY_COUNT) {
            // too many retries
            printf("too many retries\r\n");
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            break;
        }
        if (!APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG, 0x01)) {
            // could not write DEV_CTRL_REG
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
            SYS_TIME_DelayMS(APP_OV2640_RETRY_DELAY_MS, &appData.delay);
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
            SYS_TIME_DelayMS(APP_OV2640_RETRY_DELAY_MS, &appData.delay);
            appData.state = APP_OV2640_SENSOR_STATE_RETRY_WAIT;
            break;
        }
        printf("Verified OV2640 vid:pid = 0x%02x:0x%02x\r\n", vid, pid);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7;
        break;
    };

    case APP_OV2640_SENSOR_STATE_RETRY_WAIT: {
        // Wait here before retrying APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE
        if (SYS_TIME_DelayIsComplete(appData.delay)) {
            // retry check sensor type after appropriate delay...
            appData.state = APP_OV2640_SENSOR_STATE_CHECK_SENSOR_TYPE;
        } else {
            // waiting for timer -- remain in this state...
        }
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
        SYS_TIME_DelayMS(APP_OV2640_I2C_OP_DELAY_MS, &appData.delay);
        appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF;
        break;
    }

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM7_HOLDOFF: {
        // Delay 100 ms before APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT
        if (!SYS_TIME_DelayIsComplete(appData.delay)) {
            // waiting for timer -- remain in this state...
        } else {
            // Timer expired: proceeed
            appData.state = APP_OV2640_SENSOR_STATE_SUCCESS;
        }
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_JPEG_INIT: {
        for (uint32_t i = 0; i < OV2640_JPEG_INIT_Reg_count; i++) {
            if (DRV_I2C_WriteTransfer(
                    appData.drvI2CHandle, APP_OV2640_SENSOR_I2C_ADDR,
                    (void *)&OV2640_JPEG_INIT_Reg[i], 2) == true) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                appData.state = APP_OV2640_SENSOR_STATE_MEDIA_YUV422;
            } else {
                appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            }
        }
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_YUV422: {
        for (uint32_t i = 0; i < OV2640_YUV422_Reg_count; i++) {
            if (DRV_I2C_WriteTransfer(
                    appData.drvI2CHandle, APP_OV2640_SENSOR_I2C_ADDR,
                    (void *)&OV2640_YUV422_Reg[i], 2) == true) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                appData.state = APP_OV2640_SENSOR_STATE_MEDIA_JPEG;
            } else {
                appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            }
        }
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_JPEG: {
        for (uint32_t i = 0; i < OV2640_JPEG_Reg_count; i++) {
            if (DRV_I2C_WriteTransfer(appData.drvI2CHandle,
                                      APP_OV2640_SENSOR_I2C_ADDR,
                                      (void *)&OV2640_JPEG_Reg[i], 2) == true) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                appData.state = APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10;
            } else {
                appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            }
        }
    } break;

    case APP_OV2640_SENSOR_STATE_WRITE_CTRL_REG_COM10: {
        if (APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG, 0x01)) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (APP_OV2640_SENSOR_Write_Reg(OV2640_DEV_CTRL_REG_COM10, 0x00)) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                appData.state = APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG;
            } else {
                appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            }
        } else {
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        }

        /* Delay of 100 ms. */
        vTaskDelay(APP_OV2640_I2C_OP_DELAY_MS / portTICK_PERIOD_MS);
    } break;

    case APP_OV2640_SENSOR_STATE_MEDIA_320x240_JPEG: {
        appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;

        for (uint32_t i = 0; i < OV2640_320x240_JPEG_Reg_count; i++) {
            if (DRV_I2C_WriteTransfer(
                    appData.drvI2CHandle, APP_OV2640_SENSOR_I2C_ADDR,
                    (void *)&OV2640_320x240_JPEG_Reg[i], 2) == true) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
                appData.state = APP_OV2640_SENSOR_STATE_SUCCESS;
            } else {
                appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
            }
        }

        /* Delay of 1000 ms. */
        vTaskDelay(APP_OV2640_I2C_OP_DELAY_MS / portTICK_PERIOD_MS);

        if (appData.state == APP_OV2640_SENSOR_STATE_SUCCESS) {
            printf("APP_OV2640_SENSOR_Task: OV2640 is Ready...!\r\n");
        } else {
            appData.state = APP_OV2640_SENSOR_STATE_XFER_ERROR;
        }
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

static bool is_valid_vid(uint8_t vid) { return vid == 0x26; }

static bool is_valid_pid(uint8_t pid) { return (pid >= 0x40) && (pid <= 0x42); }

/*******************************************************************************
 End of File
 */

/**
 * @file app_ardu_cam.c
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
 *
 */

// *****************************************************************************
// Includes

#include "app_ardu_cam.h"

#include "app_ov2640_sensor.h"
#include "configuration.h"
#include "definitions.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// *****************************************************************************
// Private types and definitions

// MSB in byte 0 signifies a write operation
#define ARDUCHIP_WRITE_OP 0x80

#define ARDUCAM_CMD_WRITE 0x01
#define ARDUCAM_CMD_READ 0x00
#define ARDUCAM_CMD_RDSR 0x05
#define ARDUCAM_CMD_WREN 0x06
#define ARDUCAM_STATUS_BUSY_BIT 0x01

#define APP_ARDUCAM_SPI_CLK_SPEED 1000000

#define APP_ARDUCAM_READ_WRITE_RATE_MS 1000

#define ARDUCAM_NUM_BYTES_RD_WR 16

#define APP_ARDUCAM_READ_REG_SIZE 4
#define APP_ARDUCAM_WRITE_REG_SIZE 2

#define OV2640_MAX_FIFO_SIZE 0x5FFFF // 384KByte
#define BUF_SIZE 4096

#define ARDUCHIP_TEST1 0x00 // TEST register
#define ARDUCHIP_MODE 0x02  // Mode register
#define MCU2LCD_MODE 0x00
#define CAM2LCD_MODE 0x01
#define LCD2MCU_MODE 0x02

#define ARDUCHIP_TRIG 0x41 // Trigger source
#define VSYNC_MASK 0x01
#define SHUTTER_MASK 0x02
#define CAP_DONE_MASK 0x08

#define ARDUCHIP_FIFO 0x04 // FIFO and I2C control
#define FIFO_CLEAR_MASK 0x01
#define FIFO_START_MASK 0x02
#define FIFO_RDPTR_RST_MASK 0x10
#define FIFO_WRPTR_RST_MASK 0x20

#define BURST_FIFO_READ 0x3C  // Burst FIFO read operation
#define SINGLE_FIFO_READ 0x3D // Single FIFO read operation

#define FIFO_SIZE1 0x42 // Camera write FIFO size[7:0] for burst to read
#define FIFO_SIZE2 0x43 // Camera write FIFO size[15:8]
#define FIFO_SIZE3 0x44 // Camera write FIFO size[18:16]

#define SPI_HOLD_US 100

typedef enum {
    APP_ARDU_CAM_STATE_INIT,
    APP_ARDU_CAM_STATE_ARDUCHIP_TEST_BIT,
    APP_ARDU_CAM_STATE_ARDUCHIP_CHANGE_MODE,
    APP_ARDU_CAM_STATE_OV2640_READY_FLUSH_FIFO,
    APP_ARDU_CAM_STATE_CLEAR_FIFO_FLAG,
    APP_ARDU_CAM_STATE_START_CAPTURE,
    APP_ARDU_CAM_STATE_WAIT_FOR_FIFO_DONE,
    APP_ARDU_CAM_STATE_DUMP_IMAGE,
    APP_ARDU_CAM_STATE_ERROR,
} APP_ARDU_CAM_STATES;

typedef struct {
    APP_ARDU_CAM_STATES state;
    bool spi_is_ready;
    uint8_t readReg[APP_ARDUCAM_READ_REG_SIZE];
    uint8_t writeReg[APP_ARDUCAM_WRITE_REG_SIZE];
    uint32_t fifo_length;
} APP_ARDU_CAM_DATA;

// *****************************************************************************
// Private (static, forward) declarations

static bool dump_fifo(uint32_t n_bytes);

static void reset_fifo(void);

static void start_capture(void);

static uint32_t read_fifo_length(void);

static uint8_t spi_read_fifo(void);

static void spi_set_fifo_burst(void);

static void spi_set_bit(uint8_t addr, uint8_t bit);

static void clear_bit(uint8_t addr, uint8_t bit);

static uint8_t spi_get_bit(uint8_t addr, uint8_t bit);

static void spi_set_mode(uint8_t mode);

static uint8_t spi_read_reg(uint8_t addr);

static void spi_write_reg(uint8_t addr, uint8_t data);

static bool cam_spi_xfer(void *tx_buf, size_t tx_size, void *rx_buf,
                         size_t rx_size);

// *****************************************************************************
// Private (static) storage

static APP_ARDU_CAM_DATA appData;

// *****************************************************************************
// Public code

void APP_ARDU_CAM_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_ARDU_CAM_STATE_INIT;
    appData.spi_is_ready = false;

    /* Clear the SPI read and write buffers */
    memset(appData.readReg, 0, sizeof(appData.readReg));
    memset(appData.writeReg, 0, sizeof(appData.writeReg));
}

void APP_ARDU_CAM_Tasks(void) {

    // Dispatch on appData.state
    switch (appData.state) {

    /* Application's initial state. */
    case APP_ARDU_CAM_STATE_INIT: {
        appData.state = APP_ARDU_CAM_STATE_ARDUCHIP_TEST_BIT;
        appData.spi_is_ready = true;
    } break;

    case APP_ARDU_CAM_STATE_ARDUCHIP_TEST_BIT: {

        // wait for I2C camera setup to complete
        if (!APP_OV2640_SENSOR_Task_IsInitialized()) {
            // remain in this state
            appData.state = APP_ARDU_CAM_STATE_ARDUCHIP_TEST_BIT;
            break;
        }

        // Write a byte to the ArduCAM to a test register and read it back to
        // verify SPI operations are working.

        // Register Address : 0x00 - Test Register
        appData.writeReg[0] = ARDUCHIP_TEST1 | ARDUCHIP_WRITE_OP;
        appData.writeReg[1] = 0x55;

        if (!cam_spi_xfer(appData.writeReg, 2, appData.readReg, 4)) {
            printf("SPI Test Bit failed.\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }

        if (appData.readReg[1] == 0x55) {
            /* SPI interface OK. */
            printf("probe succeeded: ");
            appData.state = APP_ARDU_CAM_STATE_ARDUCHIP_CHANGE_MODE;
            break;
        } else {
            /* Failed -- stay in this state to retry */
            printf("probe pending: ");
            appData.state = APP_ARDU_CAM_STATE_ARDUCHIP_TEST_BIT;
            // TODO: holdoff for 100 mSec?
        }
        printf("%02x %02x %02x %02x\r\n", appData.readReg[0],
               appData.readReg[1], appData.readReg[2], appData.readReg[3]);
    } break;

    case APP_ARDU_CAM_STATE_ARDUCHIP_CHANGE_MODE: {
        // Change MCU mode
        // set the bit[7] of the command phase to write
        appData.writeReg[0] = ARDUCHIP_MODE | ARDUCHIP_WRITE_OP;
        appData.writeReg[1] = 0x00;

        if (!cam_spi_xfer(appData.writeReg, sizeof(appData.writeReg), NULL, 0)) {
            printf("Unable to change camera mode\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }
        printf("Changed camera mode\r\n");
        appData.state = APP_ARDU_CAM_STATE_OV2640_READY_FLUSH_FIFO;
    } break;

    case APP_ARDU_CAM_STATE_OV2640_READY_FLUSH_FIFO: {
        // one-time emptying of the fifo (subsequent captures empty
        // it in the dump_image code)

        if (APP_OV2640_SENSOR_Task_IsInitialized() == false) {
            // camera not ready - remain in this state
            appData.state = APP_ARDU_CAM_STATE_OV2640_READY_FLUSH_FIFO;
            break;
        }

        printf("About to send FIFO_CLEAR_MASK command...\r\n");
        // Flush the FIFO to prepare for first capture
        appData.writeReg[0] = ARDUCHIP_FIFO | ARDUCHIP_WRITE_OP;
        appData.writeReg[1] = FIFO_CLEAR_MASK;

        if (!cam_spi_xfer(appData.writeReg, sizeof(appData.writeReg), NULL, 0)) {
            printf("failed to write clear fifo command\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }

        // fifo has been emptied
        appData.state = APP_ARDU_CAM_STATE_CLEAR_FIFO_FLAG;
    } break;

    case APP_ARDU_CAM_STATE_CLEAR_FIFO_FLAG: {
        // Here to clear the FIFO flag prior to capturing an image.
        appData.writeReg[0] = ARDUCHIP_FIFO | ARDUCHIP_WRITE_OP;
        appData.writeReg[1] = FIFO_CLEAR_MASK;

        if (!cam_spi_xfer(appData.writeReg, sizeof(appData.writeReg), NULL, 0)) {
            printf("failed to write clear fifo flag\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }

        // ready to start capture
        appData.state = APP_ARDU_CAM_STATE_START_CAPTURE;
    } break;

    case APP_ARDU_CAM_STATE_START_CAPTURE: {
        // Here to capture an image.
        printf("APP_ARDU_CAM_Task: capturing image\r\n");

        appData.writeReg[0] = ARDUCHIP_FIFO | ARDUCHIP_WRITE_OP;
        appData.writeReg[1] = FIFO_START_MASK;

        if (!cam_spi_xfer(appData.writeReg, sizeof(appData.writeReg), NULL, 0)) {
            printf("APP_ARDU_CAM_Task: Start Capture failed.\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }

        /* CAM Start Capture command completed */
        printf("APP_ARDU_CAM_Task: Start Capture.\r\n");
        appData.state = APP_ARDU_CAM_STATE_WAIT_FOR_FIFO_DONE;
    } break;

    case APP_ARDU_CAM_STATE_WAIT_FOR_FIFO_DONE: {
        // Probe camera to see if capture has completed.
        appData.writeReg[0] = ARDUCHIP_TRIG;
        appData.writeReg[1] = 0x00;

        if (!cam_spi_xfer(appData.writeReg, 2, appData.readReg, 4)) {
            printf("APP_ARDU_CAM_Task: Failed to read capture done.\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }

        // is the Done bit set?
        if (appData.readReg[1] & CAP_DONE_MASK) {
            printf("APP_ARDU_CAM_Task: Capture Done...!\r\n");
            appData.state = APP_ARDU_CAM_STATE_DUMP_IMAGE;
            break;
        }

        // Done bit not set.  Remain in this state to repeatedly poll.
        appData.state = APP_ARDU_CAM_STATE_WAIT_FOR_FIFO_DONE;
    } break;

    case APP_ARDU_CAM_STATE_DUMP_IMAGE: {
        // Get the FIFO Length
        if (!get_fifo_length()) {
            printf("Could not get FIFO length\r\n");
            appData.state = APP_ARDU_CAM_STATE_ERROR;
            break;
        }
        printf("appData.fifo_length = %ld\r\n", appData.fifo_length);

        // if (appData.fifo_length >= OV2640_MAX_FIFO_SIZE) {
        //     printf("APP_ARDU_CAM_Task: FIFO is over Size.\r\n");
        //     appData.state = APP_ARDU_CAM_STATE_CLEAR_FIFO_FLAG;
        //     break;
        // }

        // if (appData.fifo_length == 0) {
        //     printf("APP_ARDU_CAM_Task: FIFO size is zero.\r\n");
        //     appData.state = APP_ARDU_CAM_STATE_CLEAR_FIFO_FLAG;
        //     break;
        // }

        // print contents of image as hex bytes
        dump_fifo(appData.fifo_length);
        // Prepare to capture another image
        appData.state = APP_ARDU_CAM_STATE_CLEAR_FIFO_FLAG;
    } break;

    case APP_ARDU_CAM_STATE_ERROR: {
        // remain in this state (see APP_ARDU_CAM_Task_Failed())
    } break;

    } // switch
}

bool APP_ARDU_CAM_Task_SPIIsReady(void) { return appData.spi_is_ready; }

bool APP_ARDU_CAM_Task_Failed(void) {
    return appData.state == APP_ARDU_CAM_STATE_ERROR;
}

// *****************************************************************************
// Private (static) code

static bool dump_fifo(uint32_t n_bytes) {
    while (n_bytes--) {
        appData.writeReg[0] = BURST_FIFO_READ; // Address to read
        appData.writeReg[1] = 0x00;            // Send a dummy byte

        if (!cam_spi_xfer(appData.writeReg, 1, appData.readReg, 2)) {
            printf("APP_ARDU_CAM_Task: failed to read FIFO bytes\r\n");
            return false;
        }
        printf("%02x ", appData.readReg[1]); // print image byte
        if (n_bytes % 16 == 0) {
            printf("\r\n");
        }
    }
    return true;
}

static void reset_fifo(void) {
    spi_write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

static void start_capture(void) {
    spi_write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

static uint32_t read_fifo_length(void) {
    uint32_t len1 = spi_read_reg(FIFO_SIZE1);
    uint32_t len2 = spi_read_reg(FIFO_SIZE2);
    uint32_t len3 = spi_read_reg(FIFO_SIZE3) & 0x7f;
    uint32_t length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;

    return length;
}

static uint8_t spi_read_fifo(void) {
    return spi_read_reg(SINGLE_FIFO_READ);
}

static void spi_set_fifo_burst(void) {
    uint8_t tx_buf = BURST_FIFO_READ;
    cam_spi_xfer(&tx_buf, sizeof(tx_buf), NULL, 0);
}

static void spi_set_bit(uint8_t addr, uint8_t bit) {
    spi_write_reg(addr, spi_read_reg(addr) | bit);
}

static void clear_bit(uint8_t addr, uint8_t bit) {
    spi_write_reg(addr, spi_read_reg(addr) & ~bit);
}

static uint8_t spi_get_bit(uint8_t addr, uint8_t bit) {
    spi_read_reg(addr) & bit;
}

static void spi_set_mode(uint8_t mode) {
    spi_write_reg(ARDUCHIP_MODE, mode);
}

static uint8_t spi_read_reg(uint8_t addr) {
    uint8_t tx_buf[2] = {addr, 0x00};
    uint8_t rx_buf[4];

    cam_spi_xfer(tx_buf, sizeof(tx_buf), rx_buf, sizeof(rx_buf));

    return rx_buf[1];
}

static void spi_write_reg(uint8_t addr, uint8_t data) {
    uint8_t tx_buf[2] = {addr | ARDUCHIP_WRITE_OP , data};

    cam_spi_xfer(tx_buf, sizeof(tx_buf), NULL, 0);
}

static bool cam_spi_xfer(void *tx_buf, size_t tx_size, void *rx_buf,
                         size_t rx_size) {
    bool success = SPI0_WriteRead(tx_buf, tx_size, rx_buf, rx_size);
    SYSTICK_DelayUs(100);
    return success;
}

// *****************************************************************************
// End of file

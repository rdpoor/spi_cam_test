/**
 * @file arducam.c
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

#include "arducam.h"
#include "arducam.h"
#include "definitions.h"
#include "arducam.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// *****************************************************************************
// Private types and definitions

#define MAX_RETRY_COUNT 5
#define RETRY_DELAY_MS 100

#define MAX_CAPTURE_WAIT_COUNT 15000

// MSB in byte 0 signifies a write operation
#define ARDUCHIP_WRITE_OP 0x80

#define ARDUCHIP_TEST1 0x00 // TEST register

#define ARDUCHIP_MODE 0x02  // Mode register
#define MCU2LCD_MODE 0x00
#define CAM2LCD_MODE 0x01
#define LCD2MCU_MODE 0x02

#define ARDUCHIP_FIFO 0x04 // FIFO and I2C control
#define FIFO_CLEAR_MASK 0x01
#define FIFO_START_MASK 0x02

#define ARDUCHIP_TRIG 0x41   // Trigger source
#define CLEAR_DONE_MASK 0x01 // Write this bit to clear the done bit
#define CAP_DONE_MASK 0x08   // Read as true when capture complete

#define BURST_FIFO_READ 0x3C  // Burst FIFO read operation
#define SINGLE_FIFO_READ 0x3D // Single FIFO read operation

#define FIFO_SIZE1 0x42 // Camera write FIFO size[7:0] for burst to read
#define FIFO_SIZE2 0x43 // Camera write FIFO size[15:8]
#define FIFO_SIZE3 0x44 // Camera write FIFO size[18:16]


/**
 * @brief A container for a register address / register value pair.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} reg_val_t;

/**
 * @brief arducam states.
 */
typedef enum {
    ARDUCAM_STATE_INIT,
    ARDUCAM_STATE_PROBE_SPI,
    ARDUCAM_STATE_RETRY_WAIT,
    ARDUCAM_STATE_START_CAPTURE,
    ARDUCAM_STATE_AWAIT_CAPTURE,
    ARDUCAM_STATE_START_READ_FIFO,
    ARDUCAM_STATE_AWAIT_READ_FIFO,
    ARDUCAM_STATE_SUCCESS,
    ARDUCAM_STATE_ERROR,
} arducam_state_t;

typedef struct {
    arducam_state_t state; // current state
    SYS_TIME_HANDLE delay; // general delay timer
    int retry_count;       // general retry counter
    uint8_t *yuv_buf;
    size_t yuv_buf_capacity;
    uint32_t timestamp_sys; // for telemetry
} arducam_ctx_t;

// *****************************************************************************
// Private (static) storage

/**
 * @brief Singleton arducam context.
 */
static arducam_ctx_t s_arducam;

// *****************************************************************************
// Private (static, forward) declarations

static bool reset_fifo(void);

static bool start_capture(void);

/**
 * @brief Return true if capture completed.
 */
static bool capture_is_complete(void);

/**
 * @brief Clear the capture completed bit.
 */
__attribute__((unused)) static bool clear_capture_complete(void);

static uint32_t read_fifo_length(void);

/**
 * @brief Read a single byte from the FIFO.
 */
__attribute__((unused)) static uint8_t spi_read_fifo_byte(void);

/**
 * @brief Read bytes from the FIFO in burst mode.
 */
static bool spi_read_fifo_burst(uint8_t *buf, size_t buflen);

__attribute__((unused)) static bool spi_set_bit(uint8_t addr, uint8_t bit);

__attribute__((unused)) static bool spi_clear_bit(uint8_t addr, uint8_t bit);

__attribute__((unused)) static uint8_t spi_get_bit(uint8_t addr, uint8_t bit);

/**
 * @brief Set mode.  Mode is one of MCU2LCD_MODE, CAM2LCD_MODE, LCD2MCU_MODE
 */
__attribute__((unused)) static bool spi_set_mode(uint8_t mode);

static uint8_t spi_read_reg(uint8_t addr);

static bool spi_write_reg(uint8_t addr, uint8_t data);

static bool cam_spi_xfer(void *tx_buf, size_t tx_size, void *rx_buf,
                         size_t rx_size);

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
static void await_holdoff(arducam_state_t next_state);


// *****************************************************************************
// Public code

void arducam_init(void) {
    s_arducam.state = ARDUCAM_STATE_INIT;
}

void arducam_step(void) {
    switch (s_arducam.state) {

    case ARDUCAM_STATE_INIT: {
        // remain in this state until a call to arducam_probe_spi,
        // arducam_start_capture or arducam_read_fifo advances the state
    } break;

    case ARDUCAM_STATE_PROBE_SPI: {
        // Here to prove SPI access to the camera

        if (s_arducam.retry_count++ > MAX_RETRY_COUNT) {
            printf("# too many ArduCam retries\r\n");
            s_arducam.state = ARDUCAM_STATE_ERROR;
            break;
        }
        if (!spi_write_reg(ARDUCHIP_TEST1, 0x55)) {
            printf("# SPI probe write failed.\r\n");
            set_holdoff(RETRY_DELAY_MS);
            s_arducam.state = ARDUCAM_STATE_RETRY_WAIT;
            break;
        }

        if (spi_read_reg(ARDUCHIP_TEST1) != 0x55) {
            printf("# SPI probe read failed.\r\n");
            set_holdoff(RETRY_DELAY_MS);
            s_arducam.state = ARDUCAM_STATE_RETRY_WAIT;
            break;
        }
        // success
        s_arducam.state = ARDUCAM_STATE_SUCCESS;
    } break;

    case ARDUCAM_STATE_RETRY_WAIT: {
        // An attempt at reading the VID or PID failed.  Pause briefly before
        // trying again.
        await_holdoff(ARDUCAM_STATE_PROBE_SPI);
    } break;

    case ARDUCAM_STATE_START_CAPTURE: {
        if (!reset_fifo()) {
            printf("# failed to reset fifo\r\n");
            s_arducam.state = ARDUCAM_STATE_ERROR;
            break;
        }
        // if (!clear_capture_complete()) {
        //     printf("# failed to clear capture complete flag\r\n");
        //     s_arducam.state = ARDUCAM_STATE_ERROR;
        //     break;
        // }
        memset(s_arducam.yuv_buf, 0, s_arducam.yuv_buf_capacity); // debugging
        s_arducam.timestamp_sys = SYS_TIME_CounterGet();
        if (!start_capture()) {
            printf("# Start capture failed.\r\n");
            s_arducam.state = ARDUCAM_STATE_ERROR;
            break;
        }
        // Capture successfully started
        s_arducam.retry_count = 0;
        s_arducam.state = ARDUCAM_STATE_AWAIT_CAPTURE;
        break;
    }

    case ARDUCAM_STATE_AWAIT_CAPTURE: {
        if (capture_is_complete()) {
            uint32_t dt = SYS_TIME_CounterGet() - s_arducam.timestamp_sys;
            LED0__Toggle();
            printf("    capture = %3ld tics, ", dt);
            s_arducam.state = ARDUCAM_STATE_SUCCESS;
        } else {
            // remain in this state.
            s_arducam.retry_count += 1;
        }
    } break;

    case ARDUCAM_STATE_START_READ_FIFO: {
        size_t length = read_fifo_length();
        if (length != s_arducam.yuv_buf_capacity) {
            printf("# FIFO length is %d, expected %d\r\n", length,
                   s_arducam.yuv_buf_capacity);
            s_arducam.state = ARDUCAM_STATE_ERROR;
            break;
        }
        s_arducam.timestamp_sys = SYS_TIME_CounterGet();
        if (!spi_read_fifo_burst(s_arducam.yuv_buf, length)) {
            printf("# Could not read FIFO contents\r\n");
            s_arducam.state = ARDUCAM_STATE_ERROR;
            break;
        }
        s_arducam.state = ARDUCAM_STATE_AWAIT_READ_FIFO;
    } break;

    case ARDUCAM_STATE_AWAIT_READ_FIFO: {
        // TODO: perhaps await_holdoff()?
        uint32_t dt = SYS_TIME_CounterGet() - s_arducam.timestamp_sys;
        printf("load = %3ld tics, samples = ", dt);
        // more debugging
        for (int i=0; i<20; i++) {
            printf("%02x ", s_arducam.yuv_buf[i]);
        }
        printf("%02x ", s_arducam.yuv_buf[s_arducam.yuv_buf_capacity-1]);
        s_arducam.state = ARDUCAM_STATE_SUCCESS;
    } break;

    case ARDUCAM_STATE_SUCCESS: {
        // remain in this state until a call to arducam_probe_spi,
        // arducam_start_capture or arducam_read_fifo advances the state
        asm("nop");
    } break;

    case ARDUCAM_STATE_ERROR: {
        // remain in this state
        asm("nop");
    } break;

    } // switch
}

bool arducam_probe_spi(void) {
    s_arducam.retry_count = 0;
    s_arducam.state = ARDUCAM_STATE_PROBE_SPI;
    return true;
}

bool arducam_start_capture(void) {
    s_arducam.state = ARDUCAM_STATE_START_CAPTURE;
    return true;
}

bool arducam_read_fifo(uint8_t *buf, size_t capacity) {
    s_arducam.yuv_buf = buf;
    s_arducam.yuv_buf_capacity = capacity;
    s_arducam.state = ARDUCAM_STATE_START_READ_FIFO;
    return true;
}

bool arducam_succeeded(void) {
    return s_arducam.state == ARDUCAM_STATE_SUCCESS;
}

bool arducam_had_error(void) {
    return s_arducam.state == ARDUCAM_STATE_ERROR;
}

// *****************************************************************************
// Private (static) code

static bool reset_fifo(void) {
    return spi_write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

static bool start_capture(void) {
    return spi_write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

static bool clear_capture_complete(void) {
    // TODO: can we just write register rather than write bit?
    return spi_set_bit(ARDUCHIP_TRIG, CLEAR_DONE_MASK);
}

static bool capture_is_complete(void) {
    return spi_get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
}

static uint32_t read_fifo_length(void) {
    uint32_t len1 = spi_read_reg(FIFO_SIZE1);
    uint32_t len2 = spi_read_reg(FIFO_SIZE2);
    uint32_t len3 = spi_read_reg(FIFO_SIZE3) & 0x7f;
    uint32_t length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;

    return length;
}

static uint8_t spi_read_fifo_byte(void) {
    SYSTICK_DelayUs(10);
    return spi_read_reg(SINGLE_FIFO_READ);
}

static bool spi_read_fifo_burst(uint8_t *buf, size_t buflen) {
    // write (dummy) bytes, read results into buf.
    uint8_t tx_buf[] = {BURST_FIFO_READ};
    return cam_spi_xfer(tx_buf, sizeof(tx_buf), (void *)buf, buflen);
}

static bool spi_set_bit(uint8_t addr, uint8_t bit) {
    return spi_write_reg(addr, spi_read_reg(addr) | bit);
}

static bool spi_clear_bit(uint8_t addr, uint8_t bit) {
    return spi_write_reg(addr, spi_read_reg(addr) & ~bit);
}

static uint8_t spi_get_bit(uint8_t addr, uint8_t bit) {
    return spi_read_reg(addr) & bit;
}

static bool spi_set_mode(uint8_t mode) {
    return spi_write_reg(ARDUCHIP_MODE, mode);
}

static uint8_t spi_read_reg(uint8_t addr) {
    uint8_t rx_buf[2];
    cam_spi_xfer(&addr, sizeof(addr), rx_buf, sizeof(rx_buf));
    return rx_buf[1];
}

static bool spi_write_reg(uint8_t addr, uint8_t data) {
    uint8_t tx_buf[2] = {addr | ARDUCHIP_WRITE_OP, data};

    return cam_spi_xfer(tx_buf, sizeof(tx_buf), NULL, 0);
}

static bool cam_spi_xfer(void *tx_buf, size_t tx_size, void *rx_buf,
                         size_t rx_size) {
    while (SPI0_IsTransmitterBusy()) {
        asm("nop");
    }
    bool success = SPI0_WriteRead(tx_buf, tx_size, rx_buf, rx_size);
    return success;
}

static void set_holdoff(uint32_t ms) {
    s_arducam.delay = SYS_TIME_HANDLE_INVALID;
    if (SYS_TIME_DelayMS(ms, &s_arducam.delay) != SYS_TIME_SUCCESS) {
        asm("nop"); // breakpointable
    }
}

static void await_holdoff(arducam_state_t next_state) {
    if (SYS_TIME_DelayIsComplete(s_arducam.delay)) {
        s_arducam.state = next_state;
    } // else remain in current state...
}

// *****************************************************************************
// End of file

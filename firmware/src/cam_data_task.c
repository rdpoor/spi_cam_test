/**
 * @file cam_data_task.c
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

#include "cam_data_task.h"

#include "definitions.h"
#include "ov2640_spi.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// *****************************************************************************
// Private types and definitions

#define MAX_RETRY_COUNT 5
#define RESET_DELAY_MS 100
#define RETRY_DELAY_MS 100

// Software watchdog: If more than CAPTURE_TIMEOUT_TICS elapse between starting
// a capture and the capture complete bit getting set, restart the capture.
#define CAPTURE_TIMEOUT_TICS 500

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
#define FIFO_CLEAR_WRITE 0x10
#define FIFO_CLEAR_READ 0x20


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
 * @brief cam_data_task states.
 */
typedef enum {
    CAM_DATA_TASK_STATE_INIT,
    CAM_DATA_TASK_STATE_PROBE_SPI,
    CAM_DATA_TASK_STATE_RETRY_WAIT,
    CAM_DATA_TASK_STATE_AWAIT_CAPTURE,
    CAM_DATA_TASK_STATE_START_CAPTURE,
    CAM_DATA_TASK_STATE_SUCCESS,
    CAM_DATA_TASK_STATE_ERROR,
} cam_data_task_state_t;

typedef struct {
    cam_data_task_state_t state; // current state
    SYS_TIME_HANDLE delay;       // general delay timer
    int retry_count;             // general retry counter
    uint8_t *buf_a;              // buffer for captured image (a)
    uint8_t *buf_b;              // buffer for captured image (b)
    uint8_t *put_buf;            // buffer actively being filled
    uint8_t *get_buf;            // filled buffer available to user
    size_t buflen;               // length of image buffers
    uint32_t started_at;         // sys tics when capture started
    uint32_t timestamp_sys;      // for telemetry
    uint32_t frame_count;        // frame count
} cam_data_task_ctx_t;

// *****************************************************************************
// Private (static) storage

/**
 * @brief Singleton cam_data_task context.
 */
static cam_data_task_ctx_t s_cam_data_task;

// *****************************************************************************
// Private (static, forward) declarations

static bool reset_fifo(void);

static bool start_capture(void);

/**
 * @brief Clear the capture completed bit.
 *
 * NOTE: The documentation for ArduCAM Camera Shield Series states:
 * "After capture is done, user have to clear the capture done flag by sending
 * command code 0x41 and write ‘1’ into bit[0] before next capture command."
 * i.e. into ARDUCHIP_TRIG register, write a 1.  But this seems fishy:
 * - the tflmicro demo doesn't do that.
 * - register 0x41 is listed as "read only"
 * - bit[0] is VSYNC status
 * More likely is into ARDUCHIP_FIFO, write a '1', because:
 * - that's what the tflmicro demo does
 * - register 0x04 is read-write
 * - bit[0] is FIFO_RESET
 */
__attribute__((unused)) static bool clear_capture_complete(void);

/**
 * @brief Set mode.  Mode is one of MCU2LCD_MODE, CAM2LCD_MODE, LCD2MCU_MODE
 */
__attribute__((unused)) static bool spi_set_mode(uint8_t mode);

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
static void await_holdoff(cam_data_task_state_t next_state);

static void swap_buffers(void);
static void swap_buffers_aux(uint8_t *put, uint8_t *get);

static void dump_image(uint8_t *buf, size_t n_bytes);

// *****************************************************************************
// Public code

void cam_data_task_init(uint8_t *yuv_buf_a, uint8_t *yuv_buf_b, size_t buflen) {
    s_cam_data_task.buf_a = yuv_buf_a;
    s_cam_data_task.buf_b = yuv_buf_b;
    s_cam_data_task.buflen = buflen;
    s_cam_data_task.put_buf = yuv_buf_a;
    swap_buffers();  // prepare first buffer for writing
    s_cam_data_task.state = CAM_DATA_TASK_STATE_INIT;
    s_cam_data_task.frame_count = 0;
}

void cam_data_task_step(void) {
    switch (s_cam_data_task.state) {

    case CAM_DATA_TASK_STATE_INIT: {
        // remain in this state until a call to cam_data_task_probe_spi,
        // cam_data_task_start_capture or cam_data_task_read_fifo advances the state
    } break;

    // ENTRY POINT FOR cam_data_task_probe_spi()
    case CAM_DATA_TASK_STATE_PROBE_SPI: {
        // Here to prove SPI access to the camera
        uint8_t data;

        if (s_cam_data_task.retry_count++ > MAX_RETRY_COUNT) {
            printf("# too many ArduCam retries\r\n");
            s_cam_data_task.state = CAM_DATA_TASK_STATE_ERROR;
            break;
        }
        if (!ov2640_spi_write_byte(ARDUCHIP_TEST1, 0x55)) {
            printf("# SPI probe write failed.\r\n");
            set_holdoff(RETRY_DELAY_MS);
            s_cam_data_task.state = CAM_DATA_TASK_STATE_RETRY_WAIT;
            break;
        }

        if (!ov2640_spi_read_byte(ARDUCHIP_TEST1, &data)) {
            printf("# SPI probe read failed.\r\n");
            set_holdoff(RETRY_DELAY_MS);
            s_cam_data_task.state = CAM_DATA_TASK_STATE_RETRY_WAIT;
            break;
        }

        if (data != 0x55) {
            printf("# SPI probe data mismatch.\r\n");
            set_holdoff(RETRY_DELAY_MS);
            s_cam_data_task.state = CAM_DATA_TASK_STATE_RETRY_WAIT;
            break;
        }

        // success
        s_cam_data_task.state = CAM_DATA_TASK_STATE_SUCCESS;
    } break;

    case CAM_DATA_TASK_STATE_RETRY_WAIT: {
        // An attempt at reading the VID or PID failed.  Pause briefly before
        // trying again.
        await_holdoff(CAM_DATA_TASK_STATE_PROBE_SPI);
    } break;

    case CAM_DATA_TASK_STATE_AWAIT_CAPTURE: {
        bool complete;

        uint32_t dt = SYS_TIME_CounterGet() - s_cam_data_task.started_at;
        if (dt > CAPTURE_TIMEOUT_TICS) {
            printf("# Timed out waiting for capture completion -- retry\r\n");
            s_cam_data_task.state = CAM_DATA_TASK_STATE_START_CAPTURE;
            break;
        }

        if (!ov2640_spi_test_bit(ARDUCHIP_TRIG, CAP_DONE_MASK, &complete)) {
            printf("# Failed to read completion bit\r\n");
            // remain in this state and retry
            break;
        }

        if (!complete) {
            // not yet ready.  yield to other tasks, but remain in this state.
            s_cam_data_task.state = CAM_DATA_TASK_STATE_AWAIT_CAPTURE;
            break;
        }

        // Camera reports completion.  Get # of bytes in image buffer.
        uint8_t len1, len2, len3;
        if (!ov2640_spi_read_byte(FIFO_SIZE1, &len1) ||
            !ov2640_spi_read_byte(FIFO_SIZE2, &len2) ||
            !ov2640_spi_read_byte(FIFO_SIZE3, &len3)) {
            printf("# failed to read FIFO length\r\n");
            // remain in this state and retry
            break;
        }
        uint32_t length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;

        // Verify correct number of bytes received
        if (length != s_cam_data_task.buflen) {
            printf("# Image buffer is %ld bytes, expected %d\r\n", length,
                   s_cam_data_task.buflen);
            // length error.  Restart capture
            s_cam_data_task.state = CAM_DATA_TASK_STATE_START_CAPTURE;
            break;
        }

        // Bulk read image buffer into current user buffer
        if (!ov2640_spi_read_bytes(BURST_FIFO_READ, s_cam_data_task.put_buf,
                                   length)) {
            printf("# Could not read FIFO contents\r\n");
            // read error.  Restart capture
            s_cam_data_task.state = CAM_DATA_TASK_STATE_START_CAPTURE;
            break;
        }

        swap_buffers();
        // simulate user processing of get_buf
        dump_image(s_cam_data_task.get_buf, s_cam_data_task.buflen);

        uint32_t now_sys = SYS_TIME_CounterGet();
        uint32_t tics = now_sys - s_cam_data_task.timestamp_sys;
        s_cam_data_task.timestamp_sys = now_sys;
        printf("tics: %ld, FPS: %f\n", tics, 1000000.0 / SYS_TIME_CountToUS(tics));
        LED0__Toggle();
        // FALL THROUGH to immediately start a new capture
        // === v === fall through! === v ===
    }

    // ENTRY POINT FOR cam_data_task_start_capture()
    case CAM_DATA_TASK_STATE_START_CAPTURE: {
        if (!reset_fifo()) {
            printf("# failed to reset fifo\r\n");
            // remain this state to restart capture
            s_cam_data_task.state = CAM_DATA_TASK_STATE_START_CAPTURE;
            break;
        }

        if (!start_capture()) {
            printf("# failed to start capture\r\n");
            // remain this state to restart capture
            s_cam_data_task.state = CAM_DATA_TASK_STATE_START_CAPTURE;
            break;
        }

        // Capture has started.  Set software watchdog and start polling for
        // completion bit
        s_cam_data_task.started_at = SYS_TIME_CounterGet();
        s_cam_data_task.state = CAM_DATA_TASK_STATE_AWAIT_CAPTURE;
        break;
    }

    case CAM_DATA_TASK_STATE_SUCCESS: {
        // remain in this state until a call to cam_data_task_probe_spi,
        // cam_data_task_start_capture or cam_data_task_read_fifo advances the state
        asm("nop");
    } break;

    case CAM_DATA_TASK_STATE_ERROR: {
        // remain in this state
        asm("nop");
    } break;

    } // switch
}

bool cam_data_task_probe_spi(void) {
    s_cam_data_task.retry_count = 0;
    s_cam_data_task.state = CAM_DATA_TASK_STATE_PROBE_SPI;
    return true;
}

bool cam_data_task_setup_camera(void) {
    // Placeholder in case any SPI set up needs to be done before starting
    // capture loop.  As of now, it's empty...
    s_cam_data_task.state = CAM_DATA_TASK_STATE_SUCCESS;
    return true;
}

bool cam_data_task_start_capture(void) {
    s_cam_data_task.state = CAM_DATA_TASK_STATE_START_CAPTURE;
    return true;
}

bool cam_data_task_succeeded(void) {
    return s_cam_data_task.state == CAM_DATA_TASK_STATE_SUCCESS;
}

bool cam_data_task_had_error(void) {
    return s_cam_data_task.state == CAM_DATA_TASK_STATE_ERROR;
}

// *****************************************************************************
// Private (static) code

static bool reset_fifo(void) {
    return ov2640_spi_write_byte(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

static bool start_capture(void) {
    return ov2640_spi_write_byte(ARDUCHIP_FIFO, FIFO_START_MASK);
}

static bool clear_capture_complete(void) {
    return ov2640_spi_set_bit(ARDUCHIP_TRIG, CLEAR_DONE_MASK);
}

static bool spi_set_mode(uint8_t mode) {
    return ov2640_spi_write_byte(ARDUCHIP_MODE, mode);
}

static void set_holdoff(uint32_t ms) {
    s_cam_data_task.delay = SYS_TIME_HANDLE_INVALID;
    if (SYS_TIME_DelayMS(ms, &s_cam_data_task.delay) != SYS_TIME_SUCCESS) {
        asm("nop"); // breakpointable
    }
}

static void await_holdoff(cam_data_task_state_t next_state) {
    if (SYS_TIME_DelayIsComplete(s_cam_data_task.delay)) {
        s_cam_data_task.state = next_state;
    } // else remain in current state...
}

static void swap_buffers(void) {
    if (s_cam_data_task.put_buf == s_cam_data_task.buf_a) {
        // buf_a has been filled -- prep buf_b for filling
        swap_buffers_aux(s_cam_data_task.buf_b, s_cam_data_task.buf_a);
    } else {
        // buf_b has been filled -- prep buf_a for filling
        swap_buffers_aux(s_cam_data_task.buf_a, s_cam_data_task.buf_b);
    }
}

static void swap_buffers_aux(uint8_t *put, uint8_t *get) {
    memset(put, 0, s_cam_data_task.buflen);
    s_cam_data_task.put_buf = put;
    s_cam_data_task.get_buf = get;
}

static void dump_image(uint8_t *buf, size_t n_bytes) {
    int skip = n_bytes / 20;
    for (int i=0; i<n_bytes; i+=skip) {
        printf("%02x ", buf[i]);
    }
    // printf("\r\n");
}

#if 0
void capture(uint8_t *imageDat) {
    uint16_t i, count;
    uint8_t value[96 * 96 * 2 + 8];
    uint16_t index = 0;
    while (!get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
    int length = read_fifo_length();
    // printf("the data length: %d\r\n",length);
    cs_select();
    set_fifo_burst(); //Set fifo burst mode
    spi_read_blocking(SPI_PORT, BURST_FIFO_READ, value, length);
    cs_deselect();
    //Flush the FIFO
    flush_fifo();
    //Start capture
    start_capture();
    for (i = 0; i < length - 8; i += 2) {
        imageDat[index++] = value[i];
    }
}

is_complete():
  while (!get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)); // 0x41, 0x08

void set_fifo_burst() {
    uint8_t value;
    spi_read_blocking(SPI_PORT, BURST_FIFO_READ, &value, 1); // 0x3c
}

void flush_fifo(void) {
    write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);  // 0x04, 0x01
}

void start_capture(void) {
    write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);  // 0x04, 0x02
}

void clear_fifo_flag(void) {
    write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);  // 0x04, 0x01
}
#endif

// *****************************************************************************
// End of file

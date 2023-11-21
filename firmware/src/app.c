/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

#include "app.h"
#include "bsp_arducam.h"
#include "definitions.h"
#include <stdarg.h>
#include <stdio.h>

// YUV format is set at 96 x 96 x 2
#define IMG_WIDTH 96
#define IMG_HEIGHT 96
#define IMG_DEPTH 1     // only the 1-byte Y (luminance) channel

typedef enum {
    APP_STATE_INIT,
    APP_STATE_I2C_READY,
    APP_STATE_ARDUCAM_CHECK_SPI,
    APP_STATE_ARDUCAM_CHECK_I2C,
    APP_STATE_ARDUCAM_CONFIGURE,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    app_state_t state;
    DRV_HANDLE i2c_handle;
} app_ctx_t;


static app_ctx_t s_app;

__attribute__((unused))
static uint8_t s_img_buf[IMG_WIDTH * IMG_HEIGHT * IMG_DEPTH];

static inline bool is_even(int n) { return (n & 1) == 0; }

static void display_img(const uint8_t *buf, size_t n_bytes);

static const char *luminance_to_ascii(uint8_t pixel);

void APP_Initialize(void) {
    printf("\n# =========================="
           "\n# ArduCam OV2460 Test v%s",
           APP_VERSION);
    s_app.state = APP_STATE_INIT;
}

void APP_Tasks(void) {
    switch (s_app.state) {

    case APP_STATE_INIT: {
        if (DRV_I2C_Status(sysObj.drvI2C0) == SYS_STATUS_READY) {
            printf("\nAPP_STATE_I2C_READY");
            s_app.state = APP_STATE_I2C_READY;
        } else {
            // remain in this state
        }
    } break;

    case APP_STATE_I2C_READY: {
        s_app.i2c_handle = DRV_I2C_Open(DRV_I2C_INDEX_0, DRV_IO_INTENT_EXCLUSIVE);
        if (s_app.i2c_handle == DRV_HANDLE_INVALID) {
            printf("\nCould not open ARDUCAM I2C");
            s_app.state = APP_STATE_ERROR;
        } else {
            s_app.state = APP_STATE_ARDUCAM_CHECK_SPI;
        }
    } break;

    case APP_STATE_ARDUCAM_CHECK_SPI: {
        // Probe Arducam SPI bus to verify communication
        uint8_t val;

        s_app.state = APP_STATE_ERROR;
        if (!bsp_arducam_spi_write_byte(0x00, 0x55)) {
            printf("\nCould not write test byte to ARDUCAM SPI");
        } else if (!bsp_arducam_spi_read_byte(0x00, &val)) {
            printf("\nCould not read test byte to ARDUCAM SPI");
        } else if (val != 0x55) {
            printf("\nCould not verify ARDUCAM SPI, expected 0x55, got 0x%02x", val);
        } else {
            printf("\nCVerified ARDUCAM SPI");
            s_app.state = APP_STATE_ARDUCAM_CHECK_I2C;
        }
        s_app.state = APP_STATE_ARDUCAM_CHECK_I2C;
    } break;

    case APP_STATE_ARDUCAM_CHECK_I2C: {
        // Probe Arducam I2C bus to verify communication
        uint8_t id_h, id_l;
        s_app.state = APP_STATE_ERROR;

        if (!bsp_arducam_i2c_read_byte(s_app.i2c_handle, 0x0a, &id_h)) {
            printf("\nCould not read ARDUCAM ID_H");
        } else if (id_h != 0x26) {
            printf("\nARDUCAM ID_H mismatch: expected 0x26, got 0x%02x", id_h);
        } else if (!bsp_arducam_i2c_read_byte(s_app.i2c_handle, 0x0b, &id_l)) {
            printf("\nCould not read ARDUCAM ID_L");
        } else if (id_l < 0x40 || id_l > 0x42) {
            printf("\nARDUCAM ID_L mismatch: expected 0x40..42, got 0x%02x", id_l);
        } else {
            printf("\nCVerified ARDUCAM I2C ID: %02x:%02x", id_h, id_l);
            s_app.state = APP_STATE_ARDUCAM_CONFIGURE;
        }
    } break;

    case APP_STATE_ARDUCAM_CONFIGURE: {
        // TBD
    } break;

    case APP_STATE_ERROR: {
        // TBD
    } break;

    } // switch()
}

__attribute__((format(printf, 1, 2))) _Noreturn void
APP_panic(const char *format, ...) {
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    // TODO: if needed, fflush(stdout) before going into infinite loop
    while (1) {
        asm("nop");
    }
}

/**
 * Truly low-res display: print an ASCII char per pixel, skip odd rows
 * in crude attempt to preserve aspect ratio.
 */
__attribute__((unused))
static void display_img(const uint8_t *buf, size_t n_bytes) {
  for (int row = 0; row < IMG_HEIGHT; row++) {
    if (is_even(row)) {
      for (int col = 0; col < IMG_WIDTH; col++) {
        uint8_t pixel = buf[row*IMG_WIDTH + col];
        puts(luminance_to_ascii(pixel));
      }
      putchar('\n');
    }
  }
}

/**
 * Map lumunance 0..255 to five ASCII values: ' ', '░', '▒', '▓', '█'
 */
static const char *luminance_to_ascii(uint8_t pixel) {
    const char *ch[] = {" ", "░", "▒", "▓", "█"};
    return ch[pixel / 52];
}

/*******************************************************************************
 End of File
 */

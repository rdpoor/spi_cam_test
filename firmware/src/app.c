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
#include "app_ov2640_sensor.h"
#include "definitions.h"
#include <stdarg.h>
#include <stdio.h>

// YUV format is set at 96 x 96 x 2
#define IMG_WIDTH 96
#define IMG_HEIGHT 96
#define IMG_DEPTH 1     // only the 1-byte Y (luminance) channel

__attribute__((unused))
static uint8_t s_img_buf[IMG_WIDTH * IMG_HEIGHT * IMG_DEPTH];

__attribute__((unused))
static inline bool is_even(int n) { return (n & 1) == 0; }

__attribute__((unused))
static void display_img(const uint8_t *buf, size_t n_bytes);

__attribute__((unused))
static const char *luminance_to_ascii(uint8_t pixel);

void APP_Initialize(void) {
    printf("\n# =========================="
           "\n# ArduCam OV2460 Test v%s\r\n",
           APP_VERSION);
    APP_OV2640_SENSOR_Initialize();
}

void APP_Tasks(void) {
    if (APP_OV2640_SENSOR_Task_Failed()) {
        APP_panic("initialization failed\r\n");
    } else if (APP_OV2640_SENSOR_Task_IsInitialized()) {
        APP_panic("initialization succeeded\r\n");
    } else {
        APP_OV2640_SENSOR_Tasks();
    }
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
__attribute__((unused))
static const char *luminance_to_ascii(uint8_t pixel) {
    const char *ch[] = {" ", "░", "▒", "▓", "█"};
    return ch[pixel / 52];
}

/*******************************************************************************
 End of File
 */

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
#include "arducam.h"
#include <stdarg.h>
#include <stdio.h>

// YUV format is set at 96 x 96 x 2
#define IMG_WIDTH 96
#define IMG_HEIGHT 96
#define IMG_DEPTH 1     // only the 1-byte Y (luminance) channel

static uint8_t s_img_buf[IMG_WIDTH * IMG_HEIGHT * IMG_DEPTH];

static inline bool is_even(int n) { return (n & 1) == 0; }

static void display_img(const uint8_t *buf, size_t n_bytes);

static const char *luminance_to_ascii(uint8_t pixel);

void APP_Initialize(void) {
    printf("\n# =========================="
           "\n# ArduCam OV2460 Test v%s",
           APP_VERSION);
    arducam_system_init();
    if (arducam_bus_detect()) {
        APP_panic("\nArducam SPI bus detect failed");
    }
    if (arducam_camera_probe()) {
        APP_panic("\nArducam camera probe failed");
    }
    arducam_camera_init(ARDUCAM_FMT_YUV); // 96 x 96 x 2
}

void APP_Tasks(void) {
    arducam_capture(s_img_buf);
    display_img(s_img_buf, sizeof(s_img_buf));
    asm("nop");
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

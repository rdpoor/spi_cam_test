Autocorrected to model
Conversation opened. 1 unread message.

Skip to content
Using Gmail with screen readers
5 of 58,524
brainchip readme
Inbox
Robert Poor <rdpoor@gmail.com>

Attachments4:42 PM (1 hour ago)

to me

 One attachment  •  Scanned by Gmail


# spi_cam_test
Bring up SPi/I2C base OV2640 camera on Microchip MPLAB.X/ Harmony platform

## Overview

This code project connects an OV2640 2MP CMOS camara with a SPI/I2C interface
("ArduCam") to a SAMV71 Xplained ULTRA development board.  It is constructed
using MPLAB.X v6.15 as the IDE.

The main goal is the implementation and verification of these API functions:
```
    void ov2650_init(void *tbd);
    void ov2650_start(void);
    void ov2650_stop(void);
    ov2650_status_t ov2650_status(void);
```
and this callback function:
```
    void ov2650_cb(ov2650_status_t status, uint8_t *buf, size_t bufsiz);
```

## Useful Links

* https://www.arducam.com
* https://github.com/ArduCAM
* https://github.com/ArduCAM/Arduino
* https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-42408-SAMV71-Xplained-Ultra_User-Guide.pdf
* https://www.uctronics.com/download/cam_module/OV2640DS.pdf

Note: the following is for an ISI sensor and ATMEL Start, so it has limited
relevance here:

* https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-42702-Image-Sensor-Interface-in-SAM-V7-E7-S7-Devices_ApplicationNote_AT12861.pdf

## Signal Mapping

For the prototype, we use Dupont jumper wires between the ov2650 breakout board
and EXT.2 on the SAMV71 XULT board.

| SIGNAL     | ArduCam | XULT EXT2 | V71 Port| SAMV71 I/O | color | J506 |SPI|
| -----------| --------| --------- | ------- | ---------- | ----- | ---- |---|
| SPI.CS_    | 1       | EXT2.15   | PD27    | SPI0_NPCS3 | VIO   | -na- |   |
| SPI.MOSI   | 2       | EXT2.16   | PD21    | SPI0_MOSI  | BLU   |J506.4|YEL|
| SPI.MISO   | 3       | EXT2.17   | PD20    | SPI0_MISO  | GRN   |J506.1|BRN|
| SPI.SCLK   | 4       | EXT2.18   | PD22    | SPI0_SPCK  | YEL   |J506.3|ORA|
| GND        | 5       | EXT2.19   | GND     | GND        | BLK   |J506.6|BLK|
| V3P3       | 6       | EXT2.20   | VCC     | VCC        | RED   | -na- |   |
| I2C.SDA    | 7       | EXT2.11   | PA03    | TWCK1      | ORA   | -na- |   |
| I2C.SCL    | 8       | EXT2.12   | PA04    | TWCK0      | BRN   | -na- |   |

## Extra Credit

It would be fun to write a simple app using the API that displays the camera
output in real-time using ASCII art, along the lines of the
[video-to_ascii](https://github.com/joelibaceta/video-to-ascii/tree/master)
github repository.

## Online Documents

The only online documentation appears to be example code.  >:[

BMP output example:

Arduino/ArduCAM/examples/mini/ArduCAM_Mini_2MP_OV2640_functions/ArduCAM_Mini_2MP_OV2640_functions.ino

YUV output example:

/Users/r/Projects/BrainChip/git/RPI-Pico-Cam/tflmicro/Arducam/src/arducam.c

## Alternate YUV and JPG capture

For the demo, we want to capture YUV 96 x 96 in order to feed the inference
engine, but we also want to monitor the image on a host computer screen. Because
96 x 96 is to small for monitoring, we want a larger format.  However, pushing
bytes over the serial line is a bottleneck so we need to use image compression,
i.e. jpeg.

THe plan is to generate one or more images in YUV format and send them to the
inference engine, then generate 1 image in JPEG format and send it as a byte
stream over the serial port to the host for display.


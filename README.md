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

This has links to multiple PDF files that explain some of the register
operations:

https://www.arducam.com/product/arducam-2mp-spi-camera-b0067-arduino/
http://www.uctronics.com/download/Amazon/ArduCAM_Mini_2MP_Camera_Shield_Hardware_Application_Note.pdf
https://www.uctronics.com/download/Mechanical_Drawing/UC-474(B0067).STEP
https://www.uctronics.com/download/Amazon/ArduCAM_Mini_2MP_Camera_Shield_DS.pdf
https://www.arducam.com/downloads/shields/ArduCAM_Camera_Shield_Software_Application_Note.pdf
https://www.uctronics.com/download/Amazon/B0067-B0068-Pico.pdf

YUV output code example:

/Users/r/Projects/BrainChip/git/RPI-Pico-Cam/tflmicro/Arducam/src/arducam.c

```
TfLiteStatus GetImage() =>
  init:
      arducam.systemInit()
      arducam.busDetect():
          spi_write_reg(0x00, 0x55)
          verify spi_read_reg(0x00) == 0x55)
      arducam.cameraProbe():
          i2c_read(0x0A, &id_H);
          i2c_read(0x0B, &id_L);
          if (id_H == 0x26 && (id_L == 0x40||id_L == 0x41 || id_L == 0x42)) {
      arducam.cameraInit(YUV):
        wrSensorReg8_8(0xff, 0x01);
        wrSensorReg8_8(0x12, 0x80);
        sleep_ms(100);
        wrSensorRegs8_8(OV2640_YUV_96x96);
        flush_fifo();
        start_capture();


  loop:
      capture((uint8_t *)image_data));
          wait for cap done bit
          read fifo length
          set fifo burst
          read fifo (buffer[96*96*2+8], length)
          flush fifo
          start capture
          convert YUV to RGB

```

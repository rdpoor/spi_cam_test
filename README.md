# spi_cam_test
Bring up SPi/I2C base OV2640 camera on Microchip / Harmony platform

## Overview
This code/project sketch connects an OV2640 2MP CMOS camara with a SPI/I2C interface ("ArduCam")
to a SAMV71 Xplained ULTRA development board.

The main goal is the implementation of these functions:
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

## Beyond that...

It would be fun to write a simple app using the API that displays the camera
output in real-time using ASCII art, along the lines of the 
[video-to_ascii](https://github.com/joelibaceta/video-to-ascii/tree/master)
github repository.

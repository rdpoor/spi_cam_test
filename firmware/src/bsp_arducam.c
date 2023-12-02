#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"
#include "arducam.h"
#include "ov2640.h"
uint8_t cameraCommand = 0;

enum pico_error_codes {
    PICO_OK = 0,
    PICO_ERROR_NONE = 0,
    PICO_ERROR_TIMEOUT = -1,
    PICO_ERROR_GENERIC = -2,
    PICO_ERROR_NO_DATA = -3,
    PICO_ERROR_NOT_PERMITTED = -4,
    PICO_ERROR_INVALID_ARG = -5,
    PICO_ERROR_IO = -6,
    PICO_ERROR_BADAUTH = -7,
    PICO_ERROR_CONNECT_FAILED = -8,
    PICO_ERROR_INSUFFICIENT_RESOURCES = -9,
};

/*! \brief Attempt to write specified number of bytes to address, blocking
 *  \ingroup hardware_i2c
 *
 * \param i2c Either \ref i2c0 or \ref i2c1
 * \param addr 7-bit address of device to write to
 * \param src Pointer to data to send
 * \param len Length of data in bytes to send
 * \param nostop  If true, master retains control of the bus at the end of the transfer (no Stop is issued),
 *           and the next transfer will begin with a Restart rather than a Start.
 * \return Number of bytes written, or PICO_ERROR_GENERIC if address not acknowledged, no device present.
 */
static int i2c_write_blocking(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop);

/*! \brief  Attempt to read specified number of bytes from address, blocking
 *  \ingroup hardware_i2c
 *
 * \param i2c Either \ref i2c0 or \ref i2c1
 * \param addr 7-bit address of device to read from
 * \param dst Pointer to buffer to receive data
 * \param len Length of data in bytes to receive
 * \param nostop  If true, master retains control of the bus at the end of the transfer (no Stop is issued),
 *           and the next transfer will begin with a Restart rather than a Start.
 * \return Number of bytes read, or PICO_ERROR_GENERIC if address not acknowledged or no device present.
 */
static int i2c_read_blocking(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop);

/*! \brief Write/Read to/from an SPI device
 *  \ingroup hardware_spi
 *
 * Write \p len bytes from \p src to SPI. Simultaneously read \p len bytes from SPI to \p dst.
 * Blocks until all data is transferred. No timeout, as SPI hardware always transfers at a known data rate.
 *
 * \param spi SPI instance specifier, either \ref spi0 or \ref spi1
 * \param src Buffer of data to write
 * \param dst Buffer for read data
 * \param len Length of BOTH buffers
 * \return Number of bytes written/read
*/
static int spi_write_read_blocking(spi_inst_t *spi, const uint8_t *src, uint8_t *dst, size_t len);

/*! \brief Write to an SPI device, blocking
 *  \ingroup hardware_spi
 *
 * Write \p len bytes from \p src to SPI, and discard any data received back
 * Blocks until all data is transferred. No timeout, as SPI hardware always transfers at a known data rate.
 *
 * \param spi SPI instance specifier, either \ref spi0 or \ref spi1
 * \param src Buffer of data to write
 * \param len Length of \p src
 * \return Number of bytes written/read
 */
static int spi_write_blocking(spi_inst_t *spi, const uint8_t *src, size_t len);

/*! \brief Wait for the given number of milliseconds before returning
 *
 * \param ms the number of milliseconds to sleep
 */
static void sleep_ms(uint32_t ms);

// I2C functions

int rdSensorReg8_8(uint8_t regID, uint8_t* regDat ){
    i2c_write_blocking(I2C_PORT, arducam.slave_address, &regID, 1, true );
    i2c_read_blocking(I2C_PORT, arducam.slave_address, regDat,  1, false );
}

int wrSensorReg8_8(uint8_t regID, uint8_t regDat ){
    uint8_t buf[2];
    buf[0] = regID;
    buf[1] = regDat;
    i2c_write_blocking(I2C_PORT, arducam.slave_address, buf,  2, true );
}

int wrSensorRegs8_8(const struct sensor_reg reglist[])
{
  int err = 0;
  unsigned int reg_addr = 0;
  unsigned int reg_val = 0;
  const struct sensor_reg *next = reglist;
  while ((reg_addr != 0xff) | (reg_val != 0xff))
  {
    reg_addr =next->reg;
    reg_val = next->val;
    err = wrSensorReg8_8(reg_addr, reg_val);
    sleep_ms(10);   // wtf?
    next++;
  }
  return err;
}

// SPI functions

void cs_select(void) {
    // TODO: how much setup and hold time is really required?
    asm volatile("nop \n nop \n nop");
    SPI0_CS__Clear();     // Active low
    asm volatile("nop \n nop \n nop");
}

void cs_deselect(void) {
    // TODO: how much setup and hold time is really required?
    asm volatile("nop \n nop \n nop");
    SPI0_CS__Set();      // Active low
    asm volatile("nop \n nop \n nop");
}

void write_reg(uint8_t address, uint8_t value){
    uint8_t buf[2];
    buf[0] = address|WRITE_BIT ;  // remove read bit as this is a write
    buf[1] = value;
    cs_select();
    spi_write_blocking(SPI_PORT, buf, 2);
    cs_deselect();
    sleep_ms(10);    // wtf??
}

 uint8_t read_reg(uint8_t address)
{
    uint8_t value = 0;
    address = address& 0x7f;
     cs_select();
    spi_write_blocking(SPI_PORT, &address, 1);
    sleep_ms(10);
    spi_read_blocking(SPI_PORT, 0, &value, 1);
    cs_deselect();
    sleep_ms(10);
    return value;
}

unsigned char read_fifo(void)
{
    unsigned char data;
    data = read_reg(SINGLE_FIFO_READ);
    return data;
}
void set_fifo_burst()
{
    uint8_t value;
    spi_read_blocking(SPI_PORT, BURST_FIFO_READ, &value, 1);
}


void flush_fifo(void)
{
    write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

void start_capture(void)
{
    write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

void clear_fifo_flag(void )
{
    write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

unsigned int read_fifo_length()
{
    unsigned int len1,len2,len3,len=0;
    len1 = read_reg(FIFO_SIZE1);
  len2 = read_reg(FIFO_SIZE2);
  len3 = read_reg(FIFO_SIZE3) & 0x7f;
  len = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;
    return len; 
}

//Set corresponding bit  
void set_bit(unsigned char addr, unsigned char bit)
{
    unsigned char temp;
    temp = read_reg(addr);
    write_reg(addr, temp | bit);
}
//Clear corresponding bit 
void clear_bit(unsigned char addr, unsigned char bit)
{
    unsigned char temp;
    temp = read_reg(addr);
    write_reg(addr, temp & (~bit));
}

//Get corresponding bit status
unsigned char get_bit(unsigned char addr, unsigned char bit)
{
  unsigned char temp;
  temp = read_reg(addr);
  temp = temp & bit;
  return temp;
}

void OV2640_set_JPEG_size(unsigned char size)
{
    switch(size)
    {
        case res_160x120:
            wrSensorRegs8_8(OV2640_160x120_JPEG);
            break;
        case res_176x144:
            wrSensorRegs8_8(OV2640_176x144_JPEG);
            break;
        case res_320x240:
            wrSensorRegs8_8(OV2640_320x240_JPEG);
            break;
        case res_352x288:
        wrSensorRegs8_8(OV2640_352x288_JPEG);
            break;
        case res_640x480:
            wrSensorRegs8_8(OV2640_640x480_JPEG);
            break;
        case res_800x600:
            wrSensorRegs8_8(OV2640_800x600_JPEG);
            break;
        case res_1024x768:
            wrSensorRegs8_8(OV2640_1024x768_JPEG);
            break;
        case res_1280x1024:
            wrSensorRegs8_8(OV2640_1280x1024_JPEG);
            break;
        case res_1600x1200:
            wrSensorRegs8_8(OV2640_1600x1200_JPEG);
            break;
        default:
            wrSensorRegs8_8(OV2640_320x240_JPEG);
            break;
    }
}

void ov2640Init(){
    wrSensorReg8_8(0xff, 0x01);
    wrSensorReg8_8(0x12, 0x80);
    wrSensorRegs8_8(OV2640_JPEG_INIT);
    wrSensorRegs8_8(OV2640_YUV422);
    wrSensorRegs8_8(OV2640_JPEG);
    wrSensorReg8_8(0xff, 0x01);
    wrSensorReg8_8(0x15, 0x00);
    wrSensorRegs8_8(OV2640_320x240_JPEG);
}
void singleCapture(void){
   int i , count;
   uint8_t value[1024*40];
   //Flush the FIFO
   flush_fifo();
   //Start capture
   start_capture(); 
   while(!get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK)){;}
   int length = read_fifo_length();
   count = length;
   i = 0 ;
   cs_select();
   set_fifo_burst();//Set fifo burst mode
   spi_read_blocking(SPI_PORT, BURST_FIFO_READ,value, length);
   uart_write_blocking(UART_ID, value, length);
     count = 0;
     cs_deselect();
}
uint8_t spiBusDetect(void){
    write_reg(0x00, 0x55);
    if(read_reg(0x00) == 0x55){
        printf("SPI bus normal");
        return 0;
    }else{
        printf("SPI bus error\r\n");
        return 1;
    }      
}
uint8_t ov2640Probe(){
    uint8_t id_H,id_L;
    rdSensorReg8_8(0x0A,&id_H);
    rdSensorReg8_8(0x0B,&id_L);
    if(id_H == 0x26 && (id_L == 0x40 ||id_L == 0x41 || id_L == 0x42)){
        printf("ov2640 detected\r\n");
        return 0;
    }else{
        printf("Can't find ov2640 sensor\r\n");
        return 1;
    }
}


struct camera_operate arducam = {
    .slave_address = 0x30,
    .systemInit  = picoSystemInit,
    .busDetect   = spiBusDetect,
    .cameraProbe = ov2640Probe,
    .cameraInit  = ov2640Init,
    .setJpegSize = OV2640_set_JPEG_size,
};

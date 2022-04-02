
#ifndef __SPI_H__
#define __SPI_H__

#include <driver/spi_master.h>
#include <stdint.h>
#include "sdkconfig.h"

#ifdef CONFIG_TRS_IO_ENABLE_XRAY

//-----XRAY configuration--------------------------------------------

// SPI
#define SPI_SPEED_MHZ 20
#define SPI_DIGI_POT_SPEED_MHZ 1

#define SPI_PIN_NUM_MISO GPIO_NUM_22
#define SPI_PIN_NUM_MOSI GPIO_NUM_21
#define SPI_PIN_NUM_CLK GPIO_NUM_25
#define SPI_PIN_NUM_CS_CMOD GPIO_NUM_26
#define SPI_PIN_NUM_CS_MCP4351 GPIO_NUM_19

#define FPGA_COOKIE 0xaf

#define FPGA_CMD_GET_COOKIE 0
#define FPGA_CMD_BRAM_POKE 1
#define FPGA_CMD_BRAM_PEEK 2
#define FPGA_CMD_DBUS_READ 3
#define FPGA_CMD_DBUS_WRITE 4
#define FPGA_CMD_TRS_IO_DONE 5
#define FPGA_CMD_SET_BREAKPOINT 6
#define FPGA_CMD_CLEAR_BREAKPOINT 7
#define FPGA_CMD_XRAM_POKE_CODE 8
#define FPGA_CMD_XRAM_POKE_DATA 9
#define FPGA_CMD_XRAM_PEEK_DATA 10
#define FPGA_ENABLE_BREAKPOINTS 11
#define FPGA_DISABLE_BREAKPOINTS 12
#define FPGA_CMD_XRAY_RESUME 13
#define FPGA_CMD_SET_FULL_ADDR 14
#define FPGA_CMD_GET_FPGA_VERSION 15


uint8_t spi_get_cookie();
uint8_t spi_get_fpga_version();
void spi_bram_poke(uint16_t addr, uint8_t data);
uint8_t spi_bram_peek(uint16_t addr);
void spi_xram_poke_code(uint8_t addr, uint8_t data);
void spi_xram_poke_data(uint8_t addr, uint8_t data);
uint8_t spi_xram_peek_data(uint8_t addr);
uint8_t spi_dbus_read();
void spi_dbus_write(uint8_t d);
void spi_set_breakpoint(uint8_t n, uint16_t addr);
void spi_clear_breakpoint(uint8_t n);
void spi_xray_resume();
void spi_trs_io_done();
void spi_set_full_addr(bool flag);
void spi_set_screen_color(uint8_t color);

#else

//-----M1 & MIII configuration--------------------------------------

// SPI
#define SPI_SPEED_MHZ 10

#ifdef CONFIG_TRS_IO_MODEL_1
#define SPI_PIN_NUM_MISO GPIO_NUM_22
#define SPI_PIN_NUM_MOSI GPIO_NUM_21
#define SPI_PIN_NUM_CLK GPIO_NUM_25
#define SPI_PIN_NUM_CS_MCP23S17 GPIO_NUM_26
#else
#define SPI_PIN_NUM_MISO GPIO_NUM_17
#define SPI_PIN_NUM_MOSI GPIO_NUM_13
#define SPI_PIN_NUM_CLK GPIO_NUM_14
#define SPI_PIN_NUM_CS_MCP23S08 GPIO_NUM_15
#endif


/*
 * MCP23S17
 */
#define MCP23S17 spi_mcp23S17_h
extern spi_device_handle_t spi_mcp23S17_h;

#define MCP23S17_IODIRA 0x00
#define MCP23S17_IPOLA 0x02
#define MCP23S17_GPINTENA 0x04
#define MCP23S17_DEFVALA 0x06
#define MCP23S17_INTCONA 0x08
#define MCP23S17_IOCONA 0x0A
#define MCP23S17_GPPUA 0x0C
#define MCP23S17_INTFA 0x0E
#define MCP23S17_INTCAPA 0x10
#define MCP23S17_GPIOA 0x12
#define MCP23S17_OLATA 0x14


#define MCP23S17_IODIRB 0x01
#define MCP23S17_IPOLB 0x03
#define MCP23S17_GPINTENB 0x05
#define MCP23S17_DEFVALB 0x07
#define MCP23S17_INTCONB 0x09
#define MCP23S17_IOCONB 0x0B
#define MCP23S17_GPPUB 0x0D
#define MCP23S17_INTFB 0x0F
#define MCP23S17_INTCAPB 0x11
#define MCP23S17_GPIOB 0x13
#define MCP23S17_OLATB 0x15

/*
 * MCP23S08
 */
#define MCP23S08_IODIR 0x00
#define MCP23S08_IPOL 0x01
#define MCP23S08_GPINTEN 0x02
#define MCP23S08_DEFVAL 0x03
#define MCP23S08_INTCON 0x04
#define MCP23S08_IOCON 0x05
#define MCP23S08_GPPU 0x06
#define MCP23S08_INTF 0x07
#define MCP23S08_INTCAP 0x08
#define MCP23S08_GPIO 0x09
#define MCP23S08_OLAT 0x0a

#define MCP23S08_ODR (1 << 2)


#define ACK_CHECK_EN 0x1
#define ACK_CHECK_DIS 0x0
#define ACK_VAL 0x0
#define NACK_VAL 0x1

#ifdef CONFIG_TRS_IO_MODEL_1
void writePortExpander(spi_device_handle_t dev, uint8_t cmd, uint8_t data);
uint8_t readPortExpander(spi_device_handle_t dev, uint8_t reg);
#else
void writePortExpander(uint8_t cmd, uint8_t data);
uint8_t readPortExpander(uint8_t reg);
#endif

#endif

void init_spi();

#endif

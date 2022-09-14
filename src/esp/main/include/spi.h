
#ifndef __SPI_H__
#define __SPI_H__

#include <driver/spi_master.h>
#include <stdint.h>
#include "sdkconfig.h"


//-----XRAY configuration--------------------------------------------

// SPI
#define SPI_SPEED_MHZ 20
#define SPI_DIGI_POT_SPEED_MHZ 1

#ifdef CONFIG_TRS_IO_MODEL_1
#define SPI_PIN_NUM_MISO GPIO_NUM_22
#define SPI_PIN_NUM_MOSI GPIO_NUM_21
#define SPI_PIN_NUM_CLK GPIO_NUM_25
#define SPI_PIN_NUM_CS_CMOD GPIO_NUM_26
#else
#define SPI_PIN_NUM_MISO GPIO_NUM_5
#define SPI_PIN_NUM_MOSI GPIO_NUM_19
#define SPI_PIN_NUM_CLK GPIO_NUM_14
#define SPI_PIN_NUM_CS_CMOD GPIO_NUM_15
#endif

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
#define FPGA_CMD_GET_PRINTER_BYTE 16
#define FPGA_CMD_SET_SCREEN_COLOR 17
#define FPGA_CMD_ABUS_READ 18


uint8_t spi_get_cookie();
uint8_t spi_get_fpga_version();
uint8_t spi_get_printer_byte();
void spi_bram_poke(uint16_t addr, uint8_t data);
uint8_t spi_bram_peek(uint16_t addr);
void spi_xram_poke_code(uint8_t addr, uint8_t data);
void spi_xram_poke_data(uint8_t addr, uint8_t data);
uint8_t spi_xram_peek_data(uint8_t addr);
uint8_t spi_dbus_read();
void spi_dbus_write(uint8_t d);
uint8_t spi_abus_read();
void spi_set_breakpoint(uint8_t n, uint16_t addr);
void spi_clear_breakpoint(uint8_t n);
void spi_xray_resume();
void spi_trs_io_done();
void spi_set_full_addr(bool flag);
void spi_set_screen_color(uint8_t color);

void init_spi();

#endif

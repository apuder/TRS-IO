
#ifndef __SPI_H__
#define __SPI_H__

#include <driver/spi_master.h>
#include <stdint.h>
#include "sdkconfig.h"


//-----XRAY configuration--------------------------------------------

// SPI
#define SPI_SPEED_MHZ 20
#define SPI_DIGI_POT_SPEED_MHZ 1

#if defined(CONFIG_TRS_IO_MODEL_1)
#define SPI_PIN_NUM_MISO GPIO_NUM_22
#define SPI_PIN_NUM_MOSI GPIO_NUM_21
#define SPI_PIN_NUM_CLK GPIO_NUM_25
#define SPI_PIN_NUM_CS_CMOD GPIO_NUM_26
#elif defined(CONFIG_TRS_IO_PP)
#define SPI_PIN_NUM_MISO GPIO_NUM_19
#define SPI_PIN_NUM_MOSI GPIO_NUM_25
#define SPI_PIN_NUM_CLK GPIO_NUM_26
#define SPI_PIN_NUM_CS_CMOD GPIO_NUM_22
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
#define FPGA_CMD_GET_MODE 16
#define FPGA_CMD_SET_SCREEN_COLOR 17
#define FPGA_CMD_ABUS_READ 18
#define FPGA_CMD_SEND_KEYB 19
#define FPGA_CMD_PTRS_RST 20
#define FPGA_CMD_Z80_PAUSE 21
#define FPGA_CMD_Z80_RESUME 22
#define FPGA_CMD_Z80_DSP_SET_ADDR 23
#define FPGA_CMD_Z80_DSP_POKE 24
#define FPGA_CMD_Z80_DSP_PEEK 25
#define FPGA_CMD_SET_LED 26
#define FPGA_CMD_GET_CONFIG 27
#define FPGA_CMD_SET_CASS_IN 28
#define FPGA_CMD_SET_SPI_CTRL_REG 29
#define FPGA_CMD_SET_SPI_DATA 30
#define FPGA_CMD_GET_SPI_DATA 31
#define FPGA_CMD_SET_ESP_STATUS 32
#define FPGA_CMD_SET_PRINTER_EN 33


#define PTRS_CONFIG_DIP_1   (1 << 0)
#define PTRS_CONFIG_DIP_2   (1 << 1)
#define PTRS_CONFIG_DIP_3   (1 << 2)
#define PTRS_CONFIG_DIP_4   (1 << 3)
#define PTRS_CONFIG_HIRES   (1 << 4)
#define PTRS_CONFIG_WIDE    (1 << 5)
#define PTRS_CONFIG_80_COLS (1 << 6)

uint8_t spi_get_cookie();
uint8_t spi_get_fpga_version();
uint8_t spi_get_mode();
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
void spi_set_printer_en(bool enable);
void spi_send_keyb(uint8_t idx, uint8_t mask);
void spi_ptrs_rst();
void spi_z80_pause();
void spi_z80_resume();
void spi_z80_dsp_set_addr(uint16_t addr);
void spi_z80_dsp_poke(uint8_t v);
uint8_t spi_z80_dsp_peek();
void spi_set_led(bool r, bool g, bool b);
uint8_t spi_get_config();
void spi_set_cass_in();
void spi_set_spi_ctrl_reg(uint8_t reg);
void spi_set_spi_data(uint8_t data);
uint8_t spi_get_spi_data();
void spi_set_esp_status(uint8_t status);

void init_spi();

#endif

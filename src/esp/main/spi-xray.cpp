
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task.h"
#include "esp_event_loop.h"
#include "spi.h"

#define MCP23S_CHIP_ADDRESS 0x40
#define MCP23S_WRITE 0x00
#define MCP23S_READ  0x01

static spi_bus_config_t spi_bus;

static spi_device_interface_config_t spi_cmod;
spi_device_handle_t spi_cmod_h;


void set_led(uint8_t on)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA;
  trans.length = 2 * 8;
  trans.rxlength = 0;
  trans.tx_data[0] = FPGA_CMD_SET_LED;
  trans.tx_data[1] = on;

  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans);
  ESP_ERROR_CHECK(ret);
}

void bram_poke(uint16_t addr, uint8_t data)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA;
  trans.length = 4 * 8;
  trans.rxlength = 0;
  trans.tx_data[0] = FPGA_CMD_BRAM_POKE;
  trans.tx_data[1] = addr & 0xff;
  trans.tx_data[2] = addr >> 8;
  trans.tx_data[3] = data;

  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans);
  ESP_ERROR_CHECK(ret);
}

uint8_t bram_peek(uint16_t addr)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  trans.length = 3 * 8;
  trans.rxlength = 1 * 8;
  trans.tx_data[0] = FPGA_CMD_BRAM_PEEK;
  trans.tx_data[1] = addr & 0xff;
  trans.tx_data[2] = addr >> 8;
  trans.tx_data[3] = 0;

  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans);
  ESP_ERROR_CHECK(ret);

  return trans.rx_data[0];
}

uint8_t spi_dbus_read()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA;
  trans.base.cmd = FPGA_CMD_DBUS_READ;
  trans.address_bits = 0 * 8;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  //spi_device_acquire_bus(spi_cmod_h, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  //spi_device_release_bus(spi_cmod_h);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_dbus_write(uint8_t d)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_DBUS_WRITE;
  trans.address_bits = 1 * 8;
  trans.base.addr = d;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  //spi_device_acquire_bus(spi_cmod_h, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  //spi_device_release_bus(spi_cmod_h);
  ESP_ERROR_CHECK(ret);
}

void spi_trs_io_done()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.cmd = FPGA_CMD_TRS_IO_DONE;
  trans.base.length = 0 * 8;

  //spi_device_acquire_bus(spi_cmod_h, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  //spi_device_release_bus(spi_cmod_h);
  ESP_ERROR_CHECK(ret);
}

void init_spi()
{
  spi_bus = {
        .mosi_io_num = SPI_PIN_NUM_MOSI,
        .miso_io_num = SPI_PIN_NUM_MISO,
        .sclk_io_num = SPI_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
  };
  esp_err_t ret = spi_bus_initialize(HSPI_HOST, &spi_bus, 1);
  ESP_ERROR_CHECK(ret);

  // Configure SPI device for the Cmod A7
  spi_cmod.address_bits = 0;
  spi_cmod.command_bits = 1 * 8;
  spi_cmod.dummy_bits = 0;
  spi_cmod.mode = 0;
  //spi_cmod.input_delay_ns = 50;
  spi_cmod.duty_cycle_pos = 0;
  spi_cmod.cs_ena_posttrans = 0;
  spi_cmod.cs_ena_pretrans = 0;
  spi_cmod.clock_speed_hz = SPI_SPEED_MHZ * 1000 * 1000;
  spi_cmod.spics_io_num = SPI_PIN_NUM_CS_CMOD;
  spi_cmod.flags = SPI_DEVICE_HALFDUPLEX;
  spi_cmod.queue_size = 1;
  spi_cmod.pre_cb = NULL;
  spi_cmod.post_cb = NULL;
  ret = spi_bus_add_device(HSPI_HOST, &spi_cmod, &spi_cmod_h);
  ESP_ERROR_CHECK(ret);
}

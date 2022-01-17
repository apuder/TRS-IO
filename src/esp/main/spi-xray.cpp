
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

static spi_device_interface_config_t spi_mcp4351;
spi_device_handle_t spi_mcp4351_h;


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

static void writeDigiPot(uint8_t pot, uint8_t step)
{
  spi_transaction_t trans;
  const uint8_t cmd = 0; // Write command
  uint8_t p = 0;

  // Determine MCP4351 memory address (table 7-2)
  switch(pot) {
  case 0:
    p = 0;
    break;
  case 1:
    p = 1;
    break;
  case 2:
    p = 6;
    break;
  default:
    assert(0);
  }

  uint8_t data = (p << 4) | (cmd << 2);

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA;
  trans.length = 2 * 8;   		// 2 bytes
  trans.tx_data[0] = data;
  trans.tx_data[1] = step;
  esp_err_t ret = spi_device_transmit(spi_mcp4351_h, &trans);
  ESP_ERROR_CHECK(ret);
}

static uint8_t readDigiPot(uint8_t pot)
{
  spi_transaction_t trans;
  const uint8_t cmd = 3; // Read command
  uint8_t p = 0;

  // Determine MCP4351 memory address (table 7-2)
  switch(pot) {
  case 0:
    p = 0;
    break;
  case 1:
    p = 1;
    break;
  case 2:
    p = 6;
    break;
  default:
    assert(0);
  }

  uint8_t data = (p << 4) | (cmd << 2);

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  trans.length = 2 * 8;   		// 2 bytes
  trans.rxlength = 0;
  trans.tx_data[0] = data;
  trans.tx_data[1] = 0;
  esp_err_t ret = spi_device_transmit(spi_mcp4351_h, &trans);
  ESP_ERROR_CHECK(ret);
  return trans.rx_data[1];
}

void spi_set_screen_color(uint8_t color)
{
  static const uint8_t wiper_settings[][3] = {
    {225, 225, 255},
    {51, 255, 51},
    {255, 177, 0}};

  if (color > 2) return;

  for (int i = 0; i < 3; i++) {
    writeDigiPot(i, wiper_settings[color][i]);
  }
}

#ifdef CONFIG_TRS_IO_RUN_PCB_TESTS
static void test_digital_pot()
{
  const uint8_t val1 = 255;
  const uint8_t val2 = 255;
  const uint8_t val3 = 255;

  writeDigiPot(0, val1);
  writeDigiPot(1, val2);
  writeDigiPot(2, val3);

  bool not_found = false;
  not_found |= readDigiPot(0) != val1;
  not_found |= readDigiPot(1) != val2;
  not_found |= readDigiPot(2) != val3;
  if (not_found) {
    ESP_LOGE("SPI", "MCP4351 not found");
  } else {
    ESP_LOGI("SPI", "MCP4351 found");
  }
  while (true) vTaskDelay(1);
}
#endif

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

   // Configure SPI device for MCP4351
  spi_mcp4351.address_bits = 0;
  spi_mcp4351.command_bits = 0;
  spi_mcp4351.dummy_bits = 0;
  spi_mcp4351.mode = 0;
  spi_mcp4351.duty_cycle_pos = 0;
  spi_mcp4351.cs_ena_posttrans = 0;
  spi_mcp4351.cs_ena_pretrans = 0;
  spi_mcp4351.clock_speed_hz = SPI_DIGI_POT_SPEED_MHZ * 1000 * 1000;
  spi_mcp4351.spics_io_num = SPI_PIN_NUM_CS_MCP4351;
  spi_mcp4351.flags = 0;
  spi_mcp4351.queue_size = 1;
  spi_mcp4351.pre_cb = NULL;
  spi_mcp4351.post_cb = NULL;
  ret = spi_bus_add_device(HSPI_HOST, &spi_mcp4351, &spi_mcp4351_h);
  ESP_ERROR_CHECK(ret);

#ifdef CONFIG_TRS_IO_RUN_PCB_TESTS
  test_digital_pot();
#endif

  spi_set_screen_color(1);
}

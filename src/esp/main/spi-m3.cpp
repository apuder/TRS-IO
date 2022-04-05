
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "config.h"
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task.h"
#include "esp_event_loop.h"
#include "spi.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define MCP23S_CHIP_ADDRESS 0x40
#define MCP23S_WRITE 0x00
#define MCP23S_READ  0x01

static spi_bus_config_t spi_bus;

static spi_device_interface_config_t spi_mcp23x;
static spi_device_handle_t spi_mcp23x_h;


void writePortExpander(uint8_t cmd, uint8_t data)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA;
  trans.length = 3 * 8;   		// 3 bytes
  trans.tx_data[0] = MCP23S_CHIP_ADDRESS | MCP23S_WRITE;
  trans.tx_data[1] = cmd;
  trans.tx_data[2] = data;

  esp_err_t ret = spi_device_transmit(spi_mcp23x_h, &trans);
  ESP_ERROR_CHECK(ret);
}

uint8_t readPortExpander(uint8_t reg)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  trans.length = 3 * 8;   		// 2 bytes
  trans.rxlength = 0;
  trans.tx_data[0] = MCP23S_CHIP_ADDRESS | MCP23S_READ;
  trans.tx_data[1] = reg;
  trans.tx_data[2] = 0;

  esp_err_t ret = spi_device_transmit(spi_mcp23x_h, &trans);
  ESP_ERROR_CHECK(ret);

  return trans.rx_data[2];
}

#if 1
void wire_test_port_expander()
{
#if 0
  while (1) {
    if(xQueueReceive(gpio_evt_queue, NULL, portMAX_DELAY)) {
      uint8_t data = readPortExpander2(MCP23S08_INTCAP);
      Serial.println(data);
    }
  }
#else

  // Writing
  uint8_t data = 0xaa;
  writePortExpander(MCP23S17_IODIRA, 0);

  while (1) {
    writePortExpander(MCP23S08_GPIO, data);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    data ^= 0xff;
    writePortExpander(MCP23S08_GPIO, data);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    data ^= 0xff;
  }
#endif
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

  // Configure SPI device for MCP23S08
  spi_mcp23x.address_bits = 0;
  spi_mcp23x.command_bits = 0;
  spi_mcp23x.dummy_bits = 0;
  spi_mcp23x.mode = 0;
  spi_mcp23x.duty_cycle_pos = 0;
  spi_mcp23x.cs_ena_posttrans = 0;
  spi_mcp23x.cs_ena_pretrans = 0;
  spi_mcp23x.clock_speed_hz = SPI_SPEED_MHZ * 1000 * 1000;
  spi_mcp23x.spics_io_num = SPI_PIN_NUM_CS_MCP23S08;
  spi_mcp23x.flags = 0;
  spi_mcp23x.queue_size = 1;
  spi_mcp23x.pre_cb = NULL;
  spi_mcp23x.post_cb = NULL;
  ret = spi_bus_add_device(HSPI_HOST, &spi_mcp23x, &spi_mcp23x_h);
  ESP_ERROR_CHECK(ret);

  //wire_test_port_expander();

  /*
   * MCP23S08 configuration. This port expander is connected to the 8-bit data bus of the TRS-80. Default
   * configuration is: input, disable pull-ups, disable interrupts.
   */
  writePortExpander(MCP23S08_IODIR, 0xff);
  writePortExpander(MCP23S08_GPPU, 0);
  writePortExpander(MCP23S08_GPINTEN, 0);
}

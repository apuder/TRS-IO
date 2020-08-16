
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "config.h"
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

static spi_device_interface_config_t spi_mcp23S17;
spi_device_handle_t spi_mcp23S17_h;

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
  xQueueSendFromISR(gpio_evt_queue, NULL, NULL);
}

void writePortExpander(spi_device_handle_t dev, uint8_t cmd, uint8_t data)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA;
  trans.length = 3 * 8;   		// 3 bytes
  trans.tx_data[0] = MCP23S_CHIP_ADDRESS | MCP23S_WRITE;
  trans.tx_data[1] = cmd;
  trans.tx_data[2] = data;

  esp_err_t ret = spi_device_transmit(dev, &trans);
  ESP_ERROR_CHECK(ret);
}

uint8_t readPortExpander(spi_device_handle_t dev, uint8_t reg)
{
  spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  trans.length = 3 * 8;   		// 2 bytes
  trans.rxlength = 0;
  trans.tx_data[0] = MCP23S_CHIP_ADDRESS | MCP23S_READ;
  trans.tx_data[1] = reg;
  trans.tx_data[2] = 0;

  esp_err_t ret = spi_device_transmit(dev, &trans);
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
  uint8_t data = 0;
  writePortExpander(MCP23S17, MCP23S17_IODIRA, 0);
  writePortExpander(MCP23S17, MCP23S17_IODIRB, 0);

  while (1) {
    writePortExpander(MCP23S17, MCP23S17_GPIOA, data);
    writePortExpander(MCP23S17, MCP23S17_GPIOB, data);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    data ^= 0xff;
    writePortExpander(MCP23S17, MCP23S17_GPIOA, data);
    writePortExpander(MCP23S17, MCP23S17_GPIOB, data);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    data ^= 0xff;
  }
#endif
}

#endif


void init_spi()
{
  // Configure SPI bus
  spi_bus.flags = SPICOMMON_BUSFLAG_MASTER;
  spi_bus.sclk_io_num = SPI_PIN_NUM_CLK;
  spi_bus.mosi_io_num = SPI_PIN_NUM_MOSI;
  spi_bus.miso_io_num = SPI_PIN_NUM_MISO;
  spi_bus.quadwp_io_num = -1;
  spi_bus.quadhd_io_num = -1;
  spi_bus.max_transfer_sz = 32;
  esp_err_t ret = spi_bus_initialize(HSPI_HOST, &spi_bus, 0);
  ESP_ERROR_CHECK(ret);

  // Configure SPI device for MCP23S17
  spi_mcp23S17.address_bits = 0;
  spi_mcp23S17.command_bits = 0;
  spi_mcp23S17.dummy_bits = 0;
  spi_mcp23S17.mode = 0;
  spi_mcp23S17.duty_cycle_pos = 0;
  spi_mcp23S17.cs_ena_posttrans = 0;
  spi_mcp23S17.cs_ena_pretrans = 0;
  spi_mcp23S17.clock_speed_hz = SPI_SPEED_MHZ * 1000 * 1000;
  spi_mcp23S17.spics_io_num = SPI_PIN_NUM_CS_MCP23S17;
  spi_mcp23S17.flags = 0;
  spi_mcp23S17.queue_size = 1;
  spi_mcp23S17.pre_cb = NULL;
  spi_mcp23S17.post_cb = NULL;
  ret = spi_bus_add_device(HSPI_HOST, &spi_mcp23S17, &spi_mcp23S17_h);
  ESP_ERROR_CHECK(ret);

  //wire_test_port_expander();

  /*
   * MCP23S17 configuration
   */
  // Ports A and B are connected to A0-A15. Configure as input. Disable pull-ups.
  // Disable interrupts
  writePortExpander(MCP23S17, MCP23S17_IODIRA, 0xff);
  writePortExpander(MCP23S17, MCP23S17_IODIRB, 0xff);
  writePortExpander(MCP23S17, MCP23S17_GPPUA, 0);
  writePortExpander(MCP23S17, MCP23S17_GPPUB, 0);
  writePortExpander(MCP23S17, MCP23S17_GPINTENA, 0);
  writePortExpander(MCP23S17, MCP23S17_GPINTENB, 0);
}


#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task.h"
#include "esp_event_loop.h"
#include "spi.h"

static spi_device_interface_config_t spi_cmod;
spi_device_handle_t spi_cmod_h;

static SemaphoreHandle_t mutex = NULL;

static const uint8_t screen_rgb_colors[][3] = {
  {225, 225, 255}, // White
  {51, 255, 51},   // Green
  {255, 177, 0}};  // Amber


uint8_t spi_get_cookie()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_GET_COOKIE;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

uint8_t spi_get_fpga_version()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_GET_FPGA_VERSION;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

uint8_t spi_get_mode()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_GET_MODE;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_bram_poke(uint16_t addr, uint8_t data)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_BRAM_POKE;
  trans.address_bits = 3 * 8;
  const uint32_t b0 = addr & 0xff;
  const uint32_t b1 = addr >> 8;
  const uint32_t b2 = data;
  trans.base.addr = b2 | (b1 << 8) | (b0 << 16);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

uint8_t spi_bram_peek(uint16_t addr)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_BRAM_PEEK;
  trans.address_bits = 2 * 8;
  trans.dummy_bits = 2;
  const uint32_t b0 = addr & 0xff;
  const uint32_t b1 = addr >> 8;
  trans.base.addr = b1 | (b0 << 8);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_xram_poke_code(uint8_t addr, uint8_t data)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_XRAM_POKE_CODE;
  trans.address_bits = 2 * 8;
  const uint32_t b0 = addr;
  const uint32_t b1 = data;
  trans.base.addr = b1 | (b0 << 8);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_xram_poke_data(uint8_t addr, uint8_t data)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_XRAM_POKE_DATA;
  trans.address_bits = 2 * 8;
  const uint32_t b0 = addr;
  const uint32_t b1 = data;
  trans.base.addr = b1 | (b0 << 8);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

uint8_t spi_xram_peek_data(uint8_t addr)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_XRAM_PEEK_DATA;
  trans.address_bits = 1 * 8;
  trans.dummy_bits = 2;
  trans.base.addr = addr;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

uint8_t spi_dbus_read()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_DBUS_READ;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
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

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

uint8_t spi_abus_read()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_ABUS_READ;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_trs_io_done()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.cmd = FPGA_CMD_TRS_IO_DONE;
  trans.base.length = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_set_breakpoint(uint8_t n, uint16_t addr)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_BREAKPOINT;
  trans.address_bits = 3 * 8;
  const uint32_t b0 = n;
  const uint32_t b1 = addr & 0xff;
  const uint32_t b2 = addr >> 8;
  trans.base.addr = b2 | (b1 << 8) | (b0 << 16);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_clear_breakpoint(uint8_t n)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_CLEAR_BREAKPOINT;
  trans.address_bits = 1 * 8;
  trans.base.addr = n;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_xray_resume()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.cmd = FPGA_CMD_XRAY_RESUME;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_set_full_addr(bool flag)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_FULL_ADDR;
  trans.address_bits = 1 * 8;
  trans.base.addr = flag ? 0xff : 0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

// The FPGA generates a HDMI signal and we have to tell it the RGB value
void spi_set_screen_color(uint8_t color)
{
  spi_transaction_ext_t trans;
  if (color > 2) return;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_SCREEN_COLOR;
  trans.address_bits = 3 * 8;
  const uint32_t b0 = screen_rgb_colors[color][0];
  const uint32_t b1 = screen_rgb_colors[color][1];
  const uint32_t b2 = screen_rgb_colors[color][2];
  trans.base.addr = b2 | (b1 << 8) | (b0 << 16);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_set_printer_en(bool enable)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_PRINTER_EN;
  trans.address_bits = 1 * 8;
  trans.base.addr = enable ? 1 : 0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_send_keyb(uint8_t idx, uint8_t mask)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SEND_KEYB;
  trans.address_bits = 2 * 8;
  const uint32_t b0 = idx;
  const uint32_t b1 = mask;
  trans.base.addr = b1 | (b0 << 8);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_ptrs_rst()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_PTRS_RST;
  trans.address_bits = 0 * 8;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_z80_pause()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_Z80_PAUSE;
  trans.address_bits = 0 * 8;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_z80_resume()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_Z80_RESUME;
  trans.address_bits = 0 * 8;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_z80_dsp_set_addr(uint16_t addr)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_Z80_DSP_SET_ADDR;
  trans.address_bits = 2 * 8;
  const uint32_t b0 = addr & 0xff;
  const uint32_t b1 = addr >> 8;
  trans.base.addr = b1 | (b0 << 8);
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_z80_dsp_poke(uint8_t v)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_Z80_DSP_POKE;
  trans.address_bits = 1 * 8;
  const uint32_t b0 = v;
  trans.base.addr = b0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

uint8_t spi_z80_dsp_peek()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_Z80_DSP_PEEK;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_set_led(bool r, bool g, bool b)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_LED;
  trans.address_bits = 1 * 8;
  uint32_t b0 = 0;
  if (r) b0 |= (1 << 0);
  if (g) b0 |= (1 << 1);
  if (b) b0 |= (1 << 2);
  trans.base.addr = b0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

uint8_t spi_get_config()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_GET_CONFIG;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_set_spi_ctrl_reg(uint8_t reg)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_SPI_CTRL_REG;
  trans.address_bits = 1 * 8;
  const uint32_t b0 = reg;
  trans.base.addr = b0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_set_spi_data(uint8_t data)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_SPI_DATA;
  trans.address_bits = 1 * 8;
  const uint32_t b0 = data;
  trans.base.addr = b0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

uint8_t spi_get_spi_data()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_DUMMY;
  trans.base.cmd = FPGA_CMD_GET_SPI_DATA;
  trans.address_bits = 0 * 8;
  trans.dummy_bits = 2;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 1 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);

  return trans.base.rx_data[0];
}

void spi_set_esp_status(uint8_t status)
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_ESP_STATUS;
  trans.address_bits = 1 * 8;
  const uint32_t b0 = status;
  trans.base.addr = b0;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}

void spi_set_cass_in()
{
  spi_transaction_ext_t trans;

  memset(&trans, 0, sizeof(spi_transaction_ext_t));
  trans.base.flags = SPI_TRANS_VARIABLE_ADDR;
  trans.base.cmd = FPGA_CMD_SET_CASS_IN;
  trans.address_bits = 0 * 8;
  trans.base.length = 0 * 8;
  trans.base.rxlength = 0 * 8;

  xSemaphoreTake(mutex, portMAX_DELAY);
  esp_err_t ret = spi_device_transmit(spi_cmod_h, &trans.base);
  xSemaphoreGive(mutex);
  ESP_ERROR_CHECK(ret);
}


void init_spi()
{
  mutex = xSemaphoreCreateMutex();

  spi_bus_config_t spi_bus = {
        .mosi_io_num = SPI_PIN_NUM_MOSI,
        .miso_io_num = SPI_PIN_NUM_MISO,
        .sclk_io_num = SPI_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
  };
  esp_err_t ret = spi_bus_initialize(HSPI_HOST, &spi_bus, 1);
  ESP_ERROR_CHECK(ret);

  // Configure SPI device for the Cmod A7/Nano 9K
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

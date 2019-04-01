
#include "serial-io.h"
#include "driver/uart.h"

#define ECHO_TEST_TXD  (GPIO_NUM_1)
#define ECHO_TEST_RXD  (GPIO_NUM_3)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE (1024)

int16_t read_byte() {
  int len;
  uint8_t data;

  do {
    uart_get_buffered_data_len(UART_NUM_0, (size_t*)&len);
  } while (len < 1);
 
  uart_read_bytes(UART_NUM_0, &data, 1, 0);
  return data;
}

void read_blob(void* buf, int32_t btr) {
  int len;
  
  do {
    uart_get_buffered_data_len(UART_NUM_0, (size_t*)&len);
  } while (len < btr);
 
  uart_read_bytes(UART_NUM_0, buf, btr, 0);
}

void write_byte(uint8_t b) {
  uart_write_bytes(UART_NUM_0, (const char*) &b, 1);
}

void write_blob(void* buf, int32_t btw) {
  uart_write_bytes(UART_NUM_0, buf, btw);
}

void init_serial_io() {      
  uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_param_config(UART_NUM_0, &uart_config);
  uart_set_pin(UART_NUM_0, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
  uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
}


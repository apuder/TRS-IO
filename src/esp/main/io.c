
#include "io.h"
#include "esp_event_loop.h"
#include "tcpip.h"
#include "retrostore.h"
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
static const char *TAG="io";

// GPIO pins 12-19
#define GPIO_DATA_BUS_MASK 0b11111111000000000000

#define GPIO_OUTPUT_DISABLE(mask) GPIO.enable_w1tc = (mask)

#define GPIO_OUTPUT_ENABLE(mask) GPIO.enable_w1ts = (mask)

/**
The first byte of every request is the protocol identifier.
The values are assigned defined ranges as follows

0-19 	Reserved
20-99	Core Protocols
100-199	Custom Protocols
200-255	Reserved

*/

#define PROTOCOL_PREFIX_TCPIP	20
#define PROTOCOL_PREFIX_RS		100

void io_task(void* p)
{
  
  while (true) {
	    
	    uint8_t* buf;
	    int size;
	    
	    uint8_t protocol = read_byte();
	    
	    switch( protocol ) {
		    case PROTOCOL_PREFIX_TCPIP:
				while (tcp_z80_out(read_byte()) != IP_STATE_SEND_TO_Z80);
				tcp_get_send_buffer(&buf, &size);
			    break;
		    default:
			    if ( rs_z80_out(protocol) != RS_STATE_SEND )
			    	while (rs_z80_out(read_byte()) != RS_STATE_SEND);
				rs_get_send_buffer(&buf, &size);
			    break;
	    }
	  
	    write_bytes(buf, size);

  }
}


void io_task_2(void* p)
{
  
  while (true) {
	    
	    uint8_t* buf;
	    int size;
	    
	    uint8_t protocol = read_byte();
	    
	    switch( protocol ) {
		    case PROTOCOL_PREFIX_TCPIP:
				while (tcp_z80_out(read_byte()) != IP_STATE_SEND_TO_Z80);
				tcp_get_send_buffer(&buf, &size);
			    break;
		    case PROTOCOL_PREFIX_RS:
				while (rs_z80_out(read_byte()) != RS_STATE_SEND);
				rs_get_send_buffer(&buf, &size);
			    break;
		    default:
		    	ESP_LOGE(TAG, "Unknown protocol %d", protocol);
				continue;
				break;
	    }
	  
	    write_bytes(buf, size);

  }
}

void init_io()
{
  gpio_config_t gpioConfig;

  // GPIO pins 12-19 (8 pins) are used for data bus
  gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 |
    GPIO_SEL_15 | GPIO_SEL_16 |
    GPIO_SEL_17 | GPIO_SEL_18 | GPIO_SEL_19;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Configure RD_N
  gpioConfig.pin_bit_mask = GPIO_SEL_36;
  gpio_config(&gpioConfig);
  
  // Configure ESP_SEL_N
  gpioConfig.pin_bit_mask = GPIO_SEL_23;
  gpio_config(&gpioConfig);
  
  // Configure IOBUSINT_N & ESP_WAIT_N
  gpioConfig.pin_bit_mask = GPIO_SEL_25 | GPIO_SEL_27;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Set IOBUSINT_N to 0
  gpio_set_level(GPIO_NUM_25, 0);
  
  // Set ESP_WAIT_N to 0
  gpio_set_level(GPIO_NUM_27, 0);
  
  // Configure push button
  gpioConfig.pin_bit_mask = GPIO_SEL_22;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&gpioConfig);

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, 1, NULL, 1);
}

uint8_t read_byte()
{
  uint8_t data;
  bool done = false;

  while (!done) {
    while (GPIO.in & (1 << GPIO_NUM_23)) ;

    if (GPIO.in1.data & (1 << (GPIO_NUM_36 - 32))) {
      // Read data
      data = GPIO.in >> 12;
      done = true;
    } else {
      // Ignore write request
    }
    
    // Release ESP_WAIT_N
    GPIO.out_w1ts = (1 << GPIO_NUM_27);
    
    // Wait for ESP_SEL_N to be de-asserted
    while (!(GPIO.in & (1 << GPIO_NUM_23))) ;
    
    // Set SEL_WAIT_N to 0 for next IO command
    GPIO.out_w1tc = (1 << GPIO_NUM_27);
  }
  return data;
}


void write_bytes(uint8_t* data, uint16_t len)
{
  // Assert IOBUSINT to signal TRS80 that we are ready to send
  REG_WRITE(GPIO_OUT_W1TS_REG, 1 << GPIO_NUM_25);
  
  if (len == 0) {
    for (volatile int i = 0; i < 1000; i++) ;
    // De-assert IOBUSINT
    REG_WRITE(GPIO_OUT_W1TC_REG, 1 << GPIO_NUM_25);
    return;
  }
  
  for (int i = 0; i < len; i++) {
    bool ignore = false;
    
    while (GPIO.in & (1 << GPIO_NUM_23)) ;
    
    // De-assert IOBUSINT
    REG_WRITE(GPIO_OUT_W1TC_REG, 1 << GPIO_NUM_25);

    if (GPIO.in1.data & (1 << (GPIO_NUM_36 - 32))) {
      // Ignore read request
      ignore = true;
    } else {
      GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
      // Write to bus
      uint32_t d = *data << 12;
      REG_WRITE(GPIO_OUT_W1TS_REG, d);
      d = d ^ GPIO_DATA_BUS_MASK;
      REG_WRITE(GPIO_OUT_W1TC_REG, d);
      data++;
    }
    
    // Release ESP_WAIT_N
    GPIO.out_w1ts = (1 << GPIO_NUM_27);
    
    // Wait for ESP_SEL_N to be de-asserted
    while (!(GPIO.in & (1 << GPIO_NUM_23))) ;
    
    // Set SEL_WAIT_N to 0 for next IO command
    GPIO.out_w1tc = (1 << GPIO_NUM_27);
    
    if (ignore) {
      i--;
      continue;
    }
    
    GPIO_OUTPUT_DISABLE(GPIO_DATA_BUS_MASK);
  }
}

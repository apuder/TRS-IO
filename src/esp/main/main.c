
#include "driver/gpio.h"


void app_main(void)
{
    gpio_config_t gpioConfig;
    
    // GPIO pins 12-19 (8 pins) are used for data bus
    gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 | GPIO_SEL_15 | GPIO_SEL_16 |
                              GPIO_SEL_17 | GPIO_SEL_18 | GPIO_SEL_19;
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&gpioConfig);

    // Configure RD_N & ESP_SEL_N
    gpioConfig.pin_bit_mask = GPIO_SEL_22 | GPIO_SEL_23;
    gpio_config(&gpioConfig);
    
    // Configure ESP_WAIT_N
    gpioConfig.pin_bit_mask = GPIO_SEL_27;
    gpioConfig.mode = GPIO_MODE_OUTPUT;
    gpio_config(&gpioConfig);
    
    // Set ESP_WAIT_N to 0
    gpio_set_level(GPIO_NUM_27, 0);
    
    unsigned char data = 0;
    int switched_to_out = 0;
    
    while (true) {
        while (gpio_get_level(GPIO_NUM_23) == 1) ;

        if (gpio_get_level(GPIO_NUM_22) == 1) {
            // Read data
            data = GPIO.in >> 12;
        } else {
            gpio_set_direction(GPIO_NUM_12, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_13, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
            gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
            switched_to_out = 1;
            // Write to bus
            unsigned int d = data << 12;
            REG_WRITE(GPIO_OUT_W1TS_REG, d);
            d = d ^ 0b11111111000000000000;
            REG_WRITE(GPIO_OUT_W1TC_REG, d);
            data++;
        }

        // Release ESP_WAIT_N
        gpio_set_level(GPIO_NUM_27, 1);

        // Wait for ESP_SEL_N to be de-asserted
        while (gpio_get_level(GPIO_NUM_23) == 1) ;

        // Set SEL_WAIT_N to 0 for next IO command
        gpio_set_level(GPIO_NUM_27, 0);

        if (switched_to_out) {
            gpio_set_direction(GPIO_NUM_12, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_15, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_16, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_17, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_18, GPIO_MODE_INPUT);
            gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);
            switched_to_out = 0;
        }
    }
}


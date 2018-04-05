
#include "driver/gpio.h"

static uint8_t data = 0;
static uint8_t switched_to_out = 0;

#define gpio_output_disable(gpio_num) \
        GPIO.enable_w1tc = (0x1 << gpio_num); \
        REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + (gpio_num * 4), SIG_GPIO_OUT_IDX);

#define gpio_output_enable(gpio_num) \
        GPIO.enable_w1ts = (0x1 << gpio_num); \
        gpio_matrix_out(gpio_num, SIG_GPIO_OUT_IDX, false, false);

static void IRAM_ATTR isr_esp_sel_handler(void* arg)
{
    while (gpio_get_level(GPIO_NUM_23) == 1) ;
    if (gpio_get_level(GPIO_NUM_36) == 1) {
        // Read data
        data = GPIO.in >> 12;
    } else {
        gpio_output_enable(GPIO_NUM_12);
        gpio_output_enable(GPIO_NUM_13);
        gpio_output_enable(GPIO_NUM_14);
        gpio_output_enable(GPIO_NUM_15);
        gpio_output_enable(GPIO_NUM_16);
        gpio_output_enable(GPIO_NUM_17);
        gpio_output_enable(GPIO_NUM_18);
        gpio_output_enable(GPIO_NUM_19);
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
        gpio_output_disable(GPIO_NUM_12);
        gpio_output_disable(GPIO_NUM_13);
        gpio_output_disable(GPIO_NUM_14);
        gpio_output_disable(GPIO_NUM_15);
        gpio_output_disable(GPIO_NUM_16);
        gpio_output_disable(GPIO_NUM_17);
        gpio_output_disable(GPIO_NUM_18);
        gpio_output_disable(GPIO_NUM_19);
        switched_to_out = 0;
    }
}


static void gpio_setup()
{
    gpio_config_t gpioConfig;

    gpio_install_isr_service(0);
    
    // GPIO pins 12-19 (8 pins) are used for data bus
    gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 | GPIO_SEL_15 | GPIO_SEL_16 |
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
    //gpioConfig.intr_type = GPIO_PIN_INTR_NEGEDGE;
    gpio_config(&gpioConfig);
    gpio_isr_handler_add(GPIO_NUM_23, isr_esp_sel_handler, (void*) 0);
    
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
}

void app_main(void)
{
    gpio_setup();
    while (true) {
        //gpio_set_level(GPIO_NUM_25, gpio_get_level(GPIO_NUM_22) ? 0 : 1);
        isr_esp_sel_handler(0);
    }
}


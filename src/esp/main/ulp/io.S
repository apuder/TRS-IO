
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"


.global entry

.set esp_sel_n, 12           // GPIO 2 (orig GPIO 23), RTC GPIO 12
.set esp_wait_release_n, 17  // GPIO 27, RTC GPIO 17

.text

entry:
  // Wait for ESP_SEL_N to be asserted
  READ_RTC_REG(RTC_GPIO_IN_REG, 14 + esp_sel_n, 1)
  jumpr entry, 1, ge

  // GAL asserted ESP_SEL_N. Raise interrupt with main CPU
  wake

  // Wait for ESL_SEL_N to be de-asserted
loop:
  READ_RTC_REG(RTC_GPIO_IN_REG, 14 + esp_sel_n, 1)
  jumpr loop, 1, lt

  // Set ESP_WAIT_RELEASE_N to 0 for next IO command
  WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, 14 + esp_wait_release_n, 1, 1)
  jump entry

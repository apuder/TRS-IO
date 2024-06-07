
#include "bitstream-src.h"
#include "flash.h"
#include "xflash.h"
#include "settings.h"
#include "ptrs.h"
#include "jtag.h"
#include "spi.h"
#include "led.h"
#include "esp_log.h"


#define TAG "FPGA"

static const char* configurations[] = {
  "TRS-IO (Model 1)",
  "PocketTRS (Model 1, internal TRS-IO)",
  "Reserved",
  "PocketTRS (Model III, internal TRS-IO)",
  "PocketTRS (Model 4, internal TRS-IO)",
  "PocketTRS (Model 4P, internal TRS-IO)",
  "Custom 1",
  "Custom 2",
  "TRS-IO (Model III)",
  "PocketTRS (Model 1, external TRS-IO)",
  "Reserved",
  "PocketTRS (Model III, external TRS-IO)",
  "PocketTRS (Model 4, external TRS-IO)",
  "PocketTRS (Model 4P, external TRS-IO)",
  "Custom 1",
  "Custom 2"
};

#define RESERVED  -1
#define TRS_IO_M1 0
#define TRS_IO_M3 1
#define PTRS_M1   2
#define PTRS_M3   3
#define PTRS_M4   4
#define PTRS_M4P  5
#define CUSTOM_1  6
#define CUSTOM_2  7

static const char* bitstreams[] = {
  "trs_io_m1.bin",
  "trs_io_m3.bin",
  "ptrs_m1.bin",
  "ptrs_m3.bin",
  "ptrs_m4.bin",
  "ptrs_m4p.bin",
  "custom_1.bin",
  "custom_2.bin"
};

static const int conf_to_bs[] = {
  TRS_IO_M1,
  PTRS_M1,
  RESERVED,
  PTRS_M3,
  PTRS_M4,
  PTRS_M4P,
  CUSTOM_1,
  CUSTOM_2,
  TRS_IO_M3,
  PTRS_M1,
  RESERVED,
  PTRS_M3,
  PTRS_M4,
  PTRS_M4P,
  CUSTOM_1,
  CUSTOM_2
};

static const bool is_custom[] = {
  false,
  false,
  false,
  false,
  false,
  false,
  true,
  true,
  false,
  false,
  false,
  false,
  false,
  false,
  true,
  true
};

static const int ptrs_model[] = {
  -1,
  SETTINGS_ROM_M1,
  -1,
  SETTINGS_ROM_M3,
  SETTINGS_ROM_M4,
  SETTINGS_ROM_M4P,
  -1,
  -1,
  -1,
  SETTINGS_ROM_M1,
  -1,
  SETTINGS_ROM_M3,
  SETTINGS_ROM_M4,
  SETTINGS_ROM_M4P,
  -1,
  -1
};


static void check_firmware()
{
  JTAGAdapterTrsIO* jtag;
  unsigned char buf[256];
  unsigned long addr = 0;
  int cnt;
  
  int conf = spi_get_config() & 0x0f;
  int mode = spi_get_mode();
  ESP_LOGI(TAG, "Current firmware: %s", configurations[mode]);
  ESP_LOGI(TAG, "Target firmware: %s", configurations[conf]);
  if (conf == RESERVED) {
    ESP_LOGE(TAG, "Selected RESERVED. Not changing FPGA firmware");
    return;
  }
  int curr_bs = conf_to_bs[mode];
  int target_bs = conf_to_bs[conf];
  if (curr_bs == target_bs) {
    ESP_LOGI(TAG, "FPGA already running correct firmware");
    return;
  }

  const char* new_bs = bitstreams[target_bs];
  ESP_LOGI(TAG, "Uploading FPGA firmware: %s", new_bs);
  // Set LED to solid blue
  set_led(false, false, true, false, false);
  if (is_custom[target_bs]) {
    ESP_LOGE(TAG, "Cannot handle custom firmware yet!");
    return;
  }

  BitstreamSource* bs = new BitstreamSourceFlash(new_bs);
  if (!bs->open()) {
    ESP_LOGE(TAG, "Could not open bistream file");
    goto err;
  }

  do {
    // if the address is the start of a sector then erase the sector
    if((addr & (FLASH_SECTOR_SIZE - 1)) == 0) {
      ESP_LOGI(TAG, "Address: %06lX", addr);
      flashSectorErase(addr);
      // Make watchdog happy
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    if (!bs->read(buf, sizeof(buf), &cnt)) {
      break;
    }

    flashWrite(addr, buf, cnt);

    addr += cnt;

    // if <256 bytes read then this is end of file
    if(cnt < 256) {
      break;
    }

  } while(!(cnt < 256));

  set_led(false, false, false, false, false);
  bs->close();
  ESP_LOGI(TAG, "Wrote %ld bytes", addr);

  ESP_LOGI(TAG, "Reloading FPGA");

  jtag = new JTAGAdapterTrsIO();
  jtag->setup();
  jtag->reset();
  jtag->setIR(RELOAD);
  jtag->setIR(NOOP);
  delete jtag;

  while(spi_get_cookie() != FPGA_COOKIE) {
    ESP_LOGE(TAG, "Waiting for FPGA");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  ESP_LOGI(TAG, "FPGA ready");

err:
  set_led(false, false, false, false, false);
  delete bs;
}

static void fpga_task(void* args)
{
  check_firmware();
  int conf = spi_get_config() & 0x0f;
  int model = ptrs_model[conf];
  ESP_LOGI(TAG, "TRS-IO++ running in %s mode", (model == -1) ? "TRS-IO" : "PocketTRS");
  if (model != -1) {
    // TRS-IO++ running in PocketTRS mode
    init_ptrs(model);
  }

  vTaskDelete(NULL);
}

void init_fpga()
{
  xTaskCreatePinnedToCore(fpga_task, "fpga", 6000, NULL, 1, NULL, 0);
}
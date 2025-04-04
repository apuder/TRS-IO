
#include "bitstream-src.h"
#include "flash.h"
#include "xflash.h"
#include "event.h"
#include "settings.h"
#include "ptrs.h"
#include "jtag.h"
#include "spi.h"
#include "led.h"
#include "esp_log.h"


#define TAG "FPGA"

//#define VERIFY_FLASH


static const char* configurations[] = {
  "TRS-IO (Model 1)",
  "PocketTRS (Model 1, internal TRS-IO)",
  "Rescue",
  "PocketTRS (Model III, internal TRS-IO)",
  "PocketTRS (Model 4, internal TRS-IO)",
  "PocketTRS (Model 4P, internal TRS-IO)",
  "Custom 1",
  "Custom 2",
  "TRS-IO (Model III)",
  "PocketTRS (Model 1, external TRS-IO)",
  "Rescue",
  "PocketTRS (Model III, external TRS-IO)",
  "PocketTRS (Model 4, external TRS-IO)",
  "PocketTRS (Model 4P, external TRS-IO)",
  "Custom 1",
  "Custom 2"
};

#define TRS_IO_M1 0
#define TRS_IO_M3 1
#define PTRS_M1   2
#define PTRS_M3   3
#define PTRS_M4   4
#define PTRS_M4P  5
#define CUSTOM_1  6
#define CUSTOM_2  7
#define RESCUE    8

static const char* bitstreams[] = {
  "trs_io_m1.bin",
  "trs_io_m3.bin",
  "ptrs_m1.bin",
  "ptrs_m3.bin",
  "ptrs_m4.bin",
  "ptrs_m4p.bin",
  "custom_1.bin",
  "custom_2.bin",
  "rescue.bin"
};

static const int conf_to_bs[] = {
  TRS_IO_M1,
  PTRS_M1,
  RESCUE,
  PTRS_M3,
  PTRS_M4,
  PTRS_M4P,
  CUSTOM_1,
  CUSTOM_2,
  TRS_IO_M3,
  PTRS_M1,
  RESCUE,
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
  -2, // Rescue
  SETTINGS_ROM_M3,
  SETTINGS_ROM_M4,
  SETTINGS_ROM_M4P,
  -1,
  -1,
  -1,
  SETTINGS_ROM_M1,
  -2, // Rescue
  SETTINGS_ROM_M3,
  SETTINGS_ROM_M4,
  SETTINGS_ROM_M4P,
  -1,
  -1
};

static bool is_custom_firmware_selected()
{
  int conf = spi_get_config() & 0x0f;
  int target_bs = conf_to_bs[conf];
  return is_custom[target_bs];
}

static void wait_for_fpga()
{
  while(spi_get_cookie() != FPGA_COOKIE) {
    ESP_LOGE(TAG, "Waiting for FPGA");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  ESP_LOGI(TAG, "FPGA ready");
}

static bool fpga_upload_rescue_firmware(bool wait)
{
  ESP_LOGI(TAG, "Upload rescue firmware");

  BitstreamSource* bs = new BitstreamSourceFlash(bitstreams[RESCUE]);
  JTAGAdapterTrsIO* jtag = new JTAGAdapterTrsIO();
  bool success = jtag->doProgramToSRAM(bs);
  delete bs;
  delete jtag;

  if (!success) {
    ESP_LOGE(TAG, "Upload failed");
    return false;
  }

  if (wait) {
    wait_for_fpga();
  }

  return true;
}

static void check_firmware()
{
  BitstreamSource* bs;
  JTAGAdapterTrsIO* jtag;
  unsigned char buf[256];
#ifdef VERIFY_FLASH
  unsigned char buf2[256];
#endif
  unsigned long addr = 0;
  int cnt;
  uint32_t id;

  id = flashReadMfdDevId();
  ESP_LOGI(TAG, "Flash ID: 0x%x", id);
  if (id == 0) {
    ESP_LOGE(TAG, "Bad flash ID");
    if (!fpga_upload_rescue_firmware(true)) {
      return;
    }
  }

  int conf = spi_get_config() & 0x0f;
  int mode = spi_get_mode();
  ESP_LOGI(TAG, "Current firmware: %s", configurations[mode]);
  ESP_LOGI(TAG, "Target firmware: %s", configurations[conf]);
  int curr_bs = conf_to_bs[mode];
  int target_bs = conf_to_bs[conf];
  if (settings_get_update_flag()) {
    // Firmware was updated since last boot. Force an update of the FPGA firmware.
    ESP_LOGI(TAG, "Forcing FPGA firmware update after OTA");
    curr_bs = -1;
    settings_set_update_flag(false);
    settings_commit();
  }
  if (curr_bs == target_bs) {
    ESP_LOGI(TAG, "FPGA already running correct firmware");
    return;
  }

  const char* new_bs = bitstreams[target_bs];
  ESP_LOGI(TAG, "Uploading FPGA firmware: %s", new_bs);

  if (is_custom[target_bs]) {
    ESP_LOGI(TAG, "Waiting for mounted filesystem");
    evt_wait(EVT_FS_MOUNTED);
    bs = new BitstreamSourceFile(new_bs);
  } else {
    bs = new BitstreamSourceFlash(new_bs);
  }

  if (!bs->open()) {
    ESP_LOGE(TAG, "Could not open bistream file");
    goto err;
  }

  // Set LED to solid blue
  set_led(false, false, true, false, false);

  do {
    // if the address is the start of a sector then erase the sector
    if((addr & (FLASH_SECTOR_SIZE - 1)) == 0) {
      ESP_LOGI(TAG, "Address: 0x%06lX", addr);
      flashSectorErase(addr);
      // Make watchdog happy
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    if (!bs->read(buf, sizeof(buf), &cnt)) {
      break;
    }

    flashWrite(addr, buf, cnt);

#ifdef VERIFY_FLASH
    flashRead(addr, buf2, cnt);
    if(memcmp(buf, buf2, cnt) != 0) {
      ESP_LOGI(TAG, "Verify error!");
    }
#endif

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

  wait_for_fpga();

err:
  set_led(false, false, false, false, false);
  delete bs;
}

static void fpga_task(void* args)
{
  check_firmware();
  int mode = spi_get_mode();
  int model = ptrs_model[mode];
  ESP_LOGI(TAG, "TRS-IO++ running in %s mode", (model == -1) ? "TRS-IO" : ((model == -2) ? "Rescue" : "PocketTRS"));
  if (model >= 0) {
    // TRS-IO++ running in PocketTRS mode
    init_ptrs(model);
  }

  if (args != NULL) {
    vTaskDelete(NULL);
  }
}

void init_fpga()
{
  uint8_t count = 0;

  while(spi_get_cookie() != FPGA_COOKIE) {
#ifdef CONFIG_TRS_IO_PP
    if (count++ == 5) {
      fpga_upload_rescue_firmware(false);
      count = 0;
      continue;
    }
#endif
    ESP_LOGE("SPI", "FPGA not found");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  ESP_LOGI("SPI", "Found FPGA");

#ifdef CONFIG_TRS_IO_PP
  if (is_custom_firmware_selected()) {
    // Custom firmware is loaded via TRS-FS. Need to wait until FS is mounted
    xTaskCreatePinnedToCore(fpga_task, "fpga", 6000, (void*) 1, 1, NULL, 0);
  } else {
    // Otherwise load correct firmware synchonously
    fpga_task(NULL);
  }
#endif

  uint8_t color = settings_get_screen_color();
  spi_set_screen_color(color);

  spi_set_printer_en(settings_get_printer_en());
  
  spi_set_esp_status(0);
}
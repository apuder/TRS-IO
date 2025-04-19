
#include "trs-fs.h"
#include "trs-lib.h"
#include "ota.h"
#include "led.h"
#include "settings.h"
#include "rst.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TAG "OTA"

#define OK 0
#define ERR_CORRUPT_TAR 1
#define ERR_BAD_ESP_FLASH 2
#define ERR_FIRMWARE_UPDATE_FAILED 3

#define CHECK_OTA(err) if ((err) != ESP_OK) return false;

#define BUFFER_SIZE 4096

static window_t wnd;
static FIL f_in;


// TAR header structure (basic fields used in this example)
union tar_header {
  struct _header {
    char filename[100];  // Filename (null-terminated string)
    char mode[8];        // File mode (not used in this code)
    char uid[8];         // Owner's numeric user ID (not used)
    char gid[8];         // Group's numeric user ID (not used)
    char size[12];       // File size in octal (as a string)
    char mtime[12];      // Modification time (not used)
    char chksum[8];      // Checksum (not used)
    char typeflag;       // Type of file (file, directory, etc.)
    char linkname[100];  // Link name (not used)
    char magic[6];       // UStar indicator

    // ... (other fields in the TAR header, not relevant here)
  } header;
  char buffer[512];
};

static union tar_header tar;
static uint16_t buffer_idx;

// Fetches the next byte from the tar file
static uint8_t get_next()
{
  UINT br;

  if (buffer_idx == 0) {
    trs_fs->f_read(&f_in, tar.buffer, 512, &br);
  }

  uint8_t b = tar.buffer[buffer_idx];
  buffer_idx = (buffer_idx + 1) % 512;
  return b;
}

static bool tar_copy_file(const char* filename, size_t file_size)
{
  wnd_print(&wnd, "Updating: %s ", filename);
  uint8_t x = wnd_get_x(&wnd);
  uint8_t y = wnd_get_y(&wnd);
  wnd_print(&wnd, "(0%%)");

  char* abs_path;
  asprintf(&abs_path, "/%s", filename);
  FILE* f = fopen(abs_path, "wb");
  free(abs_path);
  if (f == NULL) {
    return false;
  }
  uint8_t* buf = (uint8_t*) malloc(BUFFER_SIZE);
  int idx = 0;
  for(int i = 0; i < file_size; i++) {
    buf[idx++] = get_next();
    if (idx == BUFFER_SIZE) {
      fwrite(buf, 1, BUFFER_SIZE, f);
      idx = 0;
      wnd_goto(&wnd, x, y);
      wnd_print(&wnd, "(%d%%)", (i * 100) / file_size);
    }
  }
  if (idx != 0) {
    fwrite(buf, 1, idx, f);
  }
  fclose(f);
  free(buf);
  wnd_goto(&wnd, x, y);
  wnd_print(&wnd, "(100%%)\n");
  return true;
}

static bool tar_copy_firmware(size_t file_size)
{
  ESP_LOGI(TAG, "Performing OTA");
  wnd_print(&wnd, "Updating: ESP firmware ");
  uint8_t x = wnd_get_x(&wnd);
  uint8_t y = wnd_get_y(&wnd);
  wnd_print(&wnd, "(0%%)");

  esp_ota_handle_t update_handle = 0 ;
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition == NULL) {
    ESP_LOGE(TAG, "Found no update partition");
    return false;
  }
  ESP_LOGI(TAG, "Writing to partition %s subtype %d at offset 0x%x",
           update_partition->label, update_partition->subtype, update_partition->address);

  CHECK_OTA(esp_ota_begin(update_partition,
                          OTA_SIZE_UNKNOWN, &update_handle));

  uint8_t* buf = (uint8_t*) malloc(BUFFER_SIZE);
  int idx = 0;
  for(int i = 0; i < file_size; i++) {
    buf[idx++] = get_next();
    if (idx == BUFFER_SIZE) {
      CHECK_OTA(esp_ota_write(update_handle, (const void*) buf, BUFFER_SIZE));
      idx = 0;
      wnd_goto(&wnd, x, y);
      wnd_print(&wnd, "(%d%%)", (i * 100) / file_size);
    }
  }
  if (idx != 0) {
    CHECK_OTA(esp_ota_write(update_handle, (const void*) buf, idx));
  }
  free(buf);

  CHECK_OTA(esp_ota_end(update_handle));
  CHECK_OTA(esp_ota_set_boot_partition(update_partition));

  ESP_LOGI(TAG, "Wrote %d bytes", file_size);
  wnd_goto(&wnd, x, y);
  wnd_print(&wnd, "(100%%)\n");
  return true;
}


// Helper function to convert octal string to integer
static size_t octal_to_decimal(const char* octal)
{
  size_t result = 0;
  while (*octal >= '0' && *octal <= '7') {
    result = (result << 3) + (*octal - '0');
    octal++;
  }
  return result;
}

static int extract_tar()
{
  buffer_idx = 0;

  while (1) {
    // Read the next 512-byte header
    for (int i = 0; i < 512; i++) {
      ((uint8_t *)&tar.header)[i] = get_next();
    }

    // If the filename is empty, we've reached the end of the tar file
    if (tar.header.filename[0] == '\0') {
      break;
    }

    if (strncmp(tar.header.magic, "ustar", 5) != 0) {
      return ERR_CORRUPT_TAR;  // Exit the function if the magic value does not match
    }

    // Skip non-regular files (typeflag '0' or '\0' is for regular files)
    if (tar.header.typeflag != '0' && tar.header.typeflag != '\0') {
      // Get the file size (even for non-regular files) and skip its content
      size_t file_size = octal_to_decimal(tar.header.size);
      size_t padding = (512 - (file_size % 512)) % 512;

      // Skip file data and padding
      for (size_t i = 0; i < file_size + padding; i++) {
        get_next();
      }
      continue; // Move to the next file header
    }

    // Get the file size from the header (stored as an octal string)
    size_t file_size = octal_to_decimal(tar.header.size);

    // Open the file for extraction
    if (strncmp("html/", tar.header.filename, 5) == 0 ||
        strncmp("fpga/", tar.header.filename, 5) == 0) {
      if (!tar_copy_file(tar.header.filename, file_size)) {
        return ERR_BAD_ESP_FLASH;
      }
    } else if (strcmp("build/trs-io.bin", tar.header.filename) == 0) {
      if (!tar_copy_firmware(file_size)) {
        return ERR_FIRMWARE_UPDATE_FAILED;
      }
    } else {
      // Read and discard the file contents
      for (size_t i = 0; i < file_size; i++) {
        uint8_t byte = get_next();
      }
    }

    // Skip padding bytes (to align to 512-byte blocks)
    size_t padding = (512 - (file_size % 512)) % 512;
    for (size_t i = 0; i < padding; i++) {
      get_next();
    }
  }
  return OK;
}

static void end_update()
{
  set_led(false, false, false, false, false);
  wnd_print(&wnd, "\n\nPress any key to exit...");
  get_key();
}

void update_firmware()
{
  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  header("Update");
  screen_show(false);

  wnd_print(&wnd, "\nPress any key to continue, <ESC> to abort.");
  if (get_key() == KEY_BREAK) {
    return;
  }

  // Set LED to blue
  set_led(false, false, true, false, false);

  wnd_print(&wnd, "\n\n");

  if (trs_fs == NULL) {
    wnd_print(&wnd, "Neither SMB nor SD card are mounted. Cannot perform update.");
    end_update();
    return;
  }

  FRESULT res = trs_fs->f_open(&f_in, "update.tar", FA_READ);
  if (res != FR_OK) {
    wnd_print(&wnd, "Update file 'update.tar' not found. Aborting update.");
    end_update();
    return;
  }
  
  int err = extract_tar();
  wnd_print(&wnd, "\n");
  trs_fs->f_close(&f_in);
  switch(err) {
    case ERR_CORRUPT_TAR:
      wnd_print(&wnd, "File 'update.tar' is corrupt!");
      end_update();
      return;
    case ERR_BAD_ESP_FLASH:
      wnd_print(&wnd, "Could not write file to ESP's flash!");
      end_update();
      return;
    case ERR_FIRMWARE_UPDATE_FAILED:
      wnd_print(&wnd, "Updating of ESP firmware failed!");
      end_update();
      return;
    default:
      break;
  }

  set_led(false, false, false, false, false);
  settings_set_update_flag(true);
  settings_commit();
  wnd_print(&wnd, "Update complete. FPGA will be updated after reboot.\n");
  wnd_print(&wnd, "Press any key to reboot ESP...");
  get_key();
  wnd_cls(&wnd);
  wnd_print(&wnd, "\n\nRebooting. Do not turn off power while status LED is blue...");
  reboot_trs_io();
}
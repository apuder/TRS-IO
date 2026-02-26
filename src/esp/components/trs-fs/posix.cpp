
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <driver/sdspi_host.h>

namespace VFS {
  #include "esp_vfs_fat.h"
}

#include "sdmmc_cmd.h"

#include "trs-fs.h"
#include "posix.h"
#include "storage.h"
#include "io.h"

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
//#include "esp_log.h"

//#include "esp_flash.h"

#include "EXIO/TCA9554PWR.h"        

#define CONFIG_EXAMPLE_PIN_CLK  GPIO_NUM_2
#define CONFIG_EXAMPLE_PIN_CMD  GPIO_NUM_1
#define CONFIG_EXAMPLE_PIN_D0   GPIO_NUM_42
#define CONFIG_EXAMPLE_PIN_D1   ((gpio_num_t) -1)
#define CONFIG_EXAMPLE_PIN_D2   ((gpio_num_t) -1)
#define CONFIG_EXAMPLE_PIN_D3   ((gpio_num_t) -1)  // Using EXIO

#define SD_TAG "SD_CARD"

static esp_err_t SD_Card_D3_EN(void)
{
    Set_EXIO(TCA9554_EXIO4,true);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static esp_err_t SD_Card_D3_Dis(void)
{
    Set_EXIO(TCA9554_EXIO4,false);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

TRS_FS_POSIX::TRS_FS_POSIX() {
  err_msg = NULL;

  if (!has_sd_card_reader()) {
    err_msg = "SD card not supported";
    return;
  }

  esp_err_t ret;

  // Options for mounting the filesystem.
  // If format_if_mount_failed is set to true, SD card will be partitioned and formatted in case when mounting fails.  false true
  VFS::esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = true,           
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };

  //ESP_LOGI(SD_TAG, "Initializing SD card");
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();

  SD_Card_D3_EN();

  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;          // 1-wire  / 4-wire   slot_config.width = 4;

  slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
  slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
  slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
  slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
  slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
  slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  //ESP_LOGI(SD_TAG, "Mounting filesystem");
  ret = VFS::esp_vfs_fat_sdmmc_mount(mount, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      err_msg = "Failed to mount filesystem";
      //ESP_LOGE(SD_TAG, "Failed to mount filesystem.");
    } else {
      err_msg = "Failed to initialize the SD card";
      //ESP_LOGE(SD_TAG, "Failed to initialize the card (%s).", esp_err_to_name(ret));
    }
  } else {
    //ESP_LOGI(SD_TAG, "Filesystem mounted");
  }
}

TRS_FS_POSIX::~TRS_FS_POSIX()
{
  if (err_msg == NULL) {
    VFS::esp_vfs_fat_sdcard_unmount(mount, card);
  }
}

FS_TYPE TRS_FS_POSIX::type()
{
  return FS_POSIX;
}

bool TRS_FS_POSIX::has_sd_card_reader()
{
  return true;
}

void TRS_FS_POSIX::f_log(const char* msg) {
#if 0
  printf("TRS_FS_POSIX: %s\n", msg);
#endif
}
  
FRESULT TRS_FS_POSIX::f_open (
                            FIL* fp,           /* [OUT] Pointer to the file object structure */
                            const TCHAR* path, /* [IN] File name */
                            BYTE mode          /* [IN] Mode flags */
                            ) {
  char* abs_path;
  const char* m;
  
  switch(mode) {
  case FA_READ:
    m = "r";
    break;
  case FA_READ | FA_WRITE:
    m = "r+";
    break;
  case FA_CREATE_ALWAYS | FA_WRITE:
    m = "w";
    break;
  case FA_CREATE_ALWAYS | FA_WRITE | FA_READ:
    m = "w+";
    break;
  case FA_OPEN_APPEND | FA_WRITE:
    m = "a";
    break;
  case FA_OPEN_APPEND | FA_WRITE | FA_READ:
    m = "a+";
    break;
  case  FA_CREATE_NEW | FA_WRITE:
    m = "w";
    break;
  case FA_CREATE_NEW | FA_WRITE | FA_READ:
    m = "w+";
    break;
  default:
    assert(0);
  }

  asprintf(&abs_path, "%s/%s", mount, path);
  fp->f = fopen(abs_path, m);
  free(abs_path);
  return (fp->f == NULL) ? FR_NO_FILE : FR_OK;
}

FRESULT TRS_FS_POSIX::f_opendir (
                               DIR_* dp,           /* [OUT] Pointer to the directory object structure */
                               const TCHAR* path  /* [IN] Directory name */
                               ) {
  char* abs_path;

  if ((strcmp(path, ".") == 0) || (strcmp(path, "/") == 0)) {
    path = "";
  }
  asprintf(&abs_path, "%s/%s", mount, path);
  dp->dir = opendir(abs_path);
  free(abs_path);
  return (dp->dir != NULL) ? FR_OK : FR_DISK_ERR;
}

FRESULT TRS_FS_POSIX::f_write (
                             FIL* fp,          /* [IN] Pointer to the file object structure */
                             const void* buff, /* [IN] Pointer to the data to be written */
                             UINT btw,         /* [IN] Number of bytes to write */
                             UINT* bw          /* [OUT] Pointer to the variable to return number of bytes written */
                             ) {
  int _bw = fwrite(buff, 1, btw, (FILE*) fp->f);
  *bw = _bw;
  return (_bw >= 0) ? FR_OK : FR_DISK_ERR;
}

FRESULT TRS_FS_POSIX::f_read (
                            FIL* fp,     /* [IN] File object */
                            void* buff,  /* [OUT] Buffer to store read data */
                            UINT btr,    /* [IN] Number of bytes to read */
                            UINT* br     /* [OUT] Number of bytes read */
                            ) {
  int _br = fread(buff, 1, btr, (FILE*) fp->f);
  *br = _br;
  return (_br >= 0) ? FR_OK : FR_DISK_ERR;
}

FRESULT TRS_FS_POSIX::f_readdir (
                               DIR_* dp,      /* [IN] Directory object */
                               FILINFO* fno  /* [OUT] File information structure */
                                  ) {
  while (1) {
    struct dirent* entry = readdir((DIR*) dp->dir);
    if (entry == NULL) {
      closedir((DIR*) dp->dir);
      fno->fname[0] = '\0';
      break;
    }
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (strlen(entry->d_name) > 12) {
      continue;
    }
    strcpy(fno->fname, entry->d_name);
    f_stat(fno->fname, fno);
    //fno->fattrib = 1;
    break;
  }
  return FR_OK;
}

FSIZE_t TRS_FS_POSIX::f_tell (
                            FIL* fp   /* [IN] File object */
                            ) {
  assert(0);
}

FRESULT TRS_FS_POSIX::f_sync (
                            FIL* fp     /* [IN] File object */
                            ) {
  assert(0);
}

FRESULT TRS_FS_POSIX::f_lseek (
                             FIL*    fp,  /* [IN] File object */
                             FSIZE_t ofs  /* [IN] File read/write pointer */
                             ) {
  fseek((FILE*) fp->f, ofs, SEEK_SET);
  return FR_OK;
}
  
FRESULT TRS_FS_POSIX::f_close (
                             FIL* fp     /* [IN] Pointer to the file object */
                             ) {
  fclose((FILE*) fp->f);
  return FR_OK;
}

FRESULT TRS_FS_POSIX::f_unlink (
                              const TCHAR* path  /* [IN] Object name */
                              ) {
  char* abs_path;
  asprintf(&abs_path, "%s/%s", mount, path);
  int r = unlink(abs_path);
  free(abs_path);
  return r ? FR_NO_FILE : FR_OK;
}

FRESULT TRS_FS_POSIX::f_stat (
                            const TCHAR* path,  /* [IN] Object name */
                            FILINFO* fno        /* [OUT] FILINFO structure */
                            ) {
  char* abs_path;
  struct stat s;
  asprintf(&abs_path, "%s/%s", mount, path);
  int r = stat(abs_path, &s);
  free(abs_path);
  if (r != 0) {
    return FR_NO_FILE;
  }
  strcpy(fno->fname, path);
  fno->fsize = s.st_size;
  fno->fattrib = 1; //XXX
  return FR_OK;
}



#include "esp_log.h"
#include "mongoose.h"
#ifdef CONFIG_TRS_IO_PP
#include "fs-spiffs.h"
#endif
#include "spiffs.h"
#include <string.h>


#define TAG "SPIFFS"


typedef struct {
  const char* partition;
  size_t max_files;
} ESP_SPIFFS_PARTITION;

static const ESP_SPIFFS_PARTITION partitions[] = {
  {"html", 10},
#ifdef CONFIG_TRS_IO_PP
  {"roms", 5},
  {"fpga", 1}
#endif
};

static const int num_partitions = sizeof(partitions) / sizeof(ESP_SPIFFS_PARTITION);

static const char* directories[] = {
  "/html/printer"
};

static const int num_directories = sizeof(directories) / sizeof(const char*);


/*
 * The SPIFFS filesystem does not support directories. Mongoose uses
 * stat() to find out if a path is a directory (to send index.html).
 * We use a custom version of stat() that has the directory paths
 * hardcoded in order to recognize them properly.
 */
static int stat_spiffs(const char *path, size_t *size, time_t *mtime) {
  struct stat st;

  if (size) *size = 0;
  if (mtime) *mtime = 0;
  int len = strlen(path);
  if (len == 0) return 0;
  if (path[len - 1] == '/') {
    // Path ends with "/". Has to be a directory
    return MG_FS_READ | MG_FS_DIR;
  }
  for (int i = 0; i < num_directories; i++) {
    if (strcmp(path, directories[i]) == 0) {
      return MG_FS_READ | MG_FS_DIR;
    }
  }
  if (stat(path, &st) != 0) return 0;
  if (size) *size = (size_t) st.st_size;
  if (mtime) *mtime = st.st_mtime;
  return MG_FS_READ | MG_FS_WRITE | (S_ISDIR(st.st_mode) ? MG_FS_DIR : 0);
}

esp_err_t init_spiffs()
{
  esp_err_t ret;

  ESP_LOGI(TAG, "Initializing SPIFFS");

  for (int i = 0; i < num_partitions; i++) {
    const char* p = partitions[i].partition;
    char* path;
    asprintf(&path, "/%s", p);
    const esp_vfs_spiffs_conf_t conf = {
      .base_path = path,
      .partition_label = p,
      .max_files = partitions[i].max_files,
      .format_if_mount_failed = false
    };

    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize '%s' SPIFFS (%s)", p, esp_err_to_name(ret));
    }
  }

#ifdef CONFIG_TRS_IO_PP
  init_fs_roms();
#endif

  // Use custom version of stat() to work around SPIFFS limitations
  mg_fs_posix.st = stat_spiffs;

  return ret;
}


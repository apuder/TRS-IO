
#include "ptrs.h"
#include "settings.h"
#include "trs-lib.h"
#include "trs-fs.h"
#include "spi.h"
#include "wifi.h"
#include "fs-spiffs.h"
#include "ntp_sync.h"
#include <freertos/task.h>



#define MAX_NUM_ROMS 10

static const char** rom_items;
static uint8_t rom_selected[SETTINGS_MAX_ROMS];
static bool rom_type_dirty[SETTINGS_MAX_ROMS];



static form_item_t rom_form_items[] = {
  FORM_ITEM_SELECT_PTR("Model 1", &rom_selected[0], &rom_items, &rom_type_dirty[0]),
  FORM_ITEM_SELECT_PTR("Model III", &rom_selected[2], &rom_items, &rom_type_dirty[2]),
  FORM_ITEM_SELECT_PTR("Model 4", &rom_selected[3], &rom_items, &rom_type_dirty[3]),
  FORM_ITEM_SELECT_PTR("Model 4P", &rom_selected[4], &rom_items, &rom_type_dirty[4]),
	FORM_ITEM_END
};

static form_t rom_form = {
	.title = "ROMs",
	.form_items = rom_form_items
};

void configure_roms()
{
  int i;

  vector<FSFile>& rom_files = the_fs->getFileList();

  // Need to copy the STL vector to a C array since TRS-LIB does not support C++
  rom_items = (const char**) malloc((rom_files.size() + 1) * sizeof(char*));

  for (i = 0; i < SETTINGS_MAX_ROMS; i++) {
    rom_selected[i] = 0xff;
  }
  vector<string>& current_roms = settings_get_roms();
  for (i = 0; i < rom_files.size(); i++) {
    rom_items[i] = (const char*) rom_files.at(i).name.c_str();
    for (int j = 0; j < SETTINGS_MAX_ROMS; j++) {
      if (j == 1) {
        // Skip Model 2
        continue;
      }
      if (rom_selected[j] == 0xff && strcmp(current_roms.at(j).c_str(), rom_items[i]) == 0) {
        rom_selected[j] = i;
        continue;
      }
    }
  }
  rom_items[i] = NULL;

  for (i = 0; i < SETTINGS_MAX_ROMS; i++) {
    if (rom_selected[i] == 0xff) {
      rom_selected[i] = 0;
    }
  }

  form(&rom_form, false);

  bool update_settings = false;

  for (i = 0; i < SETTINGS_MAX_ROMS; i++) {
    printf("ROM %d: %s\n", i + 1, rom_items[rom_selected[i]]);
    // Ignore Model 2
    if (i == 1) continue;
    if (rom_type_dirty[i]) {
      current_roms.at(i) = rom_items[rom_selected[i]];
      update_settings = true;
    }
  }

  if (update_settings) {
    settings_set_roms();
    settings_commit();
  }
  free(rom_items);
}

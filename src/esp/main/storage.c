
#include "storage.h"
#include "nvs_flash.h"

static nvs_handle storage;

void init_storage()
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  err = nvs_open("retrostore", NVS_READWRITE, &storage);
  ESP_ERROR_CHECK(err);
}

void storage_erase()
{
  ESP_ERROR_CHECK(nvs_erase_all(storage));
  ESP_ERROR_CHECK(nvs_commit(storage));
}

bool storage_has_key(const char* key)
{
  size_t len = 0;
  esp_err_t err = nvs_get_str(storage, key, NULL, &len);
  return err != ESP_ERR_NVS_NOT_FOUND;
}

void storage_get(const char* key, char* out, size_t len)
{
  ESP_ERROR_CHECK(nvs_get_str(storage, key, out, &len));
}

void storage_set(const char* key, const char* value)
{
  ESP_ERROR_CHECK(nvs_set_str(storage, key, value));
  ESP_ERROR_CHECK(nvs_commit(storage));
}


#include <trs-io.h>
#include "retrostore-defs.h"
#include "retrostore.h"
#include "version.h"
#include "esp_mock.h"
#include <string.h>


retrostore::RetroStore rs;

#ifdef ESP_PLATFORM
// loader_cmd.bin
extern const uint8_t loader_cmd_start[] asm("_binary_loader_cmd_bin_start");
extern const uint8_t loader_cmd_end[] asm("_binary_loader_cmd_bin_end");

// loader_basic.cmd
extern const uint8_t loader_basic_start[] asm("_binary_loader_basic_cmd_start");
extern const uint8_t loader_basic_end[] asm("_binary_loader_basic_cmd_end");

// rsclient.cmd
extern const uint8_t rsclient_start[] asm("_binary_rsclient_cmd_start");
extern const uint8_t rsclient_end[] asm("_binary_rsclient_cmd_end");
#else

extern unsigned char loader_cmd_bin[];
extern unsigned int loader_cmd_bin_len;

extern unsigned char loader_basic_cmd[];
extern unsigned int loader_basic_cmd_len;

extern unsigned char rsclient_cmd[];
extern unsigned int rsclient_cmd_len;
#endif

#define SIZE_APP_PAGE 16


class RetroStoreModule : public TrsIO {
private:
  std::vector<retrostore::RsAppNano> apps;
  std::vector<retrostore::RsMediaImage> images;
  std::string query;
  int current_page = 0;
  int num_apps  = 0;

public:
  RetroStoreModule(int id) : TrsIO(id) {
    addCommand(static_cast<cmd_t>(&RetroStoreModule::sendVersion), "");
    addCommand(static_cast<cmd_t>(&RetroStoreModule::sendLoaderCMD), "");
    addCommand(static_cast<cmd_t>(&RetroStoreModule::sendBASIC), "");
    addCommand(static_cast<cmd_t>(&RetroStoreModule::sendCMD), "I");
    addCommand(static_cast<cmd_t>(&RetroStoreModule::sendAppTitle), "I");
    addCommand(static_cast<cmd_t>(&RetroStoreModule::sendAppDetails), "I");
    addCommand(static_cast<cmd_t>(&RetroStoreModule::setQuery), "S");
  }

  void sendVersion() {
    addByte(RS_VERSION_MAJOR);
    addByte(RS_VERSION_MINOR);
  }

  void sendLoaderCMD() {
#ifdef ESP_PLATFORM
    addBlob16((void*) loader_cmd_start, loader_cmd_end - loader_cmd_start);
#else
    addBlob16((void*) loader_cmd_bin, loader_cmd_bin_len);
#endif
  }

  void sendBASIC() {
    retrostore::RsMediaImage* image = NULL;
    for (int x = 0; x < images.size(); x++) {
      image = &images[x];
      if (image->type == retrostore::RsMediaType_BASIC) {
        break;
      }
    }
    assert(image != NULL);
    addBlob16(image->data.get(), image->data_size);
  }

  void sendCMD() {
    uint16_t idx = I(0);
    if (idx == 0xffff) {
#ifdef ESP_PLATFORM
      addBlob16((void*) rsclient_start, rsclient_end - rsclient_start);
#else
      addBlob16((void*) rsclient_cmd, rsclient_cmd_len);
#endif
    } else {
      std::vector<retrostore::RsMediaType> types;

      types.push_back(retrostore::RsMediaType_COMMAND);
      types.push_back(retrostore::RsMediaType_BASIC);
      std::string id = apps[idx - current_page * SIZE_APP_PAGE].id;
      apps.clear();
      num_apps = 0;
      images.clear();

      if (!rs.FetchMediaImages(id, types, &images) || images.size() == 0) {
        // Error happened. Just send rsclient again so we send something legal
#ifdef ESP_PLATFORM
        addBlob16((void*) rsclient_start, rsclient_end - rsclient_start);
#else
        addBlob16((void*) rsclient_cmd, rsclient_cmd_len);
#endif
      } else {
        retrostore::RsMediaImage* image = &images[0];
        if (image->type == retrostore::RsMediaType_COMMAND) {
          addBlob16(image->data.get(), image->data_size);
        } else {
          // BASIC loader
#ifdef ESP_PLATFORM
          addBlob16((void*) loader_basic_start, loader_basic_end - loader_basic_start);
#else
	        addBlob16((void*) loader_basic_cmd, loader_basic_cmd_len);
#endif
        }
      }  
    }
  }

  retrostore::RsAppNano* get_app_from_cache(int idx) {
    int page_nr = idx / SIZE_APP_PAGE;
    int offset = idx % SIZE_APP_PAGE;
    if ((page_nr != current_page) || (num_apps == 0)) {
      apps.clear();
      std::vector<retrostore::RsMediaType> hasTypes;
      hasTypes.push_back(retrostore::RsMediaType_COMMAND);
      hasTypes.push_back(retrostore::RsMediaType_BASIC);
      if (!rs.FetchAppsNano(page_nr * SIZE_APP_PAGE, SIZE_APP_PAGE, query, hasTypes, &apps)) {
        return NULL;
      }
      current_page = page_nr;
      num_apps = apps.size();
    }
    if (offset >= num_apps) {
      return NULL;
    }
    return &apps[offset];
  }

  void sendAppTitle() {
    uint16_t idx = I(0);
    retrostore::RsAppNano* app = get_app_from_cache(idx);
    if (app == NULL) {
      addStr("");
      return;
    }
    char title[64] = {'\0'};
    snprintf(title, sizeof(title), "%s (%s)", app->name.c_str(), app->author.c_str());
    addStr(title);
  }

  void appendString(const char* s) {
    while(*s != '\0') {
      addByte(*s++);
    }
  }

  void sendAppDetails() {
    uint16_t idx = I(0);
    retrostore::RsAppNano* appNano = &apps[idx - current_page * SIZE_APP_PAGE];
    retrostore::RsApp app;
    rs.SetMaxDescriptionLength(1024);
    if (!rs.FetchApp(appNano->id, &app)) {
      addStr("");
      return;
    }
    appendString(app.name.c_str());
    appendString("\nAuthor: ");
    appendString(app.author.c_str());
    appendString("\nYear: ");
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", app.release_year);
    appendString(buf);
    appendString("\n\n");
    addStr(app.description.c_str());
  }

  void setQuery() {
    query = S(0);
  }  
};

RetroStoreModule theRetroStoreModule(RETROSTORE_MODULE_ID);

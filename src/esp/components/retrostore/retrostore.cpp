
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

class RetroStoreModule : public TrsIO {
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
    unsigned char* buf;
    int size;
    get_last_app_code(&buf, &size);
    addBlob16(buf, size);
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
      int type;
      unsigned char* buf;
      int size;
      bool ok = get_app_code(idx, &type, &buf, &size);
      if (!ok) {
        // Error happened. Just send rsclient again so we send something legal
#ifdef ESP_PLATFORM
        addBlob16((void*) rsclient_start, rsclient_end - rsclient_start);
#else
        addBlob16((void*) rsclient_cmd, rsclient_cmd_len);
#endif
      } else {
        if (type == 3 /* CMD */) {
          addBlob16(buf, size);
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

  void sendAppTitle() {
    uint16_t idx = I(0);
    char* title = get_app_title(idx);
    addStr(title);
  }

  void sendAppDetails() {
    uint16_t idx = I(0);
    char* details = get_app_details(idx);
    addStr(details);
  }

  void setQuery() {
    set_query(S(0));
  }  
};

RetroStoreModule theRetroStoreModule(RETROSTORE_MODULE_ID);

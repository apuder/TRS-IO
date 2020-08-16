
#include <trs-io.h>
#include "retrostore.h"
#include "version.h"
#include "backend.h"
#include "esp_mock.h"
#include <string.h>

// loader_cmd.bin
extern const uint8_t loader_cmd_start[] asm("_binary_loader_cmd_bin_start");
extern const uint8_t loader_cmd_end[] asm("_binary_loader_cmd_bin_end");

// loader_basic.cmd
extern const uint8_t loader_basic_start[] asm("_binary_loader_basic_cmd_start");
extern const uint8_t loader_basic_end[] asm("_binary_loader_basic_cmd_end");

// rsclient.cmd
extern const uint8_t rsclient_start[] asm("_binary_rsclient_cmd_start");
extern const uint8_t rsclient_end[] asm("_binary_rsclient_cmd_end");


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
    addBlob16((void*) loader_cmd_start, loader_cmd_end - loader_cmd_start);
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
      addBlob16((void*) rsclient_start, rsclient_end - rsclient_start);
    } else {
      int type;
      unsigned char* buf;
      int size;
      bool ok = get_app_code(idx, &type, &buf, &size);
      if (!ok) {
        // Error happened. Just send rsclient again so we send something legal
        addBlob16((void*) rsclient_start, rsclient_end - rsclient_start);
      } else {
        if (type == 3 /* CMD */) {
          addBlob16(buf, size);
        } else {
          // BASIC loader
          addBlob16((void*) loader_basic_start, loader_basic_end - loader_basic_start);
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

static RetroStoreModule theRetroStoreModule(RETROSTORE_MODULE_ID);

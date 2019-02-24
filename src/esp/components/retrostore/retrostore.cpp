
#include <trs-io.h>
#include "retrostore.h"
#include "version.h"
#include "backend.h"
#include "esp_mock.h"
#include <string.h>

// Defined in loader_cmd.c
extern unsigned char loader_cmd_bin[];
extern unsigned int loader_cmd_bin_len;

// Defined in loader_basic.c
extern unsigned char loader_basic_cmd[];
extern unsigned int loader_basic_cmd_len;

// Defined in rsclient.c
extern unsigned char rsclient_cmd[];
extern unsigned int rsclient_cmd_len;


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
    addBlob16(loader_cmd_bin, loader_cmd_bin_len);
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
      addBlob16(rsclient_cmd, rsclient_cmd_len);
    } else {
      int type;
      unsigned char* buf;
      int size;
      bool ok = get_app_code(idx, &type, &buf, &size);
      if (!ok) {
        // Error happened. Just send rsclient again so we send something legal
        addBlob16(rsclient_cmd, rsclient_cmd_len);
      } else {
        if (type == 3 /* CMD */) {
          addBlob16(buf, size);
        } else {
          // BASIC loader
          addBlob16(loader_basic_cmd, loader_basic_cmd_len);
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

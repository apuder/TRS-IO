
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


#define RETROSTORE_MODULE_ID 3

class RetroStoreModule : public virtual TrsIO {
public:
 RetroStoreModule(int id) : TrsIO(id) {
    addCommand(command_send_loader_cmd, "");
    addCommand(command_send_cmd, "I");
    addCommand(command_send_app_title, "I");
    addCommand(command_send_app_details, "I");
    addCommand(command_send_status, "");
    addCommand(command_cmd_configure_wifi, "SS");
    addCommand(command_cmd_set_query, "S");
    addCommand(command_send_version, "");
    addCommand(command_send_wifi_ssid, "");
    addCommand(command_send_wifi_ip, "");
    addCommand(command_send_basic, "");
  }

  static void command_send_loader_cmd() {
    addBlob(loader_cmd_bin, loader_cmd_bin_len);
  }

  static void command_send_cmd() {
    uint16_t idx = I(0);
    if (idx == 0xffff) {
      addBlob(rsclient_cmd, rsclient_cmd_len);
    } else {
      int type;
      unsigned char* buf;
      int size;
      bool ok = get_app_code(idx, &type, &buf, &size);
      if (!ok) {
        // Error happened. Just send rsclient again so we send something legal
        addBlob(rsclient_cmd, rsclient_cmd_len);
      } else {
        if (type == 3 /* CMD */) {
          addBlob(buf, size);
        } else {
          // BASIC loader
          addBlob(loader_basic_cmd, loader_basic_cmd_len);
        }
      }  
    }
  }

  static void command_send_app_title() {
    uint16_t idx = I(0);
    char* title = get_app_title(idx);
    addStr(title);
  }

  static void command_send_app_details() {
    uint16_t idx = I(0);
    char* details = get_app_details(idx);
    addStr(details);
  }

  static void command_send_status() {
    addByte(*get_wifi_status());
  }

  static void command_cmd_configure_wifi() {
    const char* ssid = S(0);
    const char* passwd = S(1);
    set_wifi_credentials(ssid, passwd);
  }

  static void command_cmd_set_query() {
    set_query(S(0));
  }

  static void command_send_version() {
    addByte(RS_RETROCARD_REVISION);
    addByte(RS_RETROCARD_VERSION_MAJOR);
    addByte(RS_RETROCARD_VERSION_MINOR);
  }

  static void command_send_wifi_ssid() {
    const char* ssid = get_wifi_ssid();
    addStr(ssid);
  }

  static void command_send_wifi_ip() {
    const char* ip = get_wifi_ip();
    addStr(ip);
  }

  static void command_send_basic() {
    unsigned char* buf;
    int size;
    get_last_app_code(&buf, &size);
    addBlob(buf, size);
  }
  
};

RetroStoreModule theRetroStoreModule(RETROSTORE_MODULE_ID);

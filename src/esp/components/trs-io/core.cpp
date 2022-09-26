
#include <trs-io.h>
#include "esp_mock.h"
#include "version.h"
#include "led.h"

#ifndef CONFIG_TRS_IO_MODEL_3
#include "retrostore-defs.h"
#include "xray.h"
//#include "io.h"
#include "spi.h"
#include "fileio.h"

retrostore::RsSystemState trs_state;
#endif

#ifdef ESP_PLATFORM
extern const uint8_t xray_load_start[] asm("_binary_xray_load_bin_start");
extern const uint8_t xray_load_end[] asm("_binary_xray_load_bin_end");
static const uint8_t xray_load_len = xray_load_end - xray_load_start;
#else
extern unsigned char xray_load_bin[];
extern unsigned int xray_load_bin_len;
#endif


class TrsIOCoreModule : public TrsIO {
public:
  TrsIOCoreModule(int id) : TrsIO(id) {
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendVersion), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendStatus), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::configureWifi), "SS");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendWifiSSID), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendWifiIP), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::setScreenColor), "B");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::loadXRAYState), "S");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::setLED), "B");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendFPGAVersion), "");
  }

  void sendVersion() {
    addByte(TRS_IO_HARDWARE_REVISION);
    addByte(TRS_IO_VERSION_MAJOR);
    addByte(TRS_IO_VERSION_MINOR);
  }

  void sendStatus() {
    addByte(*get_wifi_status());
  }

  void configureWifi() {
    const char* ssid = S(0);
    const char* passwd = S(1);
    set_wifi_credentials(ssid, passwd);
  }

  void sendWifiSSID() {
    const char* ssid = get_wifi_ssid();
    addStr(ssid);
  }

  void sendWifiIP() {
    const char* ip = get_wifi_ip();
    addStr(ip);
  }

  void setScreenColor() {
#ifdef CONFIG_TRS_IO_MODEL_1
    const uint8_t color = B(0);
    spi_set_screen_color(color);
#endif
  }

  void loadXRAYState() {
#ifndef CONFIG_TRS_IO_MODEL_3
    uint8_t* buf = NULL;
    int token = atoi(S(0));

    if (token == 0) {
      addByte(0xff);
      return;
    }

    // Delete memory regions of old state
    trs_state.regions.clear();

    if (!rs.DownloadState(token, &trs_state)) goto err;
    if (trs_state.regions.size() == 0) goto err;

    addByte(0);

    XRAY_Z80_REGS regs;

    regs.af = trs_state.registers.af;
    regs.bc = trs_state.registers.bc;
    regs.de = trs_state.registers.de;
    regs.hl = trs_state.registers.hl;
    regs.af_p = trs_state.registers.af_prime;
    regs.bc_p = trs_state.registers.bc_prime;
    regs.de_p = trs_state.registers.de_prime;
    regs.hl_p = trs_state.registers.hl_prime;
    regs.ix = trs_state.registers.ix;
    regs.iy = trs_state.registers.iy;
    regs.pc = trs_state.registers.pc;
    regs.sp = trs_state.registers.sp;

    // Poke Z80 register values to XRAM
    addInt(regs.pc);
    spi_set_breakpoint(0, regs.pc);
    buf = ((uint8_t*) &regs) + sizeof(XRAY_Z80_REGS) - 1;
    for (int i = 0; i < sizeof(XRAY_Z80_REGS); i++) {
      spi_xram_poke_data(255 - i, *buf--);
    }

    buf = startBlob16();
    // Look for a memory region that represents a screenshot
    for (int i = 0; i < trs_state.regions.size(); i++) {
      retrostore::RsMemoryRegion* region = &trs_state.regions[i];
      if (region->start == 0x3c00 && region->length == 1024) {
        // Found one
        memcpy(buf, trs_state.regions[i].data.get(), 1024);
        skip(1024);
        break;
      }
    }
    endBlob16();

    // Poke XRAY loader stub into XRAM that will load the context
#ifdef ESP_PLATFORM
    for (int i = 0; i < xray_load_len; i++) {
      spi_xram_poke_code(i, xray_load_start[i]);
    }
#else
    for (int i = 0; i < xray_load_bin_len; i++) {
      spi_xram_poke_code(i, xray_load_bin[i]);
    }
#endif

    return;

  err:
    rewind();
    addByte(0xff);
#else
   addByte(0xfe); // Not XRAY edition
#endif
  }

  void setLED() {
    uint8_t p = B(0);
    bool r = p & (1 << 0);
    bool g = p & (1 << 1);
    bool b = p & (1 << 2);
    bool blink = p & (1 << 3);
    bool auto_off = p & (1 << 4);
    set_led(r, g, b, blink, auto_off);
  }

  void sendFPGAVersion() {
    uint8_t version_major = 0xff;
    uint8_t version_minor = 0xff;
#ifdef CONFIG_TRS_IO_MODEL_1
    uint8_t v = spi_get_fpga_version();
    version_major = (v >> 5) & 7;
    version_minor = v & 0x1f;
#endif
    addByte(version_major);
    addByte(version_minor);
  }
};

TrsIOCoreModule theTrsIOCoreModule(TRS_IO_CORE_MODULE_ID);

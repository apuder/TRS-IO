
#include <trs-io.h>
#include "esp_mock.h"
#include "version.h"
#include "led.h"
#ifdef CONFIG_TRS_IO_ENABLE_XRAY
#include "io.h"
#include "spi.h"
#include "fileio.h"
#endif


extern uint8_t* xray_upper_ram;

extern const uint8_t xray_load_start[] asm("_binary_xray_load_bin_start");
extern const uint8_t xray_load_end[] asm("_binary_xray_load_bin_end");
static const uint8_t xray_load_len = xray_load_end - xray_load_start;


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
#ifdef CONFIG_TRS_IO_ENABLE_XRAY
    const uint8_t color = B(0);
    spi_set_screen_color(color);
#endif
  }

  void loadXRAYState() {
#ifdef CONFIG_TRS_IO_ENABLE_XRAY
    FIL f;
    UINT br, btr;
    XRAY_Z80_REGS regs;
    uint8_t* buf = ((uint8_t*) &regs) + sizeof(XRAY_Z80_REGS) - 1;

    addByte(0);

    // Open file containing state
    FRESULT res = f_open(&f, S(0), FA_READ);
    if (res != FR_OK) goto err;

    // Read Z80 registers
    res = f_read(&f, &regs, sizeof(XRAY_Z80_REGS), &br);
    if (res != FR_OK || br != sizeof(XRAY_Z80_REGS)) goto err;

    // Poke Z80 register values to XRAM
    addInt(regs.pc);
    spi_set_breakpoint(0, regs.pc);
    for (int i = 0; i < sizeof(XRAY_Z80_REGS); i++) {
      spi_xram_poke_data(255 - i, *buf--);
    }

    buf = startBlob16();

    // Read video RAM
    btr = 1024;
    while(true) {
      res = f_read(&f, buf, btr, &br);
      if (res != FR_OK) {
        break;
      }
      if (btr == br) break;
      buf += br;
      btr -= br;
    }
    //if (res != FR_OK || btr != 0) goto err;
    skip(1024);
    endBlob16();

    // Read 32K of RAM
    xray_upper_ram = (uint8_t*) malloc(32 * 1024);
    buf = xray_upper_ram;
    assert(xray_upper_ram != NULL);
    btr = 32 * 1024;
    while(true) {
      res = f_read(&f, buf, 1024, &br);
      if (res != FR_OK) {
        free(xray_upper_ram);
        xray_upper_ram = NULL;
        break;
      }
      if (btr == br) break;
      buf += br;
      btr -= br;
    }
    
    f_close(&f);

    // Poke XRAY loader stub into XRAM that will load the context
    for (int i = 0; i < xray_load_len; i++) {
      spi_xram_poke_code(i, xray_load_start[i]);
    }

    return;

  err:
    rewind();
    addByte(0xff);
#else
   addByte(0xfe); // Not CRAY edition
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
#ifdef CONFIG_TRS_IO_ENABLE_XRAY
    uint8_t v = spi_get_fpga_version();
    version_major = (v >> 5) & 7;
    version_minor = v & 0x1f;
#endif
    addByte(version_major);
    addByte(version_minor);
  }
};

TrsIOCoreModule theTrsIOCoreModule(TRS_IO_CORE_MODULE_ID);


#pragma once

#include "bitstream-src.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <cstring>

#define NOOP				0x02
#define ERASE_SRAM			0x05
#define READ_SRAM			0x03
#define XFER_DONE			0x09
#define READ_IDCODE			0x11
#define INIT_ADDR			0x12
#define READ_USERCODE		0x13
#define CONFIG_ENABLE		0x15
#define XFER_WRITE			0x17
#define CONFIG_DISABLE		0x3A
#define RELOAD				0x3C
#define STATUS_REGISTER		0x41
#  define STATUS_CRC_ERROR			(1 << 0)
#  define STATUS_BAD_COMMAND		(1 << 1)
#  define STATUS_ID_VERIFY_FAILED	(1 << 2)
#  define STATUS_TIMEOUT			(1 << 3)
#  define STATUS_MEMORY_ERASE		(1 << 5)
#  define STATUS_PREAMBLE			(1 << 6)
#  define STATUS_SYSTEM_EDIT_MODE	(1 << 7)
#  define STATUS_PRG_SPIFLASH_DIRECT (1 << 8)
#  define STATUS_NON_JTAG_CNF_ACTIVE (1 << 10)
#  define STATUS_BYPASS				(1 << 11)
#  define STATUS_GOWIN_VLD			(1 << 12)
#  define STATUS_DONE_FINAL			(1 << 13)
#  define STATUS_SECURITY_FINAL		(1 << 14)
#  define STATUS_READY				(1 << 15)
#  define STATUS_POR				(1 << 16)
#  define STATUS_FLASH_LOCK			(1 << 17)

#define ERR_JTAG_UNEXPECTED_IR_LENGTH -1
#define ERR_JTAG_UNEXPECTED_DEVICES -2
#define ERR_JTAG_UNEXPECTED_IDCODE -3

#if 0
// Brown -> GPIO 16
#define TDO 0x08
// Orange -> GPIO 15
#define TDI 0x04
// Red -> GPIO 14
#define TMS 0x02
// Yellow -> GPIO 13
#define TCK 0x01
#else
// Brown -> GPIO 16
#define TDO (1 << (39 - 32))
// Orange -> GPIO 15
#define TDI (1 << 27)
// Red -> GPIO 14
#define TMS (1 << 5)
// Yellow -> GPIO 13
#define TCK (1 << 23)
#endif

// GW2A18PG256: IR length = 8
#define IR_SAMPLE_PRELOAD 0x01
#define IR_READ_USERCODE 0x13
#define IR_IDCODE 0x11
#define IR_HIGHZ 0x0C
#define IR_BYPASS 0xFF



class JTAGAdapter {
private:
  inline void outport(uint32_t d) {
    //d = (d & 7) << 13;
    GPIO.out_w1ts = d & (TCK | TMS | TDI);
    GPIO.out_w1tc = (~d) & (TCK | TMS | TDI);
  }

  inline bool inport() {
    //return GPIO.in >> 13;
    return (GPIO.in1.data & TDO) != 0;
  }

public:
  virtual ~JTAGAdapter() {}
  virtual void setup() = 0;
  int determineChainLength();
  int scan(int expected_ir_len, int expected_devices, uint32_t expected_idcode);

  inline bool pulse(uint32_t d) {
    bool tdo;

    outport(d);
    outport(d | TCK);
    tdo = inport();
    outport(d);

    return tdo;
  }

  inline void reset() {
    for(int i = 0; i < 5; i++) {
      pulse(TMS);
    }

    // Go to Run-Test/Idle
	  pulse(0);
  }

  inline void toggleClk(int n) {
    for (int i = 0; i < n; i++) {
      pulse(0);
    }
  }

  inline void enterShiftIR() {
    pulse(TMS);
    pulse(TMS);
    pulse(0);
    pulse(0);
  }

  inline void enterShiftDR() {
    pulse(TMS);
    pulse(0);
    pulse(0);
  }

  inline void exitShift() {
    pulse(TMS);
    pulse(TMS);
    pulse(0);
  }

  // note: call this function only when in shift-IR or shift-DR state
  inline void sendDataLSB(void* d, int bitlength, bool exit) {
    int bitofs = 0;
    uint8_t* p = (uint8_t*) d;

    bitlength--;
    while(bitlength--) {
      pulse((p[bitofs >> 3] >> (bitofs & 7) & 1) ? TDI : 0);  // send all bits but the last one
      bitofs++;
    }

    uint32_t tms = exit ? TMS : 0;
    pulse((p[bitofs >> 3] >> (bitofs & 7) & 1) ? TDI | tms : tms);  // send last bit, with TMS asserted
  }

  // note: call this function only when in shift-IR or shift-DR state
  inline void sendDataMSB(void* d, int bitlength, bool exit) {
    int bitofs = 0;
    uint8_t* p = (uint8_t*) d;

    bitlength--;
    while(bitlength--) {
      pulse((p[bitofs >> 3] << (bitofs & 7) & 0x80) ? TDI : 0);  // send all bits but the last one
      bitofs++;
    }
    uint32_t tms = exit ? TMS : 0;
    pulse((p[bitofs >> 3] << (bitofs & 7) & 0x80) ? TDI | tms : tms);  // send last bit, with TMS asserted
  }

  // note: call this function only when in shift-IR or shift-DR state
  void readData(void* d, int bitlength) {
    int bitofs = 0;
    uint8_t* p = (uint8_t*) d;

    memset(p, 0, (bitlength + 7) / 8);

    bitlength--;
    while(bitlength--) {
      // Read all bits but the last one
      p[bitofs / 8] |= (pulse(0) << (bitofs & 7));
      bitofs++;
    }

    // Read the last bit, with TMS asserted
    p[bitofs / 8] |= (pulse(TMS) << (bitofs & 7));

    // Return to state Run-Test/Idle
    pulse(TMS);
    pulse(0);
  }

  // note: call this function only when in shift-IR or shift-DR state
  void readDataMSB(void* d, int bitlength) {
    int bitofs = 0;
    uint8_t* p = (uint8_t*) d;

    memset(p, 0, (bitlength + 7) / 8);

    bitlength--;
    while(bitlength--) {
      // Read all bits but the last one
      p[bitofs / 8] |= (pulse(0) << (7 - (bitofs & 7)));
      bitofs++;
    }

    // Read the last bit, with TMS asserted
    p[bitofs / 8] |= (pulse(TMS) << (7 - (bitofs & 7)));

    // Return to state Run-Test/Idle
    pulse(TMS);
    pulse(0);
  }

  inline void readDR(void* p, int bitlength) {
    enterShiftDR();
    readData(p, bitlength);
    toggleClk(6);
  }

  inline void setIR(uint32_t ir) {
    enterShiftIR();
    sendDataLSB(&ir, 8, true);
    pulse(TMS);
    pulse(0);  // go back to Run-Test/Idle
    toggleClk(6);
  }

  uint32_t readStatusReg();
  bool pollFlag(uint32_t mask, uint32_t value);
  bool enableCfg();
  bool disableCfg();
  bool eraseSRAM();
  bool programToSRAM(BitstreamSource* bs, bool log);

};

class JTAGAdapterTrsIO : public JTAGAdapter {
public:
  void setup();
  void testBoundaryScan();
  bool doProgramToSRAM(BitstreamSource* bs);
  void testProgramToFLASH();
};

void uploadFPGAFirmware();

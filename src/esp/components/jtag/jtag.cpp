
// TN653E
// http://cdn.gowinsemi.com.cn/TN653E.pdf


///////////////////////////////////////////////////////////
#include "esp_log.h"
#include <stdio.h>
#include <assert.h>

#include "spi.h"
#include "jtag.h"


void JTAG_SendDataMSB(char* p, int bitlength, bool exit);



int JTAGAdapter::determineChainLength()
{
  int i;

  // Fill chain with 0s
  for(i = 0; i < 1000; i++) {
    pulse(0);
  }

  // Fill chain with 1s and stop when the first 1 is read at TDO
  for(i = 0; i < 1000; i++) {
    if (pulse(TDI)) break;
  }
  exitShift();
  return i;
}

int JTAGAdapter::scan(int expected_ir_len, int expected_devices, uint32_t expected_idcode)
{
  reset();
  enterShiftIR();
  if (determineChainLength() != expected_ir_len) {
    return ERR_JTAG_UNEXPECTED_IR_LENGTH;
  }

  // we are in BYPASS mode since JTAG_DetermineChainLength filled the IR chain full of 1's
  // now we can easily determine the number of devices (= DR chain length when all the devices are in BYPASS mode)
  enterShiftDR();
  int devices = determineChainLength();
  if (devices != expected_devices) {
    return ERR_JTAG_UNEXPECTED_DEVICES;
  }

  // read the IDCODEs (assume all devices support IDCODE, so read 32 bits per device)
  reset();
  uint32_t idcode[devices];
  readDR(idcode, 32 * devices);
  for(int i = 0; i < devices; i++) {
    if (idcode[i] != expected_idcode) {
      return ERR_JTAG_UNEXPECTED_IDCODE;
    }
  }
  return 0;
}

uint32_t JTAGAdapter::readStatusReg()
{
  uint32_t reg;
  uint8_t rx[4];

  setIR(STATUS_REGISTER);
  readDR(rx, sizeof(rx) * 8);
  reg = rx[3] << 24 | rx[2] << 16 | rx[1] << 8 | rx[0];
  return reg;
}

bool JTAGAdapter::pollFlag(uint32_t mask, uint32_t value) {
  uint32_t status;
  long timeout = 0;

  do {
    status = readStatusReg();
    if (timeout == 100000000){
      return false;
    }
    timeout++;
  } while ((status & mask) != value);

  return true;
}

bool JTAGAdapter::enableCfg()
{
  setIR(CONFIG_ENABLE);
  return pollFlag(STATUS_SYSTEM_EDIT_MODE, STATUS_SYSTEM_EDIT_MODE);
}

bool JTAGAdapter::disableCfg()
{
  setIR(CONFIG_DISABLE);
  setIR(NOOP);
  return pollFlag(STATUS_SYSTEM_EDIT_MODE, 0);
}

/* Erase SRAM:
 * TN653 p.9-10, 14 and 31
 */
bool JTAGAdapter::eraseSRAM()
{
  setIR(ERASE_SRAM);
  setIR(NOOP);

  /* TN653 specifies to wait for 4ms with
  * clock generated but
  * status register bit MEMORY_ERASE goes low when ERASE_SRAM
  * is send and goes high after erase
  * this check seems enough
  */
  if (pollFlag(STATUS_MEMORY_ERASE, STATUS_MEMORY_ERASE)) {
    return true;
  } else {
    return false;
  }
}

bool JTAGAdapter::programToSRAM(BitstreamSource* bs, bool log)
{
  setIR(READ_IDCODE);
  uint32_t id;
  readDR(&id, 32);
  if (id != 0x081b) {
    return false;
  }

  /* erase SRAM */
  if (!enableCfg()) {
    return false;
  }
  if (!eraseSRAM()) {
    return false;
  }
  if (!disableCfg()) {
    return false;
  }

  /* load bitstream in SRAM */
  if (!enableCfg()) {
    return false;
  }

  setIR(XFER_WRITE);  // Transfer Configuration Data
  enterShiftDR();

  unsigned long count = 0;
  unsigned long total_count = 0;
  int br;

  do {
    static char buf[1024];

    if (!bs->read(buf, sizeof(buf), &br)) {
      return false;
    } else {
      if (br != 0) {
        sendDataMSB(buf, br * 8, br != sizeof(buf));
        if (log) {
          count += br;
          total_count += br;
          if (count > 100000) {
            count -= 100000;
            ESP_LOGI("JTAG", "Bytes written: %ld", total_count);
          }
        }
      }
    }
  } while (br != 0);
  pulse(TMS);
  pulse(0);  // go back to Run-Test/Idle

  setIR(XFER_DONE);  // XFER_DONE
  if (!pollFlag(STATUS_DONE_FINAL, STATUS_DONE_FINAL)) {
    return false;
  }
      
  return disableCfg();
}

void JTAGAdapterTrsIO::setup()
{
  gpio_config_t gpioConfig;

  gpioConfig.pin_bit_mask = GPIO_SEL_5 | GPIO_SEL_23 | GPIO_SEL_27;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  gpioConfig.pin_bit_mask = GPIO_SEL_39;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpio_config(&gpioConfig);
}


uint8_t reverse(uint8_t v)
{
  uint8_t b = 0;
  for(int i = 0; i < 8; i++) {
    b <<= 1;
    b |= (v & 1);
    v >>= 1;
  }
  return b;
}

///////////////////////////////////////////////////////////

/*
 * Uses the JTAG boundary scan to toggle the pads of the 20K. This
 * will make the four LEDs on the TRS-IO++ board blink. The blinking
 * is not caused by the FPGA, but by the ESP toggling the pads.
 */
void JTAGAdapterTrsIO::testBoundaryScan()
{
  setup();
  reset();

  uint32_t LED = 0;

  while (1) {
    LED ^= TDI;
    const uint8_t EXTEST = 0x04;

    setIR(EXTEST);
    enterShiftDR();

    for (int i = 0; i < 760 - 1; i++) {
      switch (i) {
        case 337: // LED[3] == D10
        case 339: // LED[2] == E10
        case 687: // LED[1] == R7
        case 685: // LED[0] == P7
          pulse(LED);
          break;
        default:
          pulse(0);
          break;
      }
    }
    pulse(TMS);
    pulse(0);
    pulse(TMS);
    pulse(TMS);
    pulse(0);

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  // Never reached
}

void JTAGAdapterTrsIO::doProgramToSRAM(const char* path)
{
  setup();
  reset();
  // Section 2.2.5, IDCODE for GW2A(R)-18
  int err = scan(8, 1, 0x0000081B);
  if (err != 0) {
    ESP_LOGE("JTAG", "Unexpected scan result: %d", err);
    return;
  }

  ESP_LOGI("JTAG", "Opening bitstream '%s'", path);
  BitstreamSourceFile* bs = new BitstreamSourceFile(path);
  if (!bs->open()) {
    ESP_LOGE("JTAG", "Open failed");
  } else {
    ESP_LOGI("JTAG", "Programming to SRAM");
    if (!programToSRAM(bs, true)) {
      ESP_LOGE("JTAG", "Program failed");
      return;
    }
    if (!bs->close()) {
      ESP_LOGE("JTAG", "Close failed");
      return;
    }
    ESP_LOGI("JTAG", "Programming SRAM succeeded");
    delete bs;
  }
}

void JTAGAdapterTrsIO::testProgramToFLASH()
{
  setup();
  reset();

  setIR(0x16); // Sec 2.2.12 Bridge to SPI
  pulse(TMS);  // Figure 2.22
  pulse(0);
  pulse(0);
  pulse(TMS);
  pulse(0);
  pulse(TMS);
  pulse(0); // Shift-DR
  char b = 0x9f; // XTX: Sec 6.25 Read Identification (RDID)
  sendDataMSB(&b, 8, false);
  pulse(0);
  char id[3];
  readDataMSB(id, 3 * 8);
  ESP_LOGI("JTAG", "Flash ID: %02x %02x %02x", id[0], id[1], id[2]);

  // Return to Test-Logic Reset
  for(int i = 0; i < 5; i++) {
    pulse(TMS);
  }
}

static JTAGAdapterTrsIO jtag;

void uploadFPGAFirmware()
{
  char* path;
  uint8_t mode = spi_get_config();
  asprintf(&path, "firmware-%d.bin", mode & 0x0f);
  jtag.doProgramToSRAM(path);
  free(path);
}
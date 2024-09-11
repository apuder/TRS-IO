
#pragma once

#include <trs-io.h>
#include <xfer-params.h>


#define XFER_MODULE_ID 5

#define MAX_FNAME_LEN 15

#define XFER_CMD_DIR 0
#define XFER_CMD_OPEN 1



class XFERModule : public TrsIO {
private:
  bool xferRunning;
  uint16_t resultLen;
  uint8_t* result;
  xfer_in_t in;
  xfer_out_t* out;

public:
  XFERModule(int id);
  void doStartXFER();
  void doStopXFER();
  void doNextCMD();
  void doSendResult();
  bool sendCMD(uint16_t len, const void* cmd);
  bool getResult(uint16_t& len, const void*& result);
  void consumedResult();
  bool dosDIR(uint8_t drive, dos_err_t& err, uint16_t& count, dir_buf_t*& entries);
  bool dosOPEN(dos_err_t& err, const char* fn);
  bool dosINIT(dos_err_t& err, const char* fn);
  bool dosREAD(dos_err_t& err, uint16_t& count, sector_t*& sector);
  bool dosWRITE(dos_err_t& err, uint16_t count, sector_t* sector);
  bool dosCLOSE(dos_err_t& err);
};

extern XFERModule xferModule;

#include <xfer.h>
#include <spi.h>
#include <event.h>


using namespace std;


XFERModule::XFERModule(int id) : TrsIO(id) {
  addCommand(static_cast<cmd_t>(&XFERModule::doStartXFER), "");
  addCommand(static_cast<cmd_t>(&XFERModule::doStopXFER), "");
  addCommand(static_cast<cmd_t>(&XFERModule::doNextCMD), "");
  addCommand(static_cast<cmd_t>(&XFERModule::doSendResult), "Z");
  xferRunning = false;
}

void XFERModule::doStartXFER() {
  xferRunning = true;
}
  
void XFERModule::doStopXFER() {
  xferRunning = false;
}
  
void XFERModule::doNextCMD() {
  // Make the Z80 wait until the ESP sends a request
  evt_signal(EVT_XFER_Z80_WAITING);
  deferDone();
}

void XFERModule::doSendResult() {
  resultLen = ZL(0);
  result = Z(0);
  evt_signal(EVT_XFER_RESULT_READY);
  evt_wait(EVT_XFER_RESULT_CONSUMED);
}

bool XFERModule::sendCMD(uint16_t len, const void* cmd) {
  if (!xferRunning) {
    return false;
  }
  evt_wait(EVT_XFER_Z80_WAITING);
  uint8_t* buf = startBlob16();
  if (len > getSendBufferFreeSize()) {
    return false;
  }
  memcpy(buf, cmd, len);
  skip(len);
  endBlob16();
  spi_trs_io_done();
  return true;
}

bool XFERModule::getResult(uint16_t& len, const void*& resultBuf) {
  if (!xferRunning) {
    return false;
  }
  evt_wait(EVT_XFER_RESULT_READY);
  len = resultLen;
  resultBuf = result;
  return true;
}

void XFERModule::consumedResult() {
  evt_signal(EVT_XFER_RESULT_CONSUMED);
}

bool XFERModule::dosDIR(uint8_t drive, dos_err_t& err, uint16_t& n, dir_buf_t*& entries) {
  in.cmd = XFER_CMD_DIR;
  in.params.dir.drive = drive;
  uint16_t len = 0;

  if (!xferModule.sendCMD(sizeof(cmd_in_dir_t) + 1, &in)) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  if (len == 0) {
    return false;
  }
  err = out->dir.err;
  n = out->dir.n;
  entries = &out->dir.entries;
  return true;
}

bool XFERModule::dosOPEN(dos_err_t& err, const char* fn) {
  uint16_t len = 0;

  in.cmd = XFER_CMD_OPEN;

  if (strlen(fn) > MAX_FNAME_LEN) {
    return false;
  }
  strcpy(in.params.open.fn, fn);
  if (!xferModule.sendCMD(sizeof(cmd_in_open_t) + 1, &in)) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  if (len != 1) {
    return false;
  }
  err = out->open.err;
  return true;
}

bool XFERModule::dosINIT(dos_err_t& err, const char* fn) {
  uint16_t len = 0;

  in.cmd = XFER_CMD_INIT;

  if (strlen(fn) > MAX_FNAME_LEN) {
    return false;
  }
  strcpy(in.params.init.fn, fn);
  if (!xferModule.sendCMD(sizeof(cmd_in_init_t) + 1, &in)) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  if (len != 1) {
    return false;
  }
  err = out->init.err;
  return true;
}

bool XFERModule::dosREAD(dos_err_t& err, uint16_t& count, sector_t*& sector) {
  uint16_t len = 0;

  in.cmd = XFER_CMD_READ;
  if (!xferModule.sendCMD(sizeof(cmd_in_read_t) + 1, &in)) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  err = out->read.err;
  count = out->read.count;
  sector = &out->read.sector;
  return true;  
}

bool XFERModule::dosWRITE(dos_err_t& err, uint16_t count, sector_t* sector) {
  uint16_t len = 0;

  in.cmd = XFER_CMD_WRITE;
  in.params.write.count = count;
  memcpy(&in.params.write.sector, sector, sizeof(sector_t));
  if (!xferModule.sendCMD(sizeof(cmd_in_write_t) + 1, &in)) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  if (len != 1) {
    return false;
  }
  err = out->write.err;
  return true;  
}

bool XFERModule::dosCLOSE(dos_err_t& err) {
  uint16_t len = 0;

  in.cmd = XFER_CMD_CLOSE;
  if (!xferModule.sendCMD(1, &in) != 0) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  if (len != 1) {
    return false;
  }
  err = out->close.err;
  return true;  
}

bool XFERModule::dosREMOVE(dos_err_t& err, const char* fn) {
  uint16_t len = 0;

  in.cmd = XFER_CMD_REMOVE;

  if (strlen(fn) > MAX_FNAME_LEN) {
    return false;
  }
  strcpy(in.params.remove.fn, fn);
  if (!xferModule.sendCMD(sizeof(cmd_in_remove_t) + 1, &in)) {
    return false;
  }
  if (!xferModule.getResult(len, (const void*&) out)) {
    return false;
  }
  if (len != 1) {
    return false;
  }
  err = out->remove.err;
  return true;
}


void test_xfer()
{
  // Test for DIR
  dos_err_t err;
  uint16_t n;
  dir_buf_t* dir;
  sector_t sector;

  if (!xferModule.dosDIR(0, err, n, dir)) {
    printf("Err dosDIR\n");
    return;
  }
  if (err != NO_ERR) {
    printf("dosDIR err: %d\n", err);
  } else {
    for(int i = 0; i < n; i++) {
      dir_t* d = &(*dir)[i];
      uint16_t size = DIR_ENTRY_SIZE(d);
      printf("%.15s  %6d\n", d->fname, size);
      d++;
    }
  }
  xferModule.consumedResult();

  // Test for writing to a file called XFER/TXT
  if (!xferModule.dosINIT(err, "XFER/TXT")) {
    printf("Err dosINIT");
    return;
  }
  if (err != NO_ERR) {
    printf("dosINIT err: %d\n", err);
    xferModule.consumedResult();
    return;
  }
  xferModule.consumedResult();

  // Write 300 bytes to the file
  uint8_t v = 0;
  uint8_t idx = 0;
  for (int i = 0; i < 300; i++) {
    sector[idx++] = v++;
    if (idx == 0) {
      // Sector is full
      if (!xferModule.dosWRITE(err, sizeof(sector_t), &sector)) {
        printf("Err dosWRITE\n");
        return;
      }
      if (err != NO_ERR) {
        printf("dosWRITE err: %d\n", err);
        xferModule.consumedResult();
        return;
      }
      xferModule.consumedResult();
    }
  }
  if (!xferModule.dosWRITE(err, idx, &sector)) {
    printf("Err dosWRITE\n");
    return;
  }
  if (err != NO_ERR) {
    printf("dosWRITE err: %d\n", err);
    xferModule.consumedResult();
    return;
  }
  xferModule.consumedResult();
  if (!xferModule.dosCLOSE(err)) {
    printf("Err dosCLOSE\n");
    return;
  }
  xferModule.consumedResult();

  // Read back that some file
  if (!xferModule.dosOPEN(err, "XFER/TXT")) {
    printf("Err dosOPEN");
    return;
  }
  if (err != NO_ERR) {
    printf("dosOPEN err: %d\n", err);
    xferModule.consumedResult();
    return;
  }
  xferModule.consumedResult();

  // Read 300 bytes
  v = 0;
  uint16_t count;
  sector_t* sector_in;
  do {
    if (!xferModule.dosREAD(err, count, sector_in)) {
      printf("Err dosREAD\n");
      return;
    }
    xferModule.consumedResult();
    // Verify content
    for (int i = 0; i < count; i++) {
      if ((*sector_in)[i] != v++) {
        printf("Wrong value at index: %d\n", i);
      }
    }
  } while(count != 0);
  if (!xferModule.dosCLOSE(err)) {
    printf("Err dosCLOSE\n");
    return;
  }
  xferModule.consumedResult();

  // Remove file XFER/TXT
  if (!xferModule.dosREMOVE(err, "XFER/TXT")) {
    printf("Err dosREMOVE");
    return;
  }
  if (err != NO_ERR) {
    printf("dosREMOVE err: %d\n", err);
    xferModule.consumedResult();
    return;
  }
  xferModule.consumedResult();

  printf("Test completed\n");
}

XFERModule xferModule(XFER_MODULE_ID);


#include <trs-io.h>
#include <trs-fs.h>
#include <unordered_map>

#ifdef ESP_PLATFORM
#include "serial.h"
#include "posix.h"
#include "smb.h"
#include "settings.h"
#include "event.h"

extern "C" {
#include "trs_hard.h"
}
#endif

#define TRS_FS_VERSION_MAJOR 1
#define TRS_FS_VERSION_MINOR 0

#define TRS_FS_MODULE_ID 4

TRS_FS* trs_fs = NULL;

#ifdef ESP_PLATFORM
static TRS_FS* current_trs_fs = NULL;
static TRS_FS_POSIX* trs_fs_posix = NULL;
static TRS_FS_SMB* trs_fs_smb = NULL;

static const char* frehd_msg = NULL;

static void check_frehd() {
  FIL f;
  FRESULT ret;
  
  frehd_msg = NULL;

  if (trs_fs == NULL) {
    frehd_msg = "SMB/SD not mounted";
    return;
  }
  
  ret = f_open(&f, "FREHD.ROM", FA_OPEN_EXISTING | FA_READ);
  if (ret == FR_OK) {
    f_close(&f);
    return;
  }
  // FREHD.ROM not found. Check for hard4-0
  ret = f_open(&f, "hard4-0", FA_OPEN_EXISTING | FA_READ);
  if (ret == FR_OK) {
    f_close(&f);
    return;
  }
  // FreHD images not found
  frehd_msg = "FREHD.ROM not found";
}

static void set_fs() {
  if (trs_fs_posix != NULL && trs_fs_posix->get_err_msg() == NULL) {
    // We have a mounted SD card. This has higher precedent
    trs_fs = trs_fs_posix;
  } else {
    trs_fs = trs_fs_smb;
  }
  if (trs_fs != current_trs_fs && trs_fs->get_err_msg() == NULL) {
    if (current_trs_fs != NULL) {
      TRS_FS* new_trs_fs = trs_fs;
      trs_fs = current_trs_fs;
      close_drives();
      trs_fs = new_trs_fs;
    }
    open_drives();
  }
  current_trs_fs = trs_fs;
  if (trs_fs != NULL && trs_fs->get_err_msg() == NULL) {
    evt_signal(EVT_FS_MOUNTED);
  }
  check_frehd();
}

const char* init_trs_fs_posix() {
  if (trs_fs_posix != NULL) {
    if (current_trs_fs == trs_fs_posix) {
      close_drives();
      current_trs_fs = NULL;
    }
    delete trs_fs_posix;
  }
  trs_fs_posix = new TRS_FS_POSIX();
  set_fs();
  return trs_fs_posix->get_err_msg();
}

const char* init_trs_fs_smb() {
  if (trs_fs_smb != NULL) {
    if (current_trs_fs == trs_fs_smb) {
      close_drives();
      current_trs_fs = NULL;
    }
    delete trs_fs_smb;
  }
  trs_fs_smb = new TRS_FS_SMB();
  set_fs();
  return trs_fs_smb->get_err_msg();
}

const char* get_smb_err_msg() {
  if (trs_fs_smb == NULL) {
    return "SMB not connected";
  }
  return trs_fs_smb->get_err_msg();
}

const char* get_posix_err_msg() {
  if (trs_fs_posix == NULL) {
    return "No SD card";
  }
  return trs_fs_posix->get_err_msg();
}

const char* get_frehd_msg() {
  return frehd_msg;
}

bool trs_fs_has_sd_card_reader() {
  if (trs_fs_posix == NULL) {
    return false;
  }
  return trs_fs_posix->has_sd_card_reader();
}
#endif

using namespace std;


class TrsFileSystemModule : public TrsIO {
public:
  TrsFileSystemModule(int id) : TrsIO(id) {
    //trs_fs = new TRS_FS_SMB();
    addCommand(static_cast<cmd_t>(&TrsFileSystemModule::doVersion), "BB");
    addCommand(static_cast<cmd_t>(&TrsFileSystemModule::doOpen), "SB");
    addCommand(static_cast<cmd_t>(&TrsFileSystemModule::doWrite), "BX");
    addCommand(static_cast<cmd_t>(&TrsFileSystemModule::doRead), "BL");
    addCommand(static_cast<cmd_t>(&TrsFileSystemModule::doClose), "B");
  }

  uint8_t clientVersionMajor;
  uint8_t clientVersionMinor;
  
  uint8_t nextFd = 0;
  unordered_map<uint8_t, FIL> fileMap;
  
public:
  void doVersion() {
    clientVersionMajor = B(0);
    clientVersionMinor = B(1);
    addByte(TRS_FS_VERSION_MAJOR);
    addByte(TRS_FS_VERSION_MINOR);
  }

  void doOpen() {
    const char* path = S(0);
    uint8_t mode = B(0);
    uint8_t fd = nextFd++;
    FRESULT result = f_open(&fileMap[fd], path, mode);
    addByte(result);
    if (result == FR_OK) {
      addByte(fd);
    } else {
      nextFd--;
    }
  }

  void doWrite() {
    FIL* fp = &fileMap[B(0)];
    uint32_t bufferLen = XL(0);
    uint8_t* buffer = X(0);
    UINT bw;
    FRESULT result = f_write(fp, buffer, bufferLen, &bw);
    addByte(result);
    if (result == FR_OK) {
      addLong(bw);
    }
  }

  void doRead() {
    FIL* fp = &fileMap[B(0)];
    uint32_t length = L(0);

    addByte(FR_OK);
    uint8_t* buf = startBlob32();
    if (length > getSendBufferFreeSize()) {
      length = getSendBufferFreeSize();
    }
    UINT br;
    FRESULT result = f_read(fp, buf, length, &br);
    if (result != FR_OK) {
      rewind();
      addByte(result);
    } else {
      skip(br);
      endBlob32();
    }
  }
  
  void doClose() {
    FIL* fp = &fileMap[B(0)];
    FRESULT result = f_close(fp);
    fileMap.erase(B(0));
    addByte(result);
  }
};

TrsFileSystemModule theTrsFileSystemModule(TRS_FS_MODULE_ID);

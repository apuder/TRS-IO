
#include <trs-io.h>
#include "serial.h"
#include "smb.h"

#include "fileio.h"

#define TRS_FS_MODULE_ID 4

TRS_FS* trs_fs = NULL;

const char* init_trs_fs() {
  if (trs_fs != NULL) {
    delete trs_fs;
  }
  trs_fs = new TRS_FS_SMB();
  return trs_fs->get_err_msg();
  //trs_fs = new TRS_FS_SERIAL();
}

const char* get_smb_err_msg() {
  if (trs_fs == NULL) {
    return "SMB not connected";
  }
  return trs_fs->get_err_msg();
}

class TrsFileSystemModule : public TrsIO {
public:
  TrsFileSystemModule(int id) : TrsIO(id) {
    //trs_fs = new TRS_FS_SMB();
  }
};

static TrsFileSystemModule theTrsFileSystemModule(TRS_FS_MODULE_ID);

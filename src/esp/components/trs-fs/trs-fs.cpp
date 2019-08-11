
#include <trs-io.h>
#include "serial.h"
#include "smb.h"

#include "fileio.h"

#define TRS_FS_MODULE_ID 4

TRS_FS* trs_fs;

void init_trs_fs() {
  trs_fs = new TRS_FS_SMB();

#if 0
  FIL fp;
  f_open(&fp, "FREHD.ROM", FA_READ);
  f_close(&fp);
  
  DIR_ d;
  f_opendir(&d, "/");
  while (1) {
    FILINFO f;
    f_readdir(&d, &f);
    if (f.fname[0] == '\0') {
      break;
    }
    printf("%s\n", f.fname);
  }
#endif
  //trs_fs = new TRS_FS_SERIAL();
}

class TrsFileSystemModule : public TrsIO {
public:
  TrsFileSystemModule(int id) : TrsIO(id) {
    //trs_fs = new TRS_FS_SMB();
  }
};

static TrsFileSystemModule theTrsFileSystemModule(TRS_FS_MODULE_ID);

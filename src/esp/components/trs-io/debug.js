let debugStatus = {};
debugStatus.smb_url = '';
debugStatus.smb_user = '';
debugStatus.smb_err = '';
debugStatus.posix_err = '';
debugStatus.has_sd_card = false;
debugStatus.hardware_rev = 42;
debugStatus.vers_major = 1;
debugStatus.vers_minor = 23;
debugStatus.wifi_status = 2;
document.addEventListener('keydown', (evt) => {
  switch (evt.key) {
    case '1':
      // SDCard: None, SMB: Success
      debugStatus.smb_err = '';
      debugStatus.smb_url = 'foo';
      debugStatus.smb_user = 'bar';
      break;
    case '2':
      // SDCard: None, SMB: fail
      debugStatus.smb_err = 'Ouch';
      debugStatus.smb_url = 'foo';
      debugStatus.smb_user = 'bar';
      break;
    case '3':
      // SDCard: None, SMB: not configured
      debugStatus.smb_err = '';
      debugStatus.smb_url = '';
      debugStatus.smb_user = '';
      break;
    case '4':
      // SDCard: Success
      debugStatus.has_sd_card = true;
      debugStatus.posix_err = '';
      break;
    case '5':
      // SDCard: Fail
      debugStatus.has_sd_card = true;
      debugStatus.posix_err = 'Ouch';
      break;
    case '6':
      // SDCard: No support.
      debugStatus.has_sd_card = false;
      break;
  }
  updateStatus(debugStatus, true);
});

console.log('DebugJS loaded.');
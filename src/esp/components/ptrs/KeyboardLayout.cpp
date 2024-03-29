#include "settings.h"
#include "KeyboardLayout.h"
#include "esp_log.h"
#include "trs-lib.h"

extern fabgl::PS2Controller PS2Controller;



uint8_t updateKbdLayout() {
    uint8_t curLayout = settings_get_KeyboardLayout();
    return updateKbdLayoutu8(curLayout);
}
  
uint8_t updateKbdLayoutu8(uint8_t curLayout) {
//  curLayoutIdx = curLayout;
  printf("update Keyboard Layoutu8 %02x\n",curLayout);
  if (PS2Controller.keyboard() != nullptr){
   if (!PS2Controller.keyboard()->isKeyboardAvailable() ) {
	printf("isKeyboardAvailable NO\n");
	ESP_LOGE("KBLayout", "Failed to isKeyboardAvailable()");
   }
   else{
	printf("isKeyboardAvailable YES\n");
   	PS2Controller.keyboard()->setLayout(SupportedLayouts::layouts()[curLayout]);
   }
    printf("update Keyboard Layout %02x %s\n",curLayout,SupportedLayouts::shortNames()[curLayout]);
  }
    return curLayout;
}





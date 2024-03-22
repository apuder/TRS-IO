#include "fabgl.h"
#include "settings.h"
#include "KeyboardLayout.h"
#include "esp_log.h"
#include "trs-lib.h"

static const char* kbitems[] = {
  "DE",
  "IT",
  "UK",
  "US",
  "ES",
  "FR",
  "BE",
  "NO",
  NULL};

static  uint8_t curLayoutIdx  = DEFAULT_US_LAYOUT;
extern fabgl::PS2Controller PS2Controller;

static bool Keyboard_Layout_dirty;

static form_item_t kbLayout_form_items[] = {
  FORM_ITEM_SELECT("Keyboard Layout", &curLayoutIdx, SupportedLayouts::shortNames(), &Keyboard_Layout_dirty),
//	FORM_ITEM_SELECT("Keyboard layout", &curLayoutIdx,kbitems, &Keyboard_Layout_dirty),
  FORM_ITEM_END
};
void stringToCharArray(char* charArray, const char* str) {
    while (*str != '\0') {
        *charArray = *str;
        str++;
        charArray++;
    }
    *charArray = '\0';
}

static form_t kbLayout_form = {
	.title = "Keyboard Layout",
	.form_items = kbLayout_form_items
};

void configure_Keyboard_Layout()
{
  form(&kbLayout_form, false);

  if (Keyboard_Layout_dirty) {
    // Set Keyboard Layout
    settings_set_KeyboardLayout(curLayoutIdx);
    updateKbdLayoutu8(curLayoutIdx);
    settings_commit();

  }
}



uint8_t updateKbdLayout() {
    curLayoutIdx = settings_get_KeyboardLayout();
    return updateKbdLayoutu8(curLayoutIdx);
}
  
uint8_t updateKbdLayoutu8(uint8_t curLayout) {
  curLayoutIdx = curLayout;
  printf("update Keyboard Layoutu8 %02x\n",curLayoutIdx);
  if (PS2Controller.keyboard() != NULL){
   if (!PS2Controller.keyboard()->isKeyboardAvailable() ) {
	printf("isKeyboardAvailable NO\n");
	ESP_LOGE("KBLayout", "Failed to isKeyboardAvailable()");
   }
   else{
	printf("isKeyboardAvailable YES\n");
   	PS2Controller.keyboard()->setLayout(SupportedLayouts::layouts()[curLayoutIdx]);
   }
    printf("update Keyboard Layout %02x %s\n",curLayoutIdx,SupportedLayouts::shortNames()[curLayoutIdx]);
  }
    return curLayoutIdx;
}





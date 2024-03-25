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
  "JP",
  NULL};

static const char** kbShortNames_items;
static  uint8_t curLayoutIdx  = DEFAULT_US_LAYOUT;
extern fabgl::PS2Controller PS2Controller;

static bool Keyboard_Layout_dirty;

static form_item_t kbLayout_form_items[] = {
//	FORM_ITEM_SELECT("Keyboard layout", &curLayoutIdx,kbitems, &Keyboard_Layout_dirty),
	FORM_ITEM_SELECT_PTR("Keyboard layout", &curLayoutIdx,&kbShortNames_items, &Keyboard_Layout_dirty),
  FORM_ITEM_END
};

void init_kbShortNames(){
  uint8_t noOfCountrys = SupportedLayouts::count();
  uint8_t i;
  kbShortNames_items = (const char**) malloc((noOfCountrys + 1) * sizeof(char*));
  for (i = 0; i < noOfCountrys; i++) {
     kbShortNames_items[i] = SupportedLayouts::shortNames()[i];
  }
  kbShortNames_items[i] = NULL;
}

static form_t kbLayout_form = {
	.title = "Keyboard Layout",
	.form_items = kbLayout_form_items
};

void configure_Keyboard_Layout()
{
  init_kbShortNames();
  form(&kbLayout_form, false);

  if (Keyboard_Layout_dirty) {
    // Set Keyboard Layout
    settings_set_KeyboardLayout(curLayoutIdx);
    updateKbdLayoutu8(curLayoutIdx);
    settings_commit();

  }
  free(kbShortNames_items);
}



uint8_t updateKbdLayout() {
    curLayoutIdx = settings_get_KeyboardLayout();
    return updateKbdLayoutu8(curLayoutIdx);
}
  
uint8_t updateKbdLayoutu8(uint8_t curLayout) {
  curLayoutIdx = curLayout;
  printf("update Keyboard Layoutu8 %02x\n",curLayoutIdx);
  if (PS2Controller.keyboard() != nullptr){
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





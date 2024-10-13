
#include "keyb.h"
#include "fabgl.h"
#include "ptrs.h"
#include "spi.h"
#include "settings.h"
#include <esp_log.h>

static fabgl::PS2Controller PS2Controller;


#define ADD_SHIFT_KEY 0x100
#define REMOVE_SHIFT_KEY 0x200

typedef struct {
  uint8_t offset;
  uint16_t mask;
} TRSKey;


/*
Address   1     2     4     8     16     32     64     128    Hex Address
------- ----- ---,- ----- ----- -----  -----  -----   -----   -----------
14337     @     A     B     C      D      E      F      G        3801       1    1
14338     H     I     J     K      L      M      N      O        3802       2    2
14340     P     Q     R     S      T      U      V      W        3804       4    3
14344     X     Y     Z     ,      -      -      -      -        3808       8    4
14352     0     1!    2"    3#     4$     5%     6&     7'       3810      16    5
14368     8(    9)   *:    +;     <,     =-     >.     ?/        3820      32    6
14400  enter  clear break  up    down   left  right  space       3840      64    7
14464  shift    -     -     -  control    -      -      -        3880     128    8
*/

/* Full Model 4 keyboard:
Address   1     2     4     8     16     32     64     128    Hex Address
------- ----- ----- ----- ----- -----  -----  -----   -----   -----------
14337     @     A     B     C      D      E      F      G        3801       1    1
14338     H     I     J     K      L      M      N      O        3802       2    2
14340     P     Q     R     S      T      U      V      W        3804       4    3
14344     X     Y     Z     ,      -      -      -      -        3808       8    4
14352     0     1!    2"    3#     4$     5%     6&     7'       3810      16    5
14368     8(    9)   *:    +;     <,     =-     >.     ?/        3820      32    6
14400  enter  clear break  up    down   left  right  space       3840      64    7
14464  Lshift Rshft contrl caps   F1     F2     F3      -        3880     128    8

Additionally:
Shift-0 is now F4
Clear is on home,\
config is on F5
screenshot on F6
*/

static const TRSKey trsKeys[] = {
  {0, 0}, // VK_NONE
  {7, 128}, // VK_SPACE
  {5, 1}, //  VK_0
  {5, 2}, //  VK_1
  {5, 4}, //  VK_2
  {5, 8}, //  VK_3
  {5, 16}, //  VK_4
  {5, 32}, //  VK_5
  {5, 64}, //  VK_6
  {5, 128}, //  VK_7
  {6, 1}, //  VK_8
  {6, 2}, //  VK_9
  {5, 1}, //  VK_KP_0
  {5, 2}, //  VK_KP_1
  {5, 4}, //  VK_KP_2
  {5, 8}, //  VK_KP_3
  {5, 16}, //  VK_KP_4
  {5, 32}, //  VK_KP_5
  {5, 64}, //  VK_KP_6
  {5, 128}, //  VK_KP_7
  {6, 1}, //  VK_KP_8
  {6, 2}, //  VK_KP_9
  {1, 2}, //  VK_a
  {1, 4}, //  VK_b
  {1, 8}, //  VK_c
  {1, 16}, //  VK_d
  {1, 32}, //  VK_e
  {1, 64}, //  VK_f
  {1, 128}, //  VK_g
  {2, 1}, //  VK_h
  {2, 2}, //  VK_i
  {2, 4}, //  VK_j
  {2, 8}, //  VK_k
  {2, 16}, //  VK_l
  {2, 32}, //  VK_m
  {2, 64}, //  VK_n
  {2, 128}, //  VK_o
  {3, 1}, //  VK_p
  {3, 2}, //  VK_q
  {3, 4}, //  VK_r
  {3, 8}, //  VK_s
  {3, 16}, //  VK_t
  {3, 32}, //  VK_u
  {3, 64}, //  VK_v
  {3, 128}, //  VK_w
  {4, 1}, //  VK_x
  {4, 2}, //  VK_y
  {4, 4}, //  VK_z
  {1, 2}, //  VK_A
  {1, 4}, //  VK_B
  {1, 8}, //  VK_C
  {1, 16}, //  VK_D
  {1, 32}, //  VK_E
  {1, 64}, //  VK_F
  {1, 128}, //  VK_G
  {2, 1}, //  VK_H
  {2, 2}, //  VK_I
  {2, 4}, //  VK_J
  {2, 8}, //  VK_K
  {2, 16}, //  VK_L
  {2, 32}, //  VK_M
  {2, 64}, //  VK_N
  {2, 128}, //  VK_O
  {3, 1}, //  VK_P
  {3, 2}, //  VK_Q
  {3, 4}, //  VK_R
  {3, 8}, //  VK_S
  {3, 16}, //  VK_T
  {3, 32}, //  VK_U
  {3, 64}, //  VK_V
  {3, 128}, //  VK_W
  {4, 1}, //  VK_X
  {4, 2}, //  VK_Y
  {4, 4}, //  VK_Z
  {0, 0}, //  VK_GRAVEACCENT
  {0, 0}, //  VK_ACUTEACCENT
  {5, ADD_SHIFT_KEY | 128}, //  VK_QUOTE
  {5, 4}, //  VK_QUOTEDBL
  {6, ADD_SHIFT_KEY | 32}, //  VK_EQUALS
  {6, 32}, //  VK_MINUS
  {6, REMOVE_SHIFT_KEY | 32}, //  VK_KP_MINUS
  {6, 8}, //  VK_PLUS
  {6, ADD_SHIFT_KEY | 8}, //  VK_KP_PLUS
  {6, ADD_SHIFT_KEY | 4}, //  VK_KP_MULTIPLY
  {6, 4}, //  VK_ASTERISK
  {7, 2}, //  VK_BACKSLASH
  {6, REMOVE_SHIFT_KEY | 128}, //  VK_KP_DIVIDE
  {6, 128}, //  VK_SLASH
  {6, REMOVE_SHIFT_KEY | 64}, //  VK_KP_PERIOD
  {6, 64}, //  VK_PERIOD
  {6, REMOVE_SHIFT_KEY | 4}, //  VK_COLON
  {6, 16}, //  VK_COMMA
  {6, 8}, //  VK_SEMICOLON
  {5, 64}, //  VK_AMPERSAND
  {0, 0}, //  VK_VERTICALBAR
  {5, ADD_SHIFT_KEY | 8}, //  VK_HASH
  {1, REMOVE_SHIFT_KEY | 1}, //  VK_AT
  {0, 0}, //  VK_CARET
  {5, 16}, //  VK_DOLLAR
  {5, 8}, //  VK_POUND
  {0, 0}, //  VK_EURO
  {5, 32}, //  VK_PERCENT
  {5, 2}, //  VK_EXCLAIM
  {6, 128}, //  VK_QUESTION
  {0, 0}, //  VK_LEFTBRACE
  {0, 0}, //  VK_RIGHTBRACE
  {0, 0}, //  VK_LEFTBRACKET
  {0, 0}, //  VK_RIGHTBRACKET
  {6, 1}, //  VK_LEFTPAREN
  {6, 2}, //  VK_RIGHTPAREN
  {6, 16}, //  VK_LESS
  {6, 64}, //  VK_GREATER
  {0, 0}, //  VK_UNDERSCORE
  {0, 0}, //  VK_DEGREE
  {0, 0}, //  VK_SECTION
  {0, 0}, //  VK_TILDE
  {0, 0}, //  VK_NEGATION
  {8, 1}, //  VK_LSHIFT
  {8, 2}, //  VK_RSHIFT
  {0, 0}, //  VK_LALT
  {0, 0}, //  VK_RALT
  {8, 4}, //  VK_LCTRL
  {8, 4}, //  VK_RCTRL
  {0, 0}, //  VK_LGUI
  {0, 0}, //  VK_RGUI
  {7, 4}, //  VK_ESCAPE
  {0, 0}, //  VK_PRINTSCREEN
  {0, 0}, //  VK_SYSREQ
  {0, 0}, //  VK_INSERT
  {0, 0}, //  VK_KP_INSERT
  {0, 0}, //  VK_DELETE
  {0, 0}, //  VK_KP_DELETE
  {7, 32}, //  VK_BACKSPACE
  {7, 2}, //  VK_HOME
  {0, 0}, //  VK_KP_HOME
  {0, 0}, //  VK_END
  {0, 0}, //  VK_KP_END
  {0, 0}, //  VK_PAUSE
  {7, 4}, //  VK_BREAK
  {0, 0}, //  VK_SCROLLLOCK
  {0, 0}, //  VK_NUMLOCK
  {8, 8}, //  VK_CAPSLOCK
  {0, 0}, //  VK_TAB
  {7, 1}, //  VK_RETURN
  {7, 1}, //  VK_KP_ENTER
  {0, 0}, //  VK_APPLICATION
  {0, 0}, //  VK_PAGEUP
  {0, 0}, //  VK_KP_PAGEUP
  {0, 0}, //  VK_PAGEDOWN
  {0, 0}, //  VK_KP_PAGEDOWN
  {7, 8}, //  VK_UP
  {7, 8}, //  VK_KP_UP
  {7, 16}, //  VK_DOWN
  {7, 16}, //  VK_KP_DOWN
  {7, 32}, //  VK_LEFT
  {7, 32}, //  VK_KP_LEFT
  {7, 64}, //  VK_RIGHT
  {7, 64}, //  VK_KP_RIGHT
  {0, 0}, //  VK_KP_CENTER
  {8, 16}, //  VK_F1
  {8, 32}, //  VK_F2
  {8, 64}, //  VK_F3
  {5, ADD_SHIFT_KEY | 1}, //  VK_F4
  {0, 0}, //  VK_F5
  {0, 0}, //  VK_F6
  {0, 0}, //  VK_F7
  {0, 0}, //  VK_F8
  {0, 0}, //  VK_F9
  {0, 0}, //  VK_F10
  {0, 0}, //  VK_F11
  {0, 0}, //  VK_F12
  {0, 0}, //  VK_GRAVE_a
  {0, 0}, //  VK_GRAVE_e
  {0, 0}, //  VK_ACUTE_e
  {0, 0}, //  VK_GRAVE_i
  {0, 0}, //  VK_GRAVE_o
  {0, 0}, //  VK_GRAVE_u
  {0, 0}, //  VK_CEDILLA_c
  {0, 0}, //  VK_ESZETT
  {0, 0}, //  VK_UMLAUT_u
  {0, 0}, //  VK_UMLAUT_o
  {0, 0}, //  VK_UMLAUT_a
  {0, 0}, //  VK_CEDILLA_C
  {0, 0}, //  VK_TILDE_n
  {0, 0}, //  VK_TILDE_N
  {0, 0}, //  VK_UPPER_a
  {0, 0}, //  VK_ACUTE_a
  {0, 0}, //  VK_ACUTE_i
  {0, 0}, //  VK_ACUTE_o
  {0, 0}, //  VK_ACUTE_u
  {0, 0}, //  VK_UMLAUT_i
  {0, 0}, //  VK_EXCLAIM_INV
  {0, 0}, //  VK_QUESTION_INV
  {0, 0}, //  VK_ACUTE_A
  {0, 0}, //  VK_ACUTE_E
  {0, 0}, //  VK_ACUTE_I
  {0, 0}, //  VK_ACUTE_O
  {0, 0}, //  VK_ACUTE_U
  {0, 0}, //  VK_GRAVE_A
  {0, 0}, //  VK_GRAVE_E
  {0, 0}, //  VK_GRAVE_I
  {0, 0}, //  VK_GRAVE_O
  {0, 0}, //  VK_GRAVE_U
  {0, 0}, //  VK_INTERPUNCT
  {0, 0}, //  VK_DIAERESIS
  {0, 0}, //  VK_UMLAUT_e
  {0, 0}, //  VK_UMLAUT_A
  {0, 0}, //  VK_UMLAUT_E
  {0, 0}, //  VK_UMLAUT_I
  {0, 0}, //  VK_UMLAUT_O
  {0, 0}, //  VK_UMLAUT_U
  {0, 0}, //  VK_CARET_a
  {0, 0}, //  VK_CARET_e
  {0, 0}, //  VK_CARET_i
  {0, 0}, //  VK_CARET_o
  {0, 0}, //  VK_CARET_u
  {0, 0}, //  VK_CARET_A
  {0, 0}, //  VK_CARET_E
  {0, 0}, //  VK_CARET_I
  {0, 0}, //  VK_CARET_O
  {0, 0}, //  VK_CARET_U
  {0, 0}, //  VK_ASCII
  {0, 0}  //  VK_LAST
};

static uint8_t keyb_buffer[8] = {0};

bool is_m1()
{
  return false;
}

static void process_key(int vk, bool down)
{
  static uint8_t shiftMask = 0;

  if (is_m1() && vk == fabgl::VK_RSHIFT) {
    // For the M1, treat RSHIFT like LSHIFT
    vk = fabgl::VK_LSHIFT;
  }
  if (vk == fabgl::VK_LSHIFT) {
    if (down) {
      shiftMask |= 1;
    } else {
      shiftMask &= ~1;
    }
  }
  if (vk == fabgl::VK_RSHIFT) {
    if (down) {
      shiftMask |= 2;
    } else {
      shiftMask &= ~2;
    }
  }
  
  int offset = trsKeys[vk].offset;

  if (offset != 0) {
    bool addShiftKey = trsKeys[vk].mask & ADD_SHIFT_KEY;
    bool removeShiftKey = trsKeys[vk].mask & REMOVE_SHIFT_KEY;
    uint8_t mask = trsKeys[vk].mask & 0xff;
    if (down) {
      keyb_buffer[offset - 1] |= mask;
      if (addShiftKey) {
        keyb_buffer[7] |= 1;
      }
      if (removeShiftKey) {
        keyb_buffer[7] &= ~3;
      }
    } else {
      keyb_buffer[offset - 1] &= ~mask;
      if (addShiftKey || removeShiftKey) {
        // On key up, restore original shift mask
        keyb_buffer[7] &= ~3;
        keyb_buffer[7] |= shiftMask;
      }
    }
    if (addShiftKey || removeShiftKey) {
      spi_send_keyb(7, keyb_buffer[7]);
    }
    spi_send_keyb(offset - 1, keyb_buffer[offset - 1]);
  }
}

#define KEY_ALT  1
#define KEY_CTRL 2
#define KEY_DEL  4

void check_keyb()
{
  static uint8_t reset_trigger = 0;
  auto keyboard = PS2Controller.keyboard();

  if (keyboard == nullptr || !keyboard->isKeyboardAvailable()) {
    return;
  }

  if (reset_trigger == (KEY_CTRL | KEY_ALT | KEY_DEL)) {
    spi_ptrs_rst();
  }

  if (keyboard->virtualKeyAvailable()) {
    bool down;
    auto vk = keyboard->getNextVirtualKey(&down);

    // Check for PocketTRS configure
    if (down && vk == fabgl::VK_F5) {
      uint8_t mode = spi_get_config();
      // Only enter ptrs config if not in double wide or hires mode
      if (!(mode & (PTRS_CONFIG_HIRES | PTRS_CONFIG_WIDE))) {
        bool is_80_cols = mode & PTRS_CONFIG_80_COLS;
        configure_pocket_trs(is_80_cols);
      }
    }

    // Check for CTRL-ALT-DEL
    if (vk == fabgl::VK_LALT || vk == fabgl::VK_RALT) {
      if (down) {
        reset_trigger |= KEY_ALT;
      } else {
        reset_trigger &= ~KEY_ALT;
      }
    }
    if (vk == fabgl::VK_LCTRL || vk == fabgl::VK_RCTRL) {
      if (down) {
        reset_trigger |= KEY_CTRL;
      } else {
        reset_trigger &= ~KEY_CTRL;
      }
    }
    if (vk == fabgl::VK_DELETE || vk == fabgl::VK_KP_DELETE) {
      if (down) {
        reset_trigger |= KEY_DEL;
      } else {
        reset_trigger &= ~KEY_DEL;
      }
    }

    //printf("VirtualKey = %s\n", keyboard->virtualKeyToString(vk));
#if 0
    if (down && vk == fabgl::VK_F5 && trs_screen.isTextMode()) {
      configure_pocket_trs();
    } else if (down && vk == fabgl::VK_F9) {
      z80_reset();
    } else if (down && vk == fabgl::VK_F6) {
      trs_screen.screenshot();
    } else {
#endif
      process_key(vk, down);
#if 0
    }
#endif
  }
}

void set_keyb_layout()
{
  uint8_t layout = settings_get_keyb_layout();
  if (PS2Controller.keyboard() != nullptr) {
    if (!PS2Controller.keyboard()->isKeyboardAvailable() ) {
      ESP_LOGE("KEYB", "No PS/2 keyboard available");
    } else {
  	  PS2Controller.keyboard()->setLayout(SupportedLayouts::layouts()[layout]);
      ESP_LOGI("KEYB", "Keyboard layout %s", SupportedLayouts::names()[layout]);
    }
  }
}


// https://www.esp32.com/viewtopic.php?t=25626
#include "soc/rtc_cntl_struct.h"
#include "hal/wdt_types.h"
#include "hal/rwdt_ll.h"

static void __attribute__((noreturn)) esp_rtc_reset(void)
{
    rwdt_ll_write_protect_disable(&RTCCNTL);
    rwdt_ll_disable(&RTCCNTL);
    rwdt_ll_config_stage(&RTCCNTL, WDT_STAGE0, 0, WDT_STAGE_ACTION_RESET_RTC);
    rwdt_ll_disable_stage(&RTCCNTL, WDT_STAGE1);
    rwdt_ll_disable_stage(&RTCCNTL, WDT_STAGE2);
    rwdt_ll_disable_stage(&RTCCNTL, WDT_STAGE3);
    rwdt_ll_set_sys_reset_length(&RTCCNTL, WDT_RESET_SIG_LENGTH_3_2us);
    rwdt_ll_feed(&RTCCNTL);
    rwdt_ll_enable(&RTCCNTL);
    rwdt_ll_write_protect_enable(&RTCCNTL);
    for(;;);
}

void reset_keyb()
{
  PS2Controller.end();
  esp_rtc_reset();
}

void init_keyb()
{
  PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
  set_keyb_layout();
}
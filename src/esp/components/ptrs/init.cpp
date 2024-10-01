

#include <trs-lib.h>
#include "spi.h"
#include "ptrs.h"
#include "fabgl.h"
#include <esp_log.h>


bool is_m3()
{
  static bool init = false;
  static bool is_m1;

  if (!init) {
    int conf = spi_get_config() & PTRS_CONFIG_MODEL_MASK;
    is_m1 = conf == PTRS_CONFIG_MODEL_1;
    init = true;
  }
  return !is_m1;
}

void panic(uint8_t err)
{
  ESP_LOGE("TRS-LIB", "err: %d", err);
}

extern fabgl::PS2Controller PS2Controller;

static char get_next_key()
{
  auto keyboard = PS2Controller.keyboard();

  while (true) {
    vTaskDelay(portTICK_PERIOD_MS);
    if (!keyboard->virtualKeyAvailable()) {
      continue;
    }
  
    bool down;
    auto vk = keyboard->getNextVirtualKey(&down);
    if (!down) {
      continue;
    }
    switch(vk) {
    case fabgl::VK_ESCAPE:
      return KEY_BREAK;
    case fabgl::VK_UP:
    case fabgl::VK_KP_UP:
      return KEY_UP;
    case fabgl::VK_DOWN:
    case fabgl::VK_KP_DOWN:
      return KEY_DOWN;
    case fabgl::VK_RIGHT:
    case fabgl::VK_KP_RIGHT:
      return KEY_RIGHT;
    case fabgl::VK_LEFT:
    case fabgl::VK_KP_LEFT:
      return KEY_LEFT;
    default:
      int c = keyboard->virtualKeyToASCII(vk);
      if (c > -1) {
        return c;
      }
      break;
    }
  }
}

void fpga_screen_update_range(uint8_t* from, uint8_t* to)
{
  int ofs = from - screen.current;
  spi_z80_dsp_set_addr(ofs);
  while (from != to) {
    spi_z80_dsp_poke(*from++);
  }
}


static uint8_t* screen_original_content;
static uint8_t* foreground_buffer;
static uint8_t* background_buffer;

static uint8_t screen_width;
static uint8_t screen_height;

void init_trs_lib(bool is_80_cols)
{
  uint8_t ch;

  screen_width = is_80_cols ? 80 : 64;
  screen_height = is_80_cols ? 24 : 16;

  screen_original_content = (uint8_t*) malloc(screen_width * screen_height);
  foreground_buffer = (uint8_t*) malloc(screen_width * screen_height);
  background_buffer = (uint8_t*) malloc(screen_width * screen_height);

  spi_z80_pause();
  spi_z80_dsp_set_addr(0);
  for (int i = 0; i < screen_width * screen_height; i++) {
    screen_original_content[i] = foreground_buffer[i] = spi_z80_dsp_peek();
  }

  set_screen(foreground_buffer, background_buffer,
             screen_width, screen_height,
             fpga_screen_update_range);
  set_keyboard_callback(get_next_key);
}

void exit_trs_lib()
{
  set_screen_to_background();
  memmove(screen.current, screen_original_content, screen_width * screen_height);
  screen_show(true);
  spi_z80_resume();
  free(screen_original_content);
  free(foreground_buffer);
  free(background_buffer);
}

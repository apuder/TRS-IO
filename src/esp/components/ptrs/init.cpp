

#include <trs-lib.h>
#include "spi.h"
#include "fabgl.h"
#include <esp_log.h>


bool is_m3()
{
  return true;
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


static uint8_t screen_original_content[SCREEN_WIDTH * SCREEN_HEIGHT];


void init_trs_lib()
{
  uint8_t ch;

  // Hardcoded for M1/MIII
  static uint8_t foreground_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
  static uint8_t background_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

  spi_z80_pause();
  spi_z80_dsp_set_addr(0);
  for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
    screen_original_content[i] = foreground_buffer[i] = spi_z80_dsp_peek();
  }

  set_screen(foreground_buffer, background_buffer,
             SCREEN_WIDTH, SCREEN_HEIGHT,
             fpga_screen_update_range);
  set_keyboard_callback(get_next_key);
}

void exit_trs_lib()
{
  set_screen_to_background();
  memmove(screen.current, screen_original_content, SCREEN_WIDTH * SCREEN_HEIGHT);
  screen_show(true);
  spi_z80_resume();
}

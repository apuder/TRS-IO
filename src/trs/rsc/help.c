
#include "retrostore.h"
#include "trs-lib.h"

static window_t wnd;

void help()
{
  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  header("Help");
  wnd_print(&wnd, "\nVisit https://github.com/apuder/TRS-IO for "
            "complete online documentation.");
  screen_show(false);
  get_key();
}

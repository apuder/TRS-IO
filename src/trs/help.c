
#include "retrostore.h"

static window_t wnd;

void help()
{
  init_window(&wnd, 0, 3, 0, 0);
  header(&wnd, "Help");
  wnd_print(&wnd, false, "\nVisit https://github.com/apuder/TRS-IO for "
            "complete online documentation.");
  wnd_show(&wnd, false);
  get_key();
}

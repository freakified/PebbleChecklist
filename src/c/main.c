#include "messaging.h"
#include "settings.h"
#include "windows/checklist_window.h"
#include <pebble.h>

static Window *s_main_window;

static void init() {
  settings_init();
#ifdef PBL_TOUCH
  app_touch_navigation_enable(true);
#endif
  checklist_window_push();
  messaging_init(checklist_window_refresh);
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}

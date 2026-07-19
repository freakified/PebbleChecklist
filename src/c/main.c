#include <pebble.h>
#include "windows/checklist_window.h"
#include "messaging.h"
#include "settings.h"

static Window *s_main_window;

static void init() {
  settings_init();
  checklist_window_push();
  messaging_init(checklist_window_refresh);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

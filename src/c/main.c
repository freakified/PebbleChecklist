#include <pebble.h>
#include "windows/checklist_window.h"
#include "messaging.h"

static Window *s_main_window;

static void init() {
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

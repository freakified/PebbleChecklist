#include <pebble.h>
#include "windows/checklist_window.h"

static Window *s_main_window;

static void init() {
  checklist_window_push();
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}

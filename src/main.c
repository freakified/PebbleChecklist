#include <pebble.h>
#include "windows/checkbox_window.h"

static Window *s_main_window;

static void init() {
  checkbox_window_push();
}

static void deinit() {
  // window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}

#pragma once

#include <pebble.h>

#define CHECKBOX_WINDOW_MAX_ITEMS   25
#define CHECKBOX_WINDOW_BOX_SIZE    12

#define PERSIST_KEY_CHECKLIST_ITEMS        100
#define PERSIST_KEY_CHECKLIST_SELECTIONS   101
#define PERSIST_KEY_CHECKLIST_LENGTH       102
#define PERSIST_KEY_NUM_CHECKED            103

void checkbox_window_push();

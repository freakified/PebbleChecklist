#include "settings.h"

// persistent storage keys
#define PERSIST_KEY_SETTINGS_BITFIELD 51

// bitfield flags (shared with the config page)
#define SETTINGS_FLAG_SHOW_VOICE_BUTTON      (1 << 0)
#define SETTINGS_FLAG_MOVE_CHECKED_TO_BOTTOM (1 << 1)
#define SETTINGS_FLAG_WRAP_AROUND_SCROLLING  (1 << 2)
#define SETTINGS_FLAG_USE_LARGER_FONT        (1 << 3)
#define SETTINGS_FLAG_QUICK_LAUNCH_VOICE     (1 << 4)

static ChecklistSettings s_settings;

void settings_init() {
  // defaults for a fresh install
  s_settings.show_voice_button = true;
  s_settings.move_checked_to_bottom = true;
  s_settings.wrap_around_scrolling = true;

  if (persist_exists(PERSIST_KEY_SETTINGS_BITFIELD)) {
    settings_apply_bitfield(persist_read_int(PERSIST_KEY_SETTINGS_BITFIELD));
  }
}

void settings_save() {
  persist_write_int(PERSIST_KEY_SETTINGS_BITFIELD, settings_to_bitfield());
}

ChecklistSettings *settings_get() {
  return &s_settings;
}

int32_t settings_to_bitfield() {
  int32_t bitfield = 0;

  if (s_settings.show_voice_button) {
    bitfield |= SETTINGS_FLAG_SHOW_VOICE_BUTTON;
  }

  if (s_settings.move_checked_to_bottom) {
    bitfield |= SETTINGS_FLAG_MOVE_CHECKED_TO_BOTTOM;
  }

  if (s_settings.wrap_around_scrolling) {
    bitfield |= SETTINGS_FLAG_WRAP_AROUND_SCROLLING;
  }

  if (s_settings.use_larger_font) {
    bitfield |= SETTINGS_FLAG_USE_LARGER_FONT;
  }

  if (s_settings.quick_launch_voice) {
    bitfield |= SETTINGS_FLAG_QUICK_LAUNCH_VOICE;
  }

  return bitfield;
}

void settings_apply_bitfield(int32_t bitfield) {
  s_settings.show_voice_button = (bitfield & SETTINGS_FLAG_SHOW_VOICE_BUTTON) != 0;
  s_settings.move_checked_to_bottom = (bitfield & SETTINGS_FLAG_MOVE_CHECKED_TO_BOTTOM) != 0;
  s_settings.wrap_around_scrolling = (bitfield & SETTINGS_FLAG_WRAP_AROUND_SCROLLING) != 0;
  s_settings.use_larger_font = (bitfield & SETTINGS_FLAG_USE_LARGER_FONT) != 0;
  s_settings.quick_launch_voice = (bitfield & SETTINGS_FLAG_QUICK_LAUNCH_VOICE) != 0;
}

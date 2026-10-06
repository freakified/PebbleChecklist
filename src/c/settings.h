#pragma once

#include <pebble.h>

typedef struct {
  // show the voice input button at the top of the list (default on)
  bool show_voice_button;

  // show checked items below unchecked ones
  bool move_checked_to_bottom;

  // scrolling past either end of the list wraps to the other end (default on)
  bool wrap_around_scrolling;

  // draw checklist items one Gothic font stage larger
  bool use_larger_font;

  // start voice input right away when launched via quick launch
  bool quick_launch_voice;
} ChecklistSettings;

extern void settings_init();
extern void settings_save();

/*
 * Returns a pointer to the current settings
 */
extern ChecklistSettings *settings_get();

/*
 * Encodes the settings as a bitfield for sending to the phone
 */
extern int32_t settings_to_bitfield();

/*
 * Applies a bitfield received from the phone to the current settings
 */
extern void settings_apply_bitfield(int32_t bitfield);

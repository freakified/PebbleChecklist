/**
 * The main checklist window, showing the the list of items with their
 * associated checkboxes.
 */

#include "checklist_window.h"
#include "../checklist.h"
#include "../settings.h"
#include "../util.h"
#include "dialog_message_window.h"

static Window *s_main_window;
static MenuLayer *s_menu_layer;
static StatusBarLayer *s_status_bar;
static TextLayer *s_empty_msg_layer;
static TextLayer *s_empty_hint_layer;

static GBitmap *s_tick_black_bitmap;
static GBitmap *s_tick_white_bitmap;
static GBitmap *s_add_bitmap_black;
static GBitmap *s_add_bitmap_white;

static GTextAttributes *s_text_att;

static DictationSession *s_dictation_session;

// start voice input once the window first appears (quick launch setting)
static bool s_start_dictation_on_appear;

// Declare a buffer for the DictationSession
static char s_last_text[512];

// buffer to hold alert message
static char s_deleted_msg[30];

#ifdef PBL_TOUCH
// the row that was highlighted when the current touch began. Touch navigation
// moves the highlight to a tapped row before reporting the tap, so this is
// the only way to tell a tap on the highlighted row from a tap elsewhere
static int s_touchdown_row = -1;
#endif

// the hint shown under "No items". Round screens get hand-placed line breaks
// so the lines narrow along with the bottom of the circle
#define EMPTY_HINT_TEXT                                                        \
  PBL_IF_ROUND_ELSE(                                                           \
      "Add items via the app\nconfig page in the Pebble\napp on your phone",   \
      "Add items via the app config page in the Pebble app on your phone")
// blank space the Gothic fonts leave above their text
#define EMPTY_TEXT_TOP_SPACE 4

static const char *const CLEAR_COMPLETED_TEXT = "Clear completed";

// when "move checked to bottom" is enabled, how long a toggled item and the
// items it passes take to slide into their new positions
#define MOVE_ANIMATION_DURATION_MS 300

// MenuLayer can't animate rows itself, so after a move each affected cell
// draws whichever items currently overlap it, at positions interpolated from
// the old layout to the new one. Cells clip their drawing and tile the
// screen, so the result looks like the rows themselves are sliding.
// The affected items are the display positions from s_move_anim_first (in
// the new layout); the per-position arrays are indexed relative to it, and
// the y positions are relative to the top of the first affected cell.
static Animation *s_move_anim;
static AnimationProgress s_move_anim_progress;
static int s_move_anim_first;
static int s_move_anim_count;
static int s_move_anim_moved; // the moved item's position in the range
static int16_t s_move_anim_heights[MAX_CHECKLIST_ITEMS];
static int16_t s_move_anim_from_y[MAX_CHECKLIST_ITEMS];
static int16_t s_move_anim_to_y[MAX_CHECKLIST_ITEMS];

// Returns the number of rows shown above the checklist items (i.e. whether or
// not the voice input button is shown)
static uint16_t get_item_row_offset() {
  return settings_get()->show_voice_button ? 1 : 0;
}

// Returns the font used for checklist item text
static GFont get_item_font() {
  return fonts_get_system_font(settings_get()->use_larger_font
                                   ? FONT_KEY_GOTHIC_28_BOLD
                                   : FONT_KEY_GOTHIC_24_BOLD);
}

// Returns how far above true center to place single-line text so its glyphs
// look vertically centered (the larger font has more padding above the glyphs)
static int16_t get_item_font_center_offset() {
  return settings_get()->use_larger_font ? 5 : 2;
}

// Maps a display position to a checklist item id. When "move checked to
// bottom" is enabled, unchecked items are shown first (in list order),
// followed by checked items (also in list order); the underlying list itself
// is never reordered.
static int get_item_id_for_display_index(int display_index) {
  if (!settings_get()->move_checked_to_bottom) {
    return display_index;
  }

  int num_items = checklist_get_num_items();
  int num_sorted_checked = 0;
  for (int i = 0; i < num_items; i++) {
    if (checklist_get_item_by_id(i)->is_checked) {
      num_sorted_checked++;
    }
  }
  int num_unchecked = num_items - num_sorted_checked;

  bool find_checked = (display_index >= num_unchecked);
  int target = find_checked ? display_index - num_unchecked : display_index;

  int count = 0;
  for (int i = 0; i < num_items; i++) {
    if (checklist_get_item_by_id(i)->is_checked == find_checked) {
      if (count == target) {
        return i;
      }
      count++;
    }
  }

  return display_index;
}

// The inverse of get_item_id_for_display_index
static int get_display_index_for_item_id(int id) {
  int num_items = checklist_get_num_items();
  for (int i = 0; i < num_items; i++) {
    if (get_item_id_for_display_index(i) == id) {
      return i;
    }
  }
  return id;
}

static int16_t get_item_cell_height(int id);

static void stop_move_animation() {
  if (s_move_anim != NULL) {
    // clear it first so the stopped handler knows it was stopped early
    Animation *anim = s_move_anim;
    s_move_anim = NULL;
    animation_unschedule(anim);
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
}

static void move_anim_update(Animation *animation,
                             const AnimationProgress progress) {
  s_move_anim_progress = progress;
  layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
}

static void move_anim_stopped(Animation *animation, bool finished,
                              void *context) {
  if (animation == s_move_anim) {
    s_move_anim = NULL;
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
}

static const AnimationImplementation s_move_anim_impl = {
    .update = move_anim_update,
};

// Slides the items between two display positions from the layout where the
// moved item was at old_index to the (current) one where it's at new_index
static void start_move_animation(int old_index, int new_index) {
  stop_move_animation();

  bool down = new_index > old_index;
  int count = down ? new_index - old_index + 1 : old_index - new_index + 1;
  s_move_anim_first = down ? old_index : new_index;
  s_move_anim_count = count;
  s_move_anim_moved = down ? count - 1 : 0;

  int16_t y = 0;
  for (int i = 0; i < count; i++) {
    s_move_anim_heights[i] = get_item_cell_height(
        get_item_id_for_display_index(s_move_anim_first + i));
    s_move_anim_to_y[i] = y;
    y += s_move_anim_heights[i];
  }

  // in the old layout, the moved item was at the other end of the range and
  // the rest were shifted one position towards it
  y = 0;
  if (down) {
    s_move_anim_from_y[count - 1] = 0;
    y = s_move_anim_heights[count - 1];
    for (int i = 0; i < count - 1; i++) {
      s_move_anim_from_y[i] = y;
      y += s_move_anim_heights[i];
    }
  } else {
    for (int i = 1; i < count; i++) {
      s_move_anim_from_y[i] = y;
      y += s_move_anim_heights[i];
    }
    s_move_anim_from_y[0] = y;
  }

  s_move_anim_progress = 0;
  s_move_anim = animation_create();
  animation_set_implementation(s_move_anim, &s_move_anim_impl);
  animation_set_duration(s_move_anim, MOVE_ANIMATION_DURATION_MS);
  animation_set_curve(s_move_anim, AnimationCurveEaseInOut);
  animation_set_handlers(s_move_anim,
                         (AnimationHandlers){.stopped = move_anim_stopped},
                         NULL);
  animation_schedule(s_move_anim);
}

static void update_empty_msg_layer() {
  if (s_empty_msg_layer == NULL) {
    return;
  }

  Layer *msg_layer = text_layer_get_layer(s_empty_msg_layer);
  Layer *hint_layer = text_layer_get_layer(s_empty_hint_layer);
  bool hidden = (checklist_get_num_items() != 0);
  layer_set_hidden(msg_layer, hidden);
  layer_set_hidden(hint_layer, hidden);

  GRect bounds = layer_get_bounds(window_get_root_layer(s_main_window));
  int16_t h = bounds.size.h;

  // the text never goes above the voice button (when shown) or the status bar
  int16_t min_y;
  if (settings_get()->show_voice_button) {
    min_y = PBL_IF_ROUND_ELSE(h / 2 + CHECKLIST_CELL_MIN_HEIGHT / 2,
                              STATUS_BAR_LAYER_HEIGHT + CHECKLIST_CELL_MIN_HEIGHT);
  } else {
    min_y = PBL_IF_ROUND_ELSE(0, STATUS_BAR_LAYER_HEIGHT);
  }

  // center the message and hint together in the space below the voice button
  // when it's shown, or on the whole screen otherwise
  int16_t area_top = settings_get()->show_voice_button ? min_y : 0;

  int16_t msg_h = text_layer_get_content_size(s_empty_msg_layer).h;
  int16_t hint_h = text_layer_get_content_size(s_empty_hint_layer).h;

  // drop the hint where there isn't room for it (e.g. under the voice
  // button on round watches)
  if (min_y + msg_h + hint_h > h) {
    layer_set_hidden(hint_layer, true);
    hint_h = 0;
  }

  // the font's line height leaves blank space above the text, so shift the
  // block up a little to center what's actually drawn
  int16_t y = area_top + (h - area_top - msg_h - hint_h) / 2 -
              EMPTY_TEXT_TOP_SPACE;
  if (y < min_y) {
    y = min_y;
  }

  GRect frame = layer_get_frame(msg_layer);
  frame.origin.y = y;
  layer_set_frame(msg_layer, frame);

  frame = layer_get_frame(hint_layer);
  frame.origin.y = y + msg_h;
  layer_set_frame(hint_layer, frame);
}

// Draws a plain single-line label cell (e.g. "Clear completed") in the same
// font as the checklist items
static void draw_label_cell(GContext *ctx, Layer *cell_layer,
                            const char *text) {
  if (!settings_get()->use_larger_font) {
    menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
    return;
  }

  GRect bounds = layer_get_bounds(cell_layer);
  GFont font = get_item_font();

  graphics_context_set_text_color(ctx,
                                  menu_cell_layer_is_highlighted(cell_layer)
                                      ? GColorWhite
                                      : GColorBlack);

  GRect text_bounds =
      GRect(CHECKLIST_CELL_MARGIN, 0,
            bounds.size.w - CHECKLIST_CELL_MARGIN * 2, bounds.size.h);

  // center the text vertically (it may wrap onto multiple lines)
  GSize text_size = graphics_text_layout_get_content_size(
      text, font, text_bounds, GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  text_bounds.origin.y =
      (bounds.size.h - text_size.h) / 2 - get_item_font_center_offset();
  text_bounds.size.h = text_size.h;

  graphics_draw_text(ctx, text, font, text_bounds,
                     GTextOverflowModeTrailingEllipsis,
                     PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft),
                     s_text_att);
}

static void draw_add_button(GContext *ctx, Layer *cell_layer) {
  GRect bounds = layer_get_bounds(cell_layer);
  GRect bitmap_bounds = gbitmap_get_bounds(s_add_bitmap_black);

  GPoint pos;
  pos.x = (bounds.size.w / 2) - (bitmap_bounds.size.w / 2);
  pos.y = (bounds.size.h / 2) - (bitmap_bounds.size.h / 2);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  GBitmap *imageToUse = s_tick_black_bitmap;

  if (menu_cell_layer_is_highlighted(cell_layer)) {
    imageToUse = s_add_bitmap_white;
  } else {
    imageToUse = s_add_bitmap_black;
  }

  graphics_draw_bitmap_in_rect(
      ctx, imageToUse,
      GRect(pos.x, pos.y, bitmap_bounds.size.w, bitmap_bounds.size.h));
}

static void dictation_session_callback(DictationSession *session,
                                       DictationSessionStatus status,
                                       char *transcription, void *context) {

  // Print the results of a transcription attempt
  APP_LOG(APP_LOG_LEVEL_INFO, "Dictation status: %d", (int)status);

  if (status == DictationSessionStatusSuccess) {
    stop_move_animation();
    checklist_add_items(transcription);
    menu_layer_reload_data(s_menu_layer);
    update_empty_msg_layer();
  }
}

static uint16_t get_num_rows_callback(MenuLayer *menu_layer,
                                      uint16_t section_index, void *context) {
  uint16_t num_rows = checklist_get_num_items() + get_item_row_offset();

  if (checklist_get_num_items_checked() > 0) {
    num_rows++;
  }

  return num_rows;
}

// Draws an item into the given rect of a cell (normally the whole cell, but
// the move animation draws items partway into neighboring cells)
static void draw_checkbox_cell(GContext *ctx, Layer *cell_layer, int id,
                               GRect bounds) {
  ChecklistItem *item = checklist_get_item_by_id(id);
  int16_t top = bounds.origin.y;

  GFont font = get_item_font();
  GRect text_bounds;
  GTextAlignment alignment = GTextAlignmentLeft;

  if (item->is_checked) {
    if (menu_cell_layer_is_highlighted(cell_layer)) {
      graphics_context_set_text_color(ctx, GColorLimerick);
    } else {
      graphics_context_set_text_color(ctx, GColorArmyGreen);
    }
  } else {
    if (menu_cell_layer_is_highlighted(cell_layer)) {
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, GColorBlack);
    }
  }

  // for single-height cells with the standard font, use a standard draw
  // command; the larger font needs a custom draw at any height
  if (bounds.size.h == CHECKLIST_CELL_MIN_HEIGHT &&
      !settings_get()->use_larger_font) {
    // menu_cell_basic_draw draws into the layer's bounds, so point them at
    // the item's rect for the duration of the call
    GRect cell_bounds = layer_get_bounds(cell_layer);
    layer_set_bounds(cell_layer, bounds);
    menu_cell_basic_draw(ctx, cell_layer, item->name, NULL, NULL);
    layer_set_bounds(cell_layer, cell_bounds);
  } else {
// on round watches, single line cells should always be center aligned with no
// margin, (since anything else looks bad)
#ifdef PBL_ROUND
    text_bounds =
        GRect(CHECKLIST_CELL_MARGIN, top,
              bounds.size.w - CHECKLIST_WINDOW_BOX_SIZE * 4, bounds.size.h);
#else
    text_bounds = GRect(CHECKLIST_CELL_MARGIN, top,
                        bounds.size.w - CHECKLIST_CELL_MARGIN * 2 -
                            CHECKLIST_WINDOW_BOX_SIZE * 2,
                        bounds.size.h);
#endif

    if (bounds.size.h == CHECKLIST_CELL_MIN_HEIGHT) {
      // a single line in a fixed-height cell: center it vertically, and match
      // the alignment menu_cell_basic_draw would have used
      GSize text_size = graphics_text_layout_get_content_size(
          item->name, font, text_bounds, GTextOverflowModeTrailingEllipsis,
          alignment);
      text_bounds.origin.y = top + (bounds.size.h - text_size.h) / 2 -
                             get_item_font_center_offset();
      alignment = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, alignment);
    }

    graphics_draw_text(ctx, item->name, font, text_bounds,
                       GTextOverflowModeTrailingEllipsis, alignment,
                       s_text_att);
  }

  if (menu_cell_layer_is_highlighted(cell_layer)) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
  }

  GRect bitmap_bounds = gbitmap_get_bounds(s_tick_black_bitmap);

  GBitmap *imageToUse = s_tick_black_bitmap;

  if (menu_cell_layer_is_highlighted(cell_layer)) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    imageToUse = s_tick_white_bitmap;
  }

  // on round watches, only show the checkbox for the selected item
  bool show_checkbox = true;

#ifdef PBL_ROUND
  show_checkbox = menu_cell_layer_is_highlighted(cell_layer);
#endif

  if (show_checkbox) {
    GRect r = GRect(bounds.size.w - (2 * CHECKLIST_WINDOW_BOX_SIZE),
                    top + (bounds.size.h / 2) - (CHECKLIST_WINDOW_BOX_SIZE / 2),
                    CHECKLIST_WINDOW_BOX_SIZE, CHECKLIST_WINDOW_BOX_SIZE);

    graphics_draw_rect(ctx, r);

    if (item->is_checked) {
      // draw the checkmark
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, imageToUse,
                                   GRect(r.origin.x, r.origin.y - 3,
                                         bitmap_bounds.size.w,
                                         bitmap_bounds.size.h));
    }
  }

  // draw text strikethrough
  if (item->is_checked) {
    graphics_context_set_stroke_width(ctx, 2);

    GPoint strike_start_point, strike_end_point;

    strike_start_point.y = top + bounds.size.h / 2;
    strike_end_point.y = top + bounds.size.h / 2;

    // for single-height cells, draw a true strikethrough
    if (bounds.size.h == CHECKLIST_CELL_MIN_HEIGHT) {
      GSize text_size = graphics_text_layout_get_content_size(
          item->name, font, bounds, GTextOverflowModeTrailingEllipsis,
          PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));

// draw centered for round, left-aligned for rect
#ifdef PBL_ROUND
      strike_start_point.x = (bounds.size.w / 2) - (text_size.w / 2);
      strike_end_point.x = (bounds.size.w / 2) + (text_size.w / 2);
#else
      strike_start_point.x = CHECKLIST_CELL_MARGIN;
      strike_end_point.x = CHECKLIST_CELL_MARGIN + text_size.w;
#endif

    } else {
      // otherwise, draw a full-width line

#ifdef PBL_ROUND
      strike_start_point.x = text_bounds.origin.x + CHECKLIST_CELL_MARGIN;
      strike_end_point.x =
          text_bounds.origin.x + text_bounds.size.w - CHECKLIST_CELL_MARGIN;
#else
      strike_start_point.x = CHECKLIST_CELL_MARGIN;
      strike_end_point.x = CHECKLIST_CELL_MARGIN + text_bounds.size.w;
#endif
    }

    graphics_draw_line(ctx, strike_start_point, strike_end_point);
  }
}

// Draws the parts of the animating items that currently overlap the cell at
// the given position in the animated range
static void draw_animating_cell(GContext *ctx, Layer *cell_layer, int pos) {
  GRect bounds = layer_get_bounds(cell_layer);
  int16_t cell_top = s_move_anim_to_y[pos];

  // draw the moved item last, over the items it passes
  for (int n = 0; n < s_move_anim_count; n++) {
    int i = (n + s_move_anim_moved + 1) % s_move_anim_count;
    int16_t from = s_move_anim_from_y[i];
    int16_t to = s_move_anim_to_y[i];
    int16_t y = from +
                (int32_t)(to - from) * s_move_anim_progress /
                    ANIMATION_NORMALIZED_MAX -
                cell_top;
    int16_t h = s_move_anim_heights[i];

    if (y >= bounds.size.h || y + h <= 0) {
      continue;
    }

    GRect rect = GRect(0, y, bounds.size.w, h);
    if (i == s_move_anim_moved) {
      graphics_context_set_fill_color(
          ctx, menu_cell_layer_is_highlighted(cell_layer) ? GColorArmyGreen
                                                          : BG_COLOR);
      graphics_fill_rect(ctx, rect, 0, GCornerNone);
    }
    draw_checkbox_cell(ctx, cell_layer,
                       get_item_id_for_display_index(s_move_anim_first + i),
                       rect);
  }
}

static void draw_row_callback(GContext *ctx, Layer *cell_layer,
                              MenuIndex *cell_index, void *context) {
  uint16_t offset = get_item_row_offset();

  if (offset > 0 && cell_index->row == 0) {
    // draw the add action
    draw_add_button(ctx, cell_layer);
  } else if (cell_index->row == checklist_get_num_items() + offset) {
    // draw the clear action
    draw_label_cell(ctx, cell_layer, CLEAR_COMPLETED_TEXT);
  } else {
    // draw the checkbox
    int display_index = cell_index->row - offset;
    int pos = display_index - s_move_anim_first;

    if (s_move_anim == NULL || pos < 0 || pos >= s_move_anim_count) {
      draw_checkbox_cell(ctx, cell_layer,
                         get_item_id_for_display_index(display_index),
                         layer_get_bounds(cell_layer));
    } else {
      draw_animating_cell(ctx, cell_layer, pos);
    }
  }
}

// Measures text in the item font and clamps to the cell height limits
static int16_t get_text_cell_height(const char *text, int width) {
  GSize size = graphics_text_layout_get_content_size_with_attributes(
      text, get_item_font(), GRect(0, 0, width, 500),
      GTextOverflowModeTrailingEllipsis,
      PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft), NULL);

  if (size.h > CHECKLIST_CELL_MAX_HEIGHT) {
    return CHECKLIST_CELL_MAX_HEIGHT;
  } else if (size.h < CHECKLIST_CELL_MIN_HEIGHT) {
    return CHECKLIST_CELL_MIN_HEIGHT;
  } else {
    return size.h + CHECKLIST_CELL_MARGIN * 2;
  }
}

// Returns the height of an item's cell (which doesn't depend on its position)
static int16_t get_item_cell_height(int id) {
  int screen_width =
      layer_get_bounds(window_get_root_layer(s_main_window)).size.w;
  int width = PBL_IF_ROUND_ELSE(screen_width - CHECKLIST_WINDOW_BOX_SIZE * 4,
                                screen_width - CHECKLIST_CELL_MARGIN * 2 -
                                    CHECKLIST_WINDOW_BOX_SIZE * 2);

  return get_text_cell_height(checklist_get_item_by_id(id)->name, width);
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer,
                                        MenuIndex *cell_index,
                                        void *callback_context) {
  uint16_t offset = get_item_row_offset();
  int screen_width =
      layer_get_bounds(window_get_root_layer(s_main_window)).size.w;

  if (offset > 0 && cell_index->row == 0) {
    return CHECKLIST_CELL_MIN_HEIGHT;
  } else if (cell_index->row == checklist_get_num_items() + offset) {
    // with the standard font, the clear row always fits a single line; the
    // larger font may need to wrap
    if (!settings_get()->use_larger_font) {
      return CHECKLIST_CELL_MIN_HEIGHT;
    }
    return get_text_cell_height(CLEAR_COMPLETED_TEXT,
                                screen_width - CHECKLIST_CELL_MARGIN * 2);
  } else {
    return get_item_cell_height(
        get_item_id_for_display_index(cell_index->row - offset));
  }
}

static void select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                            void *callback_context) {
  uint16_t offset = get_item_row_offset();

  // the select button is handled manually, so it can arrive with no rows at
  // all (empty list and no voice button), where row 0 doesn't exist
  if (cell_index->row >= get_num_rows_callback(menu_layer, 0, NULL)) {
    return;
  }

  stop_move_animation();

  if (offset > 0 && cell_index->row == 0) {
    // the first row is the "add" button (when shown)
    if (s_dictation_session != NULL) {
      dictation_session_start(s_dictation_session);
    } else {
      dialog_settings_window_push(
          "Add items via the settings page on your phone.");
    }
  } else if (cell_index->row == checklist_get_num_items() + offset) {
    // the last row is always the "clear completed" button
    int num_deleted = checklist_get_num_items_checked();

    // generate and display "items deleted" message
    snprintf(s_deleted_msg, sizeof(s_deleted_msg),
             ((num_deleted == 1) ? "%i Item Deleted" : "%i Items Deleted"),
             num_deleted);

    // do stuff
    dialog_shred_window_push(s_deleted_msg);
    checklist_delete_completed_items();
    menu_layer_reload_data(menu_layer);
    update_empty_msg_layer();

  } else {
    int display_index = cell_index->row - offset;
    int id = get_item_id_for_display_index(display_index);
    checklist_item_toggle_checked(id);
    menu_layer_reload_data(menu_layer);

    int new_display_index = get_display_index_for_item_id(id);
    if (new_display_index != display_index) {
      start_move_animation(display_index, new_display_index);
    }
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  MenuIndex index = menu_layer_get_selected_index(s_menu_layer);
  select_callback(s_menu_layer, &index, NULL);
}

#ifdef PBL_TOUCH
static void touch_handler(const TouchEvent *event, void *context) {
  if (event->type == TouchEvent_Touchdown) {
    s_touchdown_row = menu_layer_get_selected_index(s_menu_layer).row;
  }
}
#endif

// Only touch navigation reaches the MenuLayer's select callback (the select
// button goes through select_click_handler), so this is where taps are
// filtered: tapping a row that isn't highlighted just highlights it, and a
// second tap is needed to act on it, so stray taps can't check items off
static void menu_select_callback(struct MenuLayer *menu_layer,
                                 MenuIndex *cell_index,
                                 void *callback_context) {
#ifdef PBL_TOUCH
  int touchdown_row = s_touchdown_row;
  s_touchdown_row = -1;
  if (touchdown_row >= 0 && cell_index->row != touchdown_row) {
    return;
  }
#endif
  select_callback(menu_layer, cell_index, callback_context);
}

// A short, subtle pulse to signal that the selection wrapped around
static void vibe_wrap_pulse() {
  static const uint32_t segments[] = {40};
  vibes_enqueue_custom_pattern((VibePattern){
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
  });
}

static void up_down_click_handler(ClickRecognizerRef recognizer,
                                  void *context) {
  bool up = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);

  stop_move_animation();

  uint16_t num_rows = get_num_rows_callback(s_menu_layer, 0, NULL);
  MenuIndex index = menu_layer_get_selected_index(s_menu_layer);

  if (settings_get()->wrap_around_scrolling && num_rows > 0) {
    if (up && index.row == 0) {
      // wrap from the top to the bottom
      menu_layer_set_selected_index(s_menu_layer, MenuIndex(0, num_rows - 1),
                                    MenuRowAlignBottom, true);
      vibe_wrap_pulse();
      return;
    } else if (!up && index.row >= num_rows - 1) {
      // wrap from the bottom to the top
      menu_layer_set_selected_index(s_menu_layer, MenuIndex(0, 0),
                                    MenuRowAlignTop, true);
      vibe_wrap_pulse();
      return;
    }
  }

  menu_layer_set_selected_next(s_menu_layer, up, MenuRowAlignCenter, true);
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200,
                                          up_down_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200,
                                          up_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void window_load(Window *window) {
  checklist_init();

  Layer *window_layer = window_get_root_layer(window);
  GRect windowBounds = layer_get_bounds(window_layer);
  ;

#ifdef PBL_ROUND
  GRect bounds = windowBounds;
#else
  GRect bounds = GRect(0, STATUS_BAR_LAYER_HEIGHT, windowBounds.size.w,
                       windowBounds.size.h - STATUS_BAR_LAYER_HEIGHT);
#endif

  s_text_att = graphics_text_attributes_create();

#ifdef PBL_ROUND
  graphics_text_attributes_enable_screen_text_flow(s_text_att,
                                                   CHECKLIST_CELL_MARGIN * 2);
#endif

  s_tick_black_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TICK_BLACK);
  s_tick_white_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TICK_WHITE);
  s_add_bitmap_black = gbitmap_create_with_resource(RESOURCE_ID_ADD_BLACK);
  s_add_bitmap_white = gbitmap_create_with_resource(RESOURCE_ID_ADD_WHITE);

  s_menu_layer = menu_layer_create(bounds);
  window_set_click_config_provider(window, click_config_provider);
  menu_layer_set_center_focused(s_menu_layer, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(
      s_menu_layer, NULL,
      (MenuLayerCallbacks){
          .get_num_rows =
              (MenuLayerGetNumberOfRowsInSectionsCallback)get_num_rows_callback,
          .draw_row = (MenuLayerDrawRowCallback)draw_row_callback,
          .get_cell_height =
              (MenuLayerGetCellHeightCallback)get_cell_height_callback,
          .select_click = (MenuLayerSelectCallback)menu_select_callback,
      });
#ifdef PBL_TOUCH
  touch_service_subscribe(touch_handler, NULL);
#endif

  window_set_background_color(window, BG_COLOR);
  menu_layer_set_normal_colors(s_menu_layer, BG_COLOR, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer, GColorArmyGreen, GColorWhite);

  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_status_bar = status_bar_layer_create();
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  status_bar_layer_set_colors(s_status_bar, BG_COLOR, GColorBlack);

  // Create dictation session
  s_dictation_session = dictation_session_create(
      sizeof(s_last_text), dictation_session_callback, NULL);

  s_start_dictation_on_appear = s_dictation_session != NULL &&
                                settings_get()->quick_launch_voice &&
                                launch_reason() == APP_LAUNCH_QUICK_LAUNCH;

  s_empty_msg_layer = text_layer_create(PBL_IF_ROUND_ELSE(
      GRect(0, bounds.size.h / 2 + 40, bounds.size.w, bounds.size.h),
      GRect(0, bounds.size.h / 2 + 25, bounds.size.w, bounds.size.h)));

  text_layer_set_text(s_empty_msg_layer, "No items");
  text_layer_set_background_color(s_empty_msg_layer, GColorClear);
  text_layer_set_text_alignment(s_empty_msg_layer, GTextAlignmentCenter);
  text_layer_set_font(s_empty_msg_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_empty_msg_layer));

  // inset the hint so its lines don't get too long on wide screens
  int16_t hint_margin = PBL_IF_ROUND_ELSE(0, bounds.size.w / 10);
  s_empty_hint_layer = text_layer_create(GRect(
      hint_margin, 0, bounds.size.w - hint_margin * 2, bounds.size.h));
  text_layer_set_text(s_empty_hint_layer, EMPTY_HINT_TEXT);
  text_layer_set_background_color(s_empty_hint_layer, GColorClear);
  text_layer_set_text_alignment(s_empty_hint_layer, GTextAlignmentCenter);
  text_layer_set_font(s_empty_hint_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(window_layer, text_layer_get_layer(s_empty_hint_layer));

  update_empty_msg_layer();
}

static void window_appear(Window *window) {
  if (s_start_dictation_on_appear) {
    s_start_dictation_on_appear = false;
    dictation_session_start(s_dictation_session);
  }
}

static void window_unload(Window *window) {
#ifdef PBL_TOUCH
  touch_service_unsubscribe();
#endif
  stop_move_animation();
  checklist_deinit();

  graphics_text_attributes_destroy(s_text_att);

  menu_layer_destroy(s_menu_layer);
  status_bar_layer_destroy(s_status_bar);
  text_layer_destroy(s_empty_msg_layer);
  text_layer_destroy(s_empty_hint_layer);
  dictation_session_destroy(s_dictation_session);

  gbitmap_destroy(s_tick_black_bitmap);
  gbitmap_destroy(s_tick_white_bitmap);
  gbitmap_destroy(s_add_bitmap_black);
  gbitmap_destroy(s_add_bitmap_white);

  window_destroy(window);
  s_main_window = NULL;
}

void checklist_window_push() {
  if (!s_main_window) {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers){
                                                  .load = window_load,
                                                  .appear = window_appear,
                                                  .unload = window_unload,
                                              });
  }
  window_stack_push(s_main_window, true);
}

void checklist_window_refresh() {
  // the list may have changed underneath the animation
  stop_move_animation();

  if (s_menu_layer != NULL) {
    menu_layer_reload_data(s_menu_layer);

    // make sure the selection is still in range (e.g. if the voice button
    // was just hidden or items were removed)
    uint16_t num_rows = get_num_rows_callback(s_menu_layer, 0, NULL);
    MenuIndex index = menu_layer_get_selected_index(s_menu_layer);

    if (num_rows > 0 && index.row >= num_rows) {
      menu_layer_set_selected_index(s_menu_layer, MenuIndex(0, num_rows - 1),
                                    MenuRowAlignCenter, false);
    }
  }

  update_empty_msg_layer();
}

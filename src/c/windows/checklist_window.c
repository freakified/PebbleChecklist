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

static GBitmap *s_tick_black_bitmap;
static GBitmap *s_tick_white_bitmap;
static GBitmap *s_add_bitmap_black;
static GBitmap *s_add_bitmap_white;

static GTextAttributes *s_text_att;

static DictationSession *s_dictation_session;

// Declare a buffer for the DictationSession
static char s_last_text[512];

// buffer to hold alert message
static char s_deleted_msg[30];

static const char *const CLEAR_COMPLETED_TEXT = "Clear completed";

// how long a just-toggled item stays in place before moving to its new
// position when "move checked to bottom" is enabled
#define PENDING_MOVE_DELAY_MS 400

// the item that was just toggled but is still shown in its old position (or
// -1 if none), and the timer that will move it
static int s_pending_move_id = -1;
static AppTimer *s_pending_move_timer;

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
// Returns whether an item is sorted with the checked items. A pending item
// keeps its old position until its move happens.
static bool is_item_sorted_as_checked(int id) {
  bool is_checked = checklist_get_item_by_id(id)->is_checked;
  return (id == s_pending_move_id) ? !is_checked : is_checked;
}

static int get_item_id_for_display_index(int display_index) {
  if (!settings_get()->move_checked_to_bottom) {
    return display_index;
  }

  int num_items = checklist_get_num_items();
  int num_sorted_checked = 0;
  for (int i = 0; i < num_items; i++) {
    if (is_item_sorted_as_checked(i)) {
      num_sorted_checked++;
    }
  }
  int num_unchecked = num_items - num_sorted_checked;

  bool find_checked = (display_index >= num_unchecked);
  int target = find_checked ? display_index - num_unchecked : display_index;

  int count = 0;
  for (int i = 0; i < num_items; i++) {
    if (is_item_sorted_as_checked(i) == find_checked) {
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

static void cancel_pending_move() {
  if (s_pending_move_timer != NULL) {
    app_timer_cancel(s_pending_move_timer);
    s_pending_move_timer = NULL;
  }
  s_pending_move_id = -1;
}

// Moves the pending item (if any) to its new position right away
static void finish_pending_move() {
  if (s_pending_move_id < 0) {
    return;
  }
  cancel_pending_move();
  menu_layer_reload_data(s_menu_layer);
}

static void pending_move_timer_callback(void *context) {
  s_pending_move_timer = NULL;
  finish_pending_move();
}

static void update_empty_msg_layer() {
  if (s_empty_msg_layer == NULL) {
    return;
  }

  Layer *layer = text_layer_get_layer(s_empty_msg_layer);
  layer_set_hidden(layer, (checklist_get_num_items() != 0));

  GRect bounds = layer_get_bounds(window_get_root_layer(s_main_window));

  // when the voice button is shown, the message sits below it; when it is
  // hidden, nothing else is on screen, so center the message vertically
  int16_t y;
  if (settings_get()->show_voice_button) {
    y = PBL_IF_ROUND_ELSE(
        bounds.size.h / 2 + 40,
        (bounds.size.h - STATUS_BAR_LAYER_HEIGHT) / 2 + 25);
  } else {
    y = PBL_IF_ROUND_ELSE(
        bounds.size.h / 2 - 12,
        STATUS_BAR_LAYER_HEIGHT + (bounds.size.h - STATUS_BAR_LAYER_HEIGHT) / 2 - 12);
  }

  GRect frame = layer_get_frame(layer);
  frame.origin.y = y;
  layer_set_frame(layer, frame);
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
    finish_pending_move();
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

static void draw_checkbox_cell(GContext *ctx, Layer *cell_layer,
                               MenuIndex *cell_index) {
  int id = get_item_id_for_display_index(cell_index->row - get_item_row_offset());

  ChecklistItem *item = checklist_get_item_by_id(id);

  GRect bounds = layer_get_bounds(cell_layer);

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
    menu_cell_basic_draw(ctx, cell_layer, item->name, NULL, NULL);
  } else {
// on round watches, single line cells should always be center aligned with no
// margin, (since anything else looks bad)
#ifdef PBL_ROUND
    text_bounds =
        GRect(CHECKLIST_CELL_MARGIN, 0,
              bounds.size.w - CHECKLIST_WINDOW_BOX_SIZE * 4, bounds.size.h);
#else
    text_bounds = GRect(CHECKLIST_CELL_MARGIN, 0,
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
      text_bounds.origin.y =
          (bounds.size.h - text_size.h) / 2 - get_item_font_center_offset();
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
                    (bounds.size.h / 2) - (CHECKLIST_WINDOW_BOX_SIZE / 2),
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

    strike_start_point.y = bounds.size.h / 2;
    strike_end_point.y = bounds.size.h / 2;

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
    draw_checkbox_cell(ctx, cell_layer, cell_index);
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
    int id = get_item_id_for_display_index(cell_index->row - offset);

    ChecklistItem *item = checklist_get_item_by_id(id);

    int width = PBL_IF_ROUND_ELSE(screen_width - CHECKLIST_WINDOW_BOX_SIZE * 4,
                                  screen_width - CHECKLIST_CELL_MARGIN * 2 -
                                      CHECKLIST_WINDOW_BOX_SIZE * 2);

    return get_text_cell_height(item->name, width);
  }
}

static void select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                            void *callback_context) {
  uint16_t offset = get_item_row_offset();
  bool is_item_row = !(offset > 0 && cell_index->row == 0) &&
                     cell_index->row != checklist_get_num_items() + offset;

  if (is_item_row && s_pending_move_id >= 0 &&
      get_item_id_for_display_index(cell_index->row - offset) ==
          s_pending_move_id) {
    // the item was toggled again before it moved: undo, leaving it in place
    checklist_item_toggle_checked(s_pending_move_id);
    cancel_pending_move();
    menu_layer_reload_data(menu_layer);
    return;
  }

  finish_pending_move();

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

    // if the item will move, briefly leave it in place so the change is
    // visible before it goes
    if (settings_get()->move_checked_to_bottom &&
        get_display_index_for_item_id(id) != display_index) {
      s_pending_move_id = id;
      s_pending_move_timer = app_timer_register(
          PENDING_MOVE_DELAY_MS, pending_move_timer_callback, NULL);
    }

    menu_layer_reload_data(menu_layer);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  MenuIndex index = menu_layer_get_selected_index(s_menu_layer);
  select_callback(s_menu_layer, &index, NULL);
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

  if (s_pending_move_id >= 0) {
    finish_pending_move();

    // the item below the moved one has now slid into the selected row, so
    // it's already where "down" would have gone
    if (!up) {
      return;
    }
  }

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
          .select_click = (MenuLayerSelectCallback)select_callback,
      });

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

  s_empty_msg_layer = text_layer_create(PBL_IF_ROUND_ELSE(
      GRect(0, bounds.size.h / 2 + 40, bounds.size.w, bounds.size.h),
      GRect(0, bounds.size.h / 2 + 25, bounds.size.w, bounds.size.h)));

  text_layer_set_text(s_empty_msg_layer, "No items");
  text_layer_set_background_color(s_empty_msg_layer, GColorClear);
  text_layer_set_text_alignment(s_empty_msg_layer, GTextAlignmentCenter);
  text_layer_set_font(s_empty_msg_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_empty_msg_layer));

  update_empty_msg_layer();
}

static void window_unload(Window *window) {
  cancel_pending_move();
  checklist_deinit();

  graphics_text_attributes_destroy(s_text_att);

  menu_layer_destroy(s_menu_layer);
  status_bar_layer_destroy(s_status_bar);
  text_layer_destroy(s_empty_msg_layer);
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
                                                  .unload = window_unload,
                                              });
  }
  window_stack_push(s_main_window, true);
}

void checklist_window_refresh() {
  // the list may have changed underneath the pending item
  cancel_pending_move();

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

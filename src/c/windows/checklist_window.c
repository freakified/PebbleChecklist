/**
 * The main checklist window, showing the the list of items with their associated checkboxes.
 */

#include "checklist_window.h"
#include "dialog_message_window.h"
#include "../checklist.h"
#include "../util.h"

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

static void draw_add_button(GContext *ctx, Layer *cell_layer) {
  GRect bounds = layer_get_bounds(cell_layer);
  GRect bitmap_bounds = gbitmap_get_bounds(s_add_bitmap_black);

  GPoint pos;
  pos.x = (bounds.size.w / 2) - (bitmap_bounds.size.w / 2);
  pos.y = (bounds.size.h / 2) - (bitmap_bounds.size.h / 2);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  GBitmap *imageToUse = s_tick_black_bitmap;

  if(menu_cell_layer_is_highlighted(cell_layer)) {
    imageToUse = s_add_bitmap_white;
  } else {
    imageToUse = s_add_bitmap_black;
  }

  graphics_draw_bitmap_in_rect(ctx, imageToUse, GRect(pos.x, pos.y, bitmap_bounds.size.w, bitmap_bounds.size.h));
}

static void dictation_session_callback(DictationSession *session, DictationSessionStatus status,
                                       char *transcription, void *context) {

  // Print the results of a transcription attempt
  APP_LOG(APP_LOG_LEVEL_INFO, "Dictation status: %d", (int)status);

  if(status == DictationSessionStatusSuccess) {
    checklist_add_items(transcription);
    menu_layer_reload_data(s_menu_layer);
  }
}

static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  if(checklist_get_num_items() == 0) {
    return 1;
  } else {
    if(checklist_get_num_items_checked() > 0) {
      return checklist_get_num_items() + 2;
    } else {
      return checklist_get_num_items() + 1;
    }
  }
}

static void draw_checkbox_cell(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index) {
  // draw a checklist item
  int id = checklist_get_num_items() - (cell_index->row - 1) - 1;

  ChecklistItem *item = checklist_get_item_by_id(id);

  GRect bounds = layer_get_bounds(cell_layer);

  GRect text_bounds;
  GTextAlignment alignment = GTextAlignmentLeft;

  if(item->is_checked) {
    if(menu_cell_layer_is_highlighted(cell_layer)) {
      graphics_context_set_text_color(ctx, GColorLimerick);
    } else {
      graphics_context_set_text_color(ctx, GColorArmyGreen);
    }
  } else {
    if(menu_cell_layer_is_highlighted(cell_layer)) {
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_text_color(ctx, GColorBlack);
    }
  }

  // for single-height cells, use a standard draw command
  if(bounds.size.h == CHECKLIST_CELL_MIN_HEIGHT) {
    menu_cell_basic_draw(ctx, cell_layer, item->name, NULL, NULL);
  } else {
    // on round watches, single line cells should always be center aligned with no margin,
    // (since anything else looks bad)
    #ifdef PBL_ROUND
      text_bounds = GRect(CHECKLIST_CELL_MARGIN, 0,
                               bounds.size.w - CHECKLIST_WINDOW_BOX_SIZE * 4,
                               bounds.size.h);
    #else
      text_bounds = GRect(CHECKLIST_CELL_MARGIN, 0,
                               bounds.size.w - CHECKLIST_CELL_MARGIN * 2 - CHECKLIST_WINDOW_BOX_SIZE * 2,
                               bounds.size.h);
    #endif

    graphics_draw_text(ctx, item->name,
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       text_bounds,
                       GTextOverflowModeTrailingEllipsis,
                       alignment,
                       s_text_att);

  }

  if(menu_cell_layer_is_highlighted(cell_layer)) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
  }

  GRect bitmap_bounds = gbitmap_get_bounds(s_tick_black_bitmap);

  GBitmap *imageToUse = s_tick_black_bitmap;

  if(menu_cell_layer_is_highlighted(cell_layer)) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    imageToUse = s_tick_white_bitmap;
  }

  // on round watches, only show the checkbox for the selected item
  bool show_checkbox = true;

  #ifdef PBL_ROUND
    show_checkbox = menu_cell_layer_is_highlighted(cell_layer);
  #endif

  if(show_checkbox) {
    GRect r = GRect(
      bounds.size.w - (2 * CHECKLIST_WINDOW_BOX_SIZE),
      (bounds.size.h / 2) - (CHECKLIST_WINDOW_BOX_SIZE / 2),
      CHECKLIST_WINDOW_BOX_SIZE,
      CHECKLIST_WINDOW_BOX_SIZE
    );

    graphics_draw_rect(ctx, r);

    if(item->is_checked) {
      // draw the checkmark
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, imageToUse, GRect(r.origin.x, r.origin.y - 3, bitmap_bounds.size.w, bitmap_bounds.size.h));
    }
  }


  // draw text strikethrough
  if(item->is_checked) {
    graphics_context_set_stroke_width(ctx, 2);

    GPoint strike_start_point, strike_end_point;

    strike_start_point.y = bounds.size.h / 2;
    strike_end_point.y = bounds.size.h / 2;

    // for single-height cells, draw a true strikethrough
    if(bounds.size.h == CHECKLIST_CELL_MIN_HEIGHT) {
      GSize text_size = graphics_text_layout_get_content_size(item->name,
                                                   fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                                                   bounds,
                                                   GTextOverflowModeTrailingEllipsis,
                                                   PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));

      // draw centered for round, left-aligned for rect
      #ifdef PBL_ROUND
        strike_start_point.x = (bounds.size.w / 2) - (text_size.w / 2);
        strike_end_point.x = (bounds.size.w / 2) + (text_size.w / 2);
      #else
        strike_start_point.x = CHECKLIST_CELL_MARGIN;
        strike_end_point.x =  CHECKLIST_CELL_MARGIN + text_size.w;
      #endif

    } else {
      // otherwise, draw a full-width line

      #ifdef PBL_ROUND
        strike_start_point.x = text_bounds.origin.x + CHECKLIST_CELL_MARGIN;
        strike_end_point.x = text_bounds.origin.x + text_bounds.size.w - CHECKLIST_CELL_MARGIN;
      #else
        strike_start_point.x = CHECKLIST_CELL_MARGIN;
        strike_end_point.x =  CHECKLIST_CELL_MARGIN + text_bounds.size.w;
      #endif
    }

    graphics_draw_line(ctx, strike_start_point, strike_end_point);
  }
}

static void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *context) {
  layer_set_hidden(text_layer_get_layer(s_empty_msg_layer), (checklist_get_num_items() != 0));

  if(cell_index->row == 0) {
    // draw the add action
    draw_add_button(ctx, cell_layer);
  } else if(cell_index->row == checklist_get_num_items() + 1) {
    // draw the clear action
    menu_cell_basic_draw(ctx, cell_layer, "Clear completed", NULL, NULL);
  } else {
    // draw the checkbox
    draw_checkbox_cell(ctx, cell_layer, cell_index);
  }

}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if(cell_index->row == 0 || cell_index->row == checklist_get_num_items() + 1) {
    return CHECKLIST_CELL_MIN_HEIGHT;
  } else {
    int id = checklist_get_num_items() - (cell_index->row - 1) - 1;

    ChecklistItem *item = checklist_get_item_by_id(id);

    int screen_width = layer_get_bounds(window_get_root_layer(s_main_window)).size.w;
    int width =  PBL_IF_ROUND_ELSE(screen_width - CHECKLIST_WINDOW_BOX_SIZE * 4, screen_width - CHECKLIST_CELL_MARGIN * 2 - CHECKLIST_WINDOW_BOX_SIZE * 2);

    GSize size = graphics_text_layout_get_content_size_with_attributes(item->name,
                                                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                                                       GRect(0, 0, width, 500),
                                                       GTextOverflowModeTrailingEllipsis,
                                                       PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft),
                                                       NULL);

    if(size.h > CHECKLIST_CELL_MAX_HEIGHT) {
      return CHECKLIST_CELL_MAX_HEIGHT;
    } else if(size.h < CHECKLIST_CELL_MIN_HEIGHT) {
      return CHECKLIST_CELL_MIN_HEIGHT;
    } else {
      return size.h + CHECKLIST_CELL_MARGIN * 2;
    }
  }
}

static void select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if(cell_index->row == 0) {
    // the first row is always the "add" button
    if(s_dictation_session != NULL) {
      dictation_session_start(s_dictation_session);
    } else {
      dialog_warning_window_push("Voice is offline, add items via the settings page.");
    }
  } else if(cell_index->row == checklist_get_num_items() + 1) {
    // the last row is always the "clear completed" button
    int num_deleted = checklist_get_num_items_checked();

    // generate and display "items deleted" message
    snprintf(s_deleted_msg,
             sizeof(s_deleted_msg),
             ((num_deleted == 1) ? "%i Item Deleted" : "%i Items Deleted"),
             num_deleted);

    // do stuff
    dialog_shred_window_push(s_deleted_msg);
    checklist_delete_completed_items();
    menu_layer_reload_data(menu_layer);

  } else {
    // if the item is a checklist item, toggle its checked state
    // get the id number of the checklist item to delete
    int id = checklist_get_num_items() - (cell_index->row - 1) - 1;

    checklist_item_toggle_checked(id);

    menu_layer_reload_data(menu_layer);
  }
}

static void window_load(Window *window) {
  checklist_init();

  Layer *window_layer = window_get_root_layer(window);
  GRect windowBounds = layer_get_bounds(window_layer);;

  #ifdef PBL_ROUND
    GRect bounds = windowBounds;
  #else
    GRect bounds = GRect(0, STATUS_BAR_LAYER_HEIGHT, windowBounds.size.w, windowBounds.size.h - STATUS_BAR_LAYER_HEIGHT);
  #endif

  s_text_att = graphics_text_attributes_create();

  #ifdef PBL_ROUND
    graphics_text_attributes_enable_screen_text_flow(s_text_att, CHECKLIST_CELL_MARGIN * 2);
  #endif

  s_tick_black_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TICK_BLACK);
  s_tick_white_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TICK_WHITE);
  s_add_bitmap_black = gbitmap_create_with_resource(RESOURCE_ID_ADD_BLACK);
  s_add_bitmap_white = gbitmap_create_with_resource(RESOURCE_ID_ADD_WHITE);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  menu_layer_set_center_focused(s_menu_layer, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
      .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback)get_num_rows_callback,
      .draw_row = (MenuLayerDrawRowCallback)draw_row_callback,
      .get_cell_height = (MenuLayerGetCellHeightCallback)get_cell_height_callback,
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
  s_dictation_session = dictation_session_create(sizeof(s_last_text),
                                                 dictation_session_callback, NULL);

  s_empty_msg_layer = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(0, bounds.size.h / 2 + 40, bounds.size.w, bounds.size.h),
    GRect(0, bounds.size.h / 2 + 25, bounds.size.w, bounds.size.h)
  ));

  text_layer_set_text(s_empty_msg_layer, "No items");
  text_layer_set_background_color(s_empty_msg_layer, GColorClear);
  text_layer_set_text_alignment(s_empty_msg_layer, GTextAlignmentCenter);
  text_layer_set_font(s_empty_msg_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_empty_msg_layer));

}

static void window_unload(Window *window) {
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
  if(!s_main_window) {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
  }
  window_stack_push(s_main_window, true);
}

void checklist_window_refresh() {
  if(s_menu_layer != NULL) {
    menu_layer_reload_data(s_menu_layer);
  }
}

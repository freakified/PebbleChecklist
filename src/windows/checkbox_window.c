/**
 * Example implementation of the checkbox list UI pattern.
 */

#include "checkbox_window.h"
#include "dialog_message_window.h"
// #include "../checklist_item.h"
#include "../values.h"
#include "../util.h"

static Window *s_main_window;
static MenuLayer *s_menu_layer;
static StatusBarLayer *s_status_bar;
static TextLayer *s_empty_msg_layer;

static GBitmap *s_tick_black_bitmap;
static GBitmap *s_tick_white_bitmap;
static GBitmap *add_bitmap_black;
static GBitmap *add_bitmap_white;

static  DictationSession *s_dictation_session;
// Declare a buffer for the DictationSession
static char s_last_text[512];

static char s_deleted_msg[30];

// max items, 30 chars per item
static char checkListItems[CHECKBOX_WINDOW_MAX_ITEMS][30];
static bool s_selections[CHECKBOX_WINDOW_MAX_ITEMS];
static int numberOfChecklistItems = 0;

static void dictation_session_callback(DictationSession *session, DictationSessionStatus status,
                                       char *transcription, void *context) {

  // Print the results of a transcription attempt
  APP_LOG(APP_LOG_LEVEL_INFO, "Dictation status: %d", (int)status);

  if(status == DictationSessionStatusSuccess) {
    strncpy(checkListItems[numberOfChecklistItems], transcription, 30);
    numberOfChecklistItems++;
  }

  // TODO: Add bounds checking
}

static void draw_add_button(GContext *ctx, Layer *cell_layer);

static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  if(numberOfChecklistItems == 0) {
    return 1;
  } else {
    return numberOfChecklistItems + 2;
  }
}

static void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *context) {
  printf("number of items: %i", numberOfChecklistItems);
  layer_set_hidden(text_layer_get_layer(s_empty_msg_layer), (numberOfChecklistItems != 0));

  if(cell_index->row == 0) {
    // Add action
    draw_add_button(ctx, cell_layer);
  } else if(cell_index->row == numberOfChecklistItems + 1) {
    // Clear action
    menu_cell_basic_draw(ctx, cell_layer, "Clear completed", NULL, NULL);
  } else {
    // it's a checklist item
    // int idx = cell_index->row - 1;
    int idx = numberOfChecklistItems - (cell_index->row - 1) - 1;

    menu_cell_basic_draw(ctx, cell_layer, checkListItems[idx], NULL, NULL);

    if(menu_cell_layer_is_highlighted(cell_layer)) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
    }

    GRect bounds = layer_get_bounds(cell_layer);
    GRect bitmap_bounds = gbitmap_get_bounds(s_tick_black_bitmap);

    GBitmap *imageToUse = s_tick_black_bitmap;

    if(menu_cell_layer_is_highlighted(cell_layer)) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      imageToUse = s_tick_white_bitmap;
    }

    // Draw checkbox
    GRect r = GRect(
      bounds.size.w - (2 * CHECKBOX_WINDOW_BOX_SIZE),
      (bounds.size.h / 2) - (CHECKBOX_WINDOW_BOX_SIZE / 2),
      CHECKBOX_WINDOW_BOX_SIZE,
      CHECKBOX_WINDOW_BOX_SIZE
    );

    graphics_draw_rect(ctx, r);

    if(s_selections[idx]) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, imageToUse, GRect(r.origin.x, r.origin.y - 3, bitmap_bounds.size.w, bitmap_bounds.size.h));

      graphics_context_set_stroke_width(ctx, 2);

      // draw text strikethrough
      GSize size = graphics_text_layout_get_content_size(checkListItems[idx],
                                                         fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                                                         bounds,
                                                         GTextOverflowModeTrailingEllipsis,
                                                         PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));

      // draw centered for round, left-aligned for rect
      #ifdef PBL_ROUND
        graphics_draw_line(ctx,
                           GPoint((bounds.size.w / 2) - (size.w / 2), bounds.size.h / 2 ),
                           GPoint((bounds.size.w / 2) + (size.w / 2), bounds.size.h / 2 ));
      #else
        graphics_draw_line(ctx,
                           GPoint(5, bounds.size.h / 2 ),
                           GPoint(5 + size.w, bounds.size.h / 2 ));
      #endif
    }
  }
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  #ifdef PBL_ROUND
    return menu_layer_menu_index_selected(menu_layer, cell_index) ?
      FOCUSED_TALL_CELL_HEIGHT : UNFOCUSED_TALL_CELL_HEIGHT;
  #else
    return CHECKBOX_WINDOW_CELL_HEIGHT;
  #endif
}

static void select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if(cell_index->row == 0) {
    dictation_session_start(s_dictation_session);
  } else if(cell_index->row == numberOfChecklistItems + 1) {
    // Clear the completed items
    int numDeleted = 0;

    // TODO: make this not awful
    int i = 0;
    while (i < numberOfChecklistItems) {
      printf("NumberOfItems: %i s_selections[%i] = %i", numberOfChecklistItems, i, s_selections[i]);
      if(s_selections[i]) { // is the item checked?

        // delete the item
        memmove(&s_selections[i], &s_selections[i+1], sizeof(s_selections[0])*(numberOfChecklistItems - i));
        memmove(&checkListItems[i], &checkListItems[i+1], sizeof(checkListItems[0])*(numberOfChecklistItems - i));

        numDeleted++;
        numberOfChecklistItems--;
      } else {
        i++;
      }
    }

    // Display indication
    if(numDeleted == 0) {
      snprintf(s_deleted_msg, sizeof(s_deleted_msg), "Nothing to delete!");
    } else if(numDeleted == 1) {
      snprintf(s_deleted_msg, sizeof(s_deleted_msg), "%i Item Deleted", numDeleted);
    } else {
      snprintf(s_deleted_msg, sizeof(s_deleted_msg), "%i Items Deleted", numDeleted);
    }

    dialog_message_window_push(s_deleted_msg);

    menu_layer_reload_data(menu_layer);
  } else {
    // Check/uncheck
    int idx = numberOfChecklistItems - (cell_index->row - 1) - 1;

    printf("Row %i was hit!", idx);
    s_selections[idx] = !s_selections[idx];
    menu_layer_reload_data(menu_layer);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect windowBounds = layer_get_bounds(window_layer);;

  #ifdef PBL_ROUND
    GRect bounds = layer_get_bounds(window_layer);
  #else
    GRect bounds = GRect(0, STATUS_BAR_LAYER_HEIGHT, windowBounds.size.w, windowBounds.size.h - STATUS_BAR_LAYER_HEIGHT);
  #endif

  s_tick_black_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TICK_BLACK);
  s_tick_white_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TICK_WHITE);
  add_bitmap_black = gbitmap_create_with_resource(RESOURCE_ID_ADD_BLACK);
  add_bitmap_white = gbitmap_create_with_resource(RESOURCE_ID_ADD_WHITE);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  menu_layer_set_center_focused(s_menu_layer, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
      .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback)get_num_rows_callback,
      .draw_row = (MenuLayerDrawRowCallback)draw_row_callback,
      .get_cell_height = (MenuLayerGetCellHeightCallback)get_cell_height_callback,
      .select_click = (MenuLayerSelectCallback)select_callback,
  });

  window_set_background_color(window, GColorYellow);
  menu_layer_set_normal_colors(s_menu_layer, GColorYellow, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer, GColorArmyGreen, GColorWhite);

  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_status_bar = status_bar_layer_create();
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  status_bar_layer_set_colors(s_status_bar, GColorYellow, GColorBlack);

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
  menu_layer_destroy(s_menu_layer);

  gbitmap_destroy(s_tick_black_bitmap);
  gbitmap_destroy(s_tick_white_bitmap);
  gbitmap_destroy(add_bitmap_black);
  gbitmap_destroy(add_bitmap_white);

  window_destroy(window);
  s_main_window = NULL;
}

void checkbox_window_push() {
  if(!s_main_window) {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
  }
  window_stack_push(s_main_window, true);
}

static void draw_add_button(GContext *ctx, Layer *cell_layer) {
  GRect bounds = layer_get_bounds(cell_layer);
  // printf("Bounds: X: %i, Y: %i, W: %i, H: %i", bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h);
  GRect bitmap_bounds = gbitmap_get_bounds(add_bitmap_black);

  GPoint pos;
  pos.x = (bounds.size.w / 2) - (bitmap_bounds.size.w / 2);
  pos.y = (bounds.size.h / 2) - (bitmap_bounds.size.h / 2);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  GBitmap *imageToUse = s_tick_black_bitmap;

  if(menu_cell_layer_is_highlighted(cell_layer)) {
    imageToUse = add_bitmap_white;
  } else {
    imageToUse = add_bitmap_black;
  }

  graphics_draw_bitmap_in_rect(ctx, imageToUse, GRect(pos.x, pos.y, bitmap_bounds.size.w, bitmap_bounds.size.h));
}

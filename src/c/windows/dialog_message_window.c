/**
 * Example implementation of the dialog message UI pattern.
 */

#include "dialog_message_window.h"

#define MARGIN 10
#define DELTA 13

static Window *s_main_window;
static TextLayer *s_label_layer;
static Layer *s_background_layer;
static Layer *s_canvas_layer;
#ifdef PBL_PLATFORM_APLITE
static GDrawCommandImage *s_command_img;
#else
static GDrawCommandSequence *s_command_seq;
#endif
static AppTimer *s_timer;
static char *s_message_text;
static int s_current_frame_idx = 0;

typedef enum { SETTINGS, SHRED } DialogType;
static DialogType s_dialog_type;

static GBitmap *s_icon_bitmap;
static BitmapLayer *s_icon_layer;
static GColor s_background_color;

// animation code
static void next_frame_handler(void *context) {
  s_timer = NULL;
#ifdef PBL_PLATFORM_APLITE
  window_stack_pop(true);
#else
  // Draw the next frame
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  // Continue the sequence
  s_timer = app_timer_register(DELTA, next_frame_handler, NULL);
#endif
}

static void background_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, s_background_color);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, 0);
}

static void update_proc(Layer *layer, GContext *ctx) {
#ifdef PBL_PLATFORM_APLITE
  if (s_command_img) {
    gdraw_command_image_draw(ctx, s_command_img,
                             PBL_IF_ROUND_ELSE(GPoint(20, 10), GPoint(5, 10)));
  }
#else
  // Get the next frame
  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(
      s_command_seq, s_current_frame_idx);

  // If another frame was found, draw it
  if (frame) {
    gdraw_command_frame_draw(ctx, s_command_seq, frame,
                             PBL_IF_ROUND_ELSE(GPoint(20, 10), GPoint(5, 10)));
  }

  // Advance to the next frame, wrapping if neccessary
  int num_frames = gdraw_command_sequence_get_num_frames(s_command_seq);
  s_current_frame_idx++;
  if (s_current_frame_idx == num_frames) {
    // if we run out of frames, stop the animation
    if (s_timer != NULL) {
      app_timer_cancel(s_timer);
      s_timer = NULL;
    }
    window_stack_pop(true);
  }
#endif
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_background_layer = layer_create(bounds);
  layer_set_update_proc(s_background_layer, background_update_proc);
  layer_add_child(window_layer, s_background_layer);

  if (s_dialog_type == SHRED) {
    // Create the canvas Layer
#ifdef PBL_PLATFORM_APLITE
    s_canvas_layer = layer_create(GRect(30, 10, bounds.size.w, bounds.size.h));
#else
    s_canvas_layer = layer_create(GRect(30, 30, bounds.size.w, bounds.size.h));
#endif

#ifdef PBL_PLATFORM_APLITE
    s_command_img =
        gdraw_command_image_create_with_resource(RESOURCE_ID_DELETED_STATIC);
#else
    s_command_seq = gdraw_command_sequence_create_with_resource(
        RESOURCE_ID_DELETED_SEQUENCE);
#endif

    // Add to parent Window
    layer_add_child(window_layer, s_canvas_layer);

    // Set the LayerUpdateProc
    s_current_frame_idx = 0;
    layer_set_update_proc(s_canvas_layer, update_proc);

#ifdef PBL_PLATFORM_APLITE
    s_timer = app_timer_register(1000, next_frame_handler, NULL);
#else
    s_timer = app_timer_register(DELTA, next_frame_handler, NULL);
#endif

    s_background_color = PBL_IF_COLOR_ELSE(GColorLimerick, GColorWhite);

    s_label_layer =
        text_layer_create(GRect(MARGIN, bounds.size.h / 2 + 15 + MARGIN,
                                bounds.size.w - (2 * MARGIN), bounds.size.h));
  } else {
    s_background_color = PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite);
    ;

    s_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SETTINGS);
    GRect bitmap_bounds = gbitmap_get_bounds(s_icon_bitmap);

    s_icon_layer = bitmap_layer_create(
        GRect(bounds.size.w / 2 - bitmap_bounds.size.w / 2, MARGIN,
              bitmap_bounds.size.w, bitmap_bounds.size.h));

    bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
    bitmap_layer_set_compositing_mode(s_icon_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_icon_layer));

    s_label_layer =
        text_layer_create(GRect(MARGIN, bounds.size.h / 2 + 10,
                                bounds.size.w - (2 * MARGIN), bounds.size.h));
  }

  text_layer_set_text(s_label_layer, s_message_text);
  text_layer_set_background_color(s_label_layer, GColorClear);
  text_layer_set_text_alignment(s_label_layer, GTextAlignmentCenter);
  text_layer_set_font(s_label_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_label_layer));

  // Start the animation
}

static void window_unload(Window *window) {
  if (s_canvas_layer != NULL) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }

  if (s_label_layer != NULL) {
    text_layer_destroy(s_label_layer);
    s_label_layer = NULL;
  }

  if (s_icon_bitmap != NULL) {
    gbitmap_destroy(s_icon_bitmap);
    s_icon_bitmap = NULL;
  }

  if (s_icon_layer != NULL) {
    bitmap_layer_destroy(s_icon_layer);
    s_icon_layer = NULL;
  }

#ifdef PBL_PLATFORM_APLITE
  if (s_command_img != NULL) {
    gdraw_command_image_destroy(s_command_img);
    s_command_img = NULL;
  }
#else
  if (s_command_seq != NULL) {
    gdraw_command_sequence_destroy(s_command_seq);
    s_command_seq = NULL;
  }
#endif

  if (s_timer != NULL) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }

  if (s_background_layer != NULL) {
    layer_destroy(s_background_layer);
    s_background_layer = NULL;
  }

  window_destroy(window);
  s_main_window = NULL;
}

void dialog_shred_window_push(char *message) {
  s_dialog_type = SHRED;
  s_message_text = message;

  if (!s_main_window) {
    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorBlack);
    window_set_window_handlers(
        s_main_window,
        (WindowHandlers){.load = window_load, .unload = window_unload});
  }

  // display the window
  window_stack_push(s_main_window, true);
}

void dialog_settings_window_push(char *message) {
  s_dialog_type = SETTINGS;
  s_message_text = message;

  if (!s_main_window) {
    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorBlack);
    window_set_window_handlers(
        s_main_window,
        (WindowHandlers){.load = window_load, .unload = window_unload});
  }

  window_stack_push(s_main_window, true);
}

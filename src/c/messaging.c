#include "messaging.h"
#include "checklist.h"
#include "settings.h"
#include "util.h"
#include <pebble.h>

static char s_items_to_add_buffer[512];
static char s_current_state_buffer[LIST_JSON_MAX_BYTES + 1];

void (*message_processed_callback)(void);

void messaging_init(void (*processed_callback)(void)) {
  // register my custom callback
  message_processed_callback = processed_callback;

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  // app_message_open(app_message_inbox_size_maximum(),
  // app_message_outbox_size_maximum());
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Watch messaging is started!");
  app_message_register_inbox_received(inbox_received_callback);
}

void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Check for items to add (existing functionality)
  Tuple *items_to_add_tuple = dict_find(iterator, KEY_ITEMS_TO_ADD);

  if (items_to_add_tuple != NULL) {
    strncpy(s_items_to_add_buffer, items_to_add_tuple->value->cstring,
            sizeof(s_items_to_add_buffer) - 1);

    checklist_add_items(s_items_to_add_buffer);
  }

  // Check for state request (new two-way sync functionality)
  Tuple *request_state_tuple = dict_find(iterator, KEY_REQUEST_STATE);

  if (request_state_tuple != NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,
            "State request received, sending current state");
    send_current_state_to_phone();
  }

  // Check for item updates (new two-way sync functionality)
  Tuple *item_updates_tuple = dict_find(iterator, KEY_ITEM_UPDATES);

  if (item_updates_tuple != NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Item updates received: %s",
            item_updates_tuple->value->cstring);
    process_item_updates(item_updates_tuple->value->cstring);
  }

  // Check for settings changes
  Tuple *settings_tuple = dict_find(iterator, KEY_SETTINGS);

  if (settings_tuple != NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Settings received: %d",
            (int)settings_tuple->value->int32);
    settings_apply_bitfield(settings_tuple->value->int32);
    settings_save();
  }

  // notify the main screen, in case something changed
  message_processed_callback();
}

void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

void outbox_failed_callback(DictionaryIterator *iterator,
                            AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! %d %d %d", reason,
          APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED);
}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

// JSON serialization functions for two-way sync
//
// The list is exchanged as a compact JSON array in both directions:
//   [{"n":"Milk","c":0},{"n":"Eggs","c":1}]
// Only '"' and '\\' are escaped by the watch; the parser below also accepts the
// escapes JSON.stringify may produce on the phone side.

// Returns the number of bytes needed to write the name as a JSON string body
static int escaped_length(const char *name) {
  int len = 0;
  for (int i = 0; name[i] != '\0'; i++) {
    len += (name[i] == '"' || name[i] == '\\') ? 2 : 1;
  }
  return len;
}

void serialize_current_state() {
  int num_items = checklist_get_num_items();
  int pos = 0;
  int max_pos = sizeof(s_current_state_buffer) - 2; // room for ']' and '\0'

  s_current_state_buffer[pos++] = '[';

  for (int i = 0; i < num_items; i++) {
    ChecklistItem *item = checklist_get_item_by_id(i);

    // {"n":"<name>","c":0} plus a separating comma; only whole items are
    // written, so a list that doesn't fit is cut short rather than mangled
    int needed = (i > 0 ? 1 : 0) + 14 + escaped_length(item->name);
    if (pos + needed > max_pos) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "State too large; sent %d of %d items",
              i, num_items);
      break;
    }

    if (i > 0) {
      s_current_state_buffer[pos++] = ',';
    }

    memcpy(&s_current_state_buffer[pos], "{\"n\":\"", 6);
    pos += 6;

    for (int j = 0; item->name[j] != '\0'; j++) {
      if (item->name[j] == '"' || item->name[j] == '\\') {
        s_current_state_buffer[pos++] = '\\';
      }
      s_current_state_buffer[pos++] = item->name[j];
    }

    memcpy(&s_current_state_buffer[pos], "\",\"c\":", 6);
    pos += 6;
    s_current_state_buffer[pos++] = item->is_checked ? '1' : '0';
    s_current_state_buffer[pos++] = '}';
  }

  s_current_state_buffer[pos++] = ']';
  s_current_state_buffer[pos] = '\0';
}

void send_current_state_to_phone() {
  serialize_current_state();

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter) {
    dict_write_cstring(iter, KEY_CURRENT_STATE, s_current_state_buffer);
    dict_write_int32(iter, KEY_SETTINGS, settings_to_bitfield());

    // lets the config page tell if the list had to be cut short
    dict_write_int32(iter, KEY_TOTAL_ITEMS, checklist_get_num_items());
    app_message_outbox_send();
  }
}

// Reads a JSON string body starting just after its opening quote into buf
// (truncating if needed), and returns a pointer just past the closing quote
static const char *read_json_string(const char *ptr, char *buf, int buf_size) {
  int len = 0;

  while (*ptr && *ptr != '"') {
    char c = *ptr++;

    if (c == '\\' && *ptr) {
      char escaped = *ptr++;
      switch (escaped) {
        case 'n': case 'r': case 't': c = ' '; break;
        case 'u': c = '?'; ptr += (strlen(ptr) >= 4) ? 4 : strlen(ptr); break;
        default: c = escaped; break; // '"', '\\', '/'
      }
    }

    if (len < buf_size - 1) {
      buf[len++] = c;
    }
  }

  buf[len] = '\0';
  utf8_trim_partial(buf);

  return (*ptr == '"') ? ptr + 1 : ptr;
}

void process_item_updates(const char *json_string) {
  checklist_clear();

  const char *ptr = json_string;

  while ((ptr = strstr(ptr, "\"n\":\"")) != NULL) {
    char name[MAX_NAME_LENGTH];
    ptr = read_json_string(ptr + 5, name, sizeof(name));

    const char *checked = strstr(ptr, "\"c\":");
    if (!checked) {
      break;
    }
    ptr = checked + 4;
    bool is_checked = (*ptr == '1' || *ptr == 't');

    // only mark the item checked if it was actually added (the list may be
    // full, or the name may have been blank)
    int num_items_before = checklist_get_num_items();
    checklist_add_items(name);

    if (is_checked && checklist_get_num_items() > num_items_before) {
      checklist_item_toggle_checked(num_items_before);
    }
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Processed item updates");
}

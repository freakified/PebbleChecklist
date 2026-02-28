#include "messaging.h"
#include "checklist.h"
#include <pebble.h>

static char s_items_to_add_buffer[512];
static char s_current_state_buffer[1024];

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
void serialize_current_state() {
  int num_items = checklist_get_num_items();
  int pos = 0;

  // Start JSON array
  s_current_state_buffer[pos++] = '[';

  for (int i = 0; i < num_items; i++) {
    ChecklistItem *item = checklist_get_item_by_id(i);

    // Add opening brace
    s_current_state_buffer[pos++] = '{';

    // Add name field
    s_current_state_buffer[pos++] = '"';
    s_current_state_buffer[pos++] = 'n';
    s_current_state_buffer[pos++] = '"';
    s_current_state_buffer[pos++] = ':';
    s_current_state_buffer[pos++] = '"';

    // Copy name (escape quotes if needed)
    for (int j = 0; item->name[j] != '\0' &&
                    pos < (int)sizeof(s_current_state_buffer) - 10;
         j++) {
      if (item->name[j] == '"') {
        s_current_state_buffer[pos++] = '\\';
      }
      s_current_state_buffer[pos++] = item->name[j];
    }

    s_current_state_buffer[pos++] = '"';
    s_current_state_buffer[pos++] = ',';

    // Add checked field
    s_current_state_buffer[pos++] = '"';
    s_current_state_buffer[pos++] = 'c';
    s_current_state_buffer[pos++] = '"';
    s_current_state_buffer[pos++] = ':';
    s_current_state_buffer[pos++] = item->is_checked ? '1' : '0';

    // Add closing brace
    s_current_state_buffer[pos++] = '}';

    // Add comma if not last item
    if (i < num_items - 1) {
      s_current_state_buffer[pos++] = ',';
    }
  }

  // Close JSON array
  s_current_state_buffer[pos++] = ']';
  s_current_state_buffer[pos] = '\0';

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Serialized current state: %s",
          s_current_state_buffer);
}

void send_current_state_to_phone() {
  serialize_current_state();

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter) {
    dict_write_cstring(iter, KEY_CURRENT_STATE, s_current_state_buffer);
    app_message_outbox_send();
  }
}

void process_item_updates(const char *json_string) {
  checklist_clear();

  const char *ptr = json_string;

  while (*ptr) {
    // Skip to name field
    ptr = strstr(ptr, "\"name\":");
    if (!ptr)
      break;
    ptr += 7; // Skip "name":

    if (*ptr == '"')
      ptr++; // Skip opening quote

    // Extract name
    char name[MAX_NAME_LENGTH];
    int name_pos = 0;
    while (*ptr && *ptr != '"' && name_pos < MAX_NAME_LENGTH - 1) {
      if (*ptr == '\\' && *(ptr + 1) == '"') {
        name[name_pos++] = '"';
        ptr += 2;
      } else {
        name[name_pos++] = *ptr++;
      }
    }
    name[name_pos] = '\0';

    // Find checked field
    ptr = strstr(ptr, "\"checked\":");
    if (!ptr)
      break;
    ptr += 10; // Skip "checked":

    bool is_checked = (*ptr == '1' || *ptr == 't');

    // Add item to checklist
    if (strlen(name) > 0) {
      checklist_add_items(name);

      // Set checked state if needed
      if (is_checked) {
        int total_items = checklist_get_num_items();
        if (total_items > 0) {
          checklist_item_toggle_checked(total_items - 1);
        }
      }
    }

    // Move to next item
    ptr = strstr(ptr, "}");
    if (!ptr)
      break;
    ptr++;
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Processed item updates");
}

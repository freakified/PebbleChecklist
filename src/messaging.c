#include <pebble.h>
#include "messaging.h"
#include "checklist.h"

static char s_items_to_add_buffer[512];

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
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "clach04 app_message_outbox_size_maximum()=%d", app_message_outbox_size_maximum());
  // just under 8Kb, with 52 * 50 bytes could send entire checklist as one string to phone....

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Watch messaging is started!");
  app_message_register_inbox_received(inbox_received_callback);
}

void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // does this message contain weather information?
  Tuple *items_to_add_tuple = dict_find(iterator, KEY_ITEMS_TO_ADD);

  if(items_to_add_tuple != NULL) {
    strncpy(s_items_to_add_buffer, items_to_add_tuple->value->cstring, sizeof(s_items_to_add_buffer) - 1);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "clach04 inbox value '%s'", s_items_to_add_buffer);
    if (strncmp(s_items_to_add_buffer, "export", sizeof(s_items_to_add_buffer) - 1) == 0)
    {
        ChecklistItem *item=NULL;

        // magic export mode keyword found
        APP_LOG(APP_LOG_LEVEL_DEBUG, "clach04 got magic export mode keyword");

        // index 0 is the bottom of the list, higher numbers are top of list
        for(int id = 0; id < checklist_get_num_items() ; id++) {
            item = checklist_get_item_by_id(id);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "clach04 [%02d] %d %s", id, (int) item->is_checked, item->name);
        }

        // Declare the dictionary's iterator
        DictionaryIterator *out_iter;

        // Prepare the outbox buffer for this message
        AppMessageResult result = app_message_outbox_begin(&out_iter);
        if(result == APP_MSG_OK) {
            // Construct the message
            // using the last item
            DictionaryResult dict_result = dict_write_cstring(out_iter, KEY_ITEMS_TO_ADD, item->name);
            
            if (dict_result == DICT_OK) {
                // Send this message
                result = app_message_outbox_send();

                // Check the result
                if(result != APP_MSG_OK) {
                  APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
                }
            }
            else {
                  APP_LOG(APP_LOG_LEVEL_ERROR, "Error writing cstring to out dict: %d", (int)dict_result);
            }


        } else {
          // The outbox cannot be used right now
          APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
        }
    }
    else
    {
        checklist_add_items(s_items_to_add_buffer);
    }
  }

  // notify the main screen, in case something changed
  message_processed_callback();
}

void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! %d %d %d", reason, APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED);

}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

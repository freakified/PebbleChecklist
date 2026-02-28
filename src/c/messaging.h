#pragma once
#include <pebble.h>

#define KEY_ITEMS_TO_ADD 0
#define KEY_REQUEST_STATE 1
#define KEY_CURRENT_STATE 2
#define KEY_ITEM_UPDATES 3

#ifdef PBL_PLATFORM_APLITE
#define INBOX_SIZE 512
#define OUTBOX_SIZE 512
#else
#define INBOX_SIZE 2048
#define OUTBOX_SIZE 2048
#endif

void messaging_init(void (*message_processed_callback)(void));
void inbox_received_callback(DictionaryIterator *iterator, void *context);
void inbox_dropped_callback(AppMessageResult reason, void *context);
void outbox_failed_callback(DictionaryIterator *iterator,
                            AppMessageResult reason, void *context);
void outbox_sent_callback(DictionaryIterator *iterator, void *context);

// Two-way sync functions
void serialize_current_state();
void send_current_state_to_phone();
void process_item_updates(const char *json_string);

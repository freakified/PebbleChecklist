#pragma once
#include <pebble.h>

#define KEY_ITEMS_TO_ADD 0

void messaging_init(void (*message_processed_callback)(void));
void inbox_received_callback(DictionaryIterator *iterator, void *context);
void inbox_dropped_callback(AppMessageResult reason, void *context);
void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context);
void outbox_sent_callback(DictionaryIterator *iterator, void *context);

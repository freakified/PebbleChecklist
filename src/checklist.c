#include "checklist.h"
#include "util.h"

// persistent storage keys
#define PERSIST_KEY_CHECKLIST_LENGTH       100

// the checklist will occupy storage keys from 200 to 200 + MAX_CHECKLIST_ITEMS
#define PERSIST_KEY_CHECKLIST_ITEM_FIRST  200

static ChecklistItem checklist_items[MAX_CHECKLIST_ITEMS];

static int checklist_length;
static int checklist_num_checked;

void checklist_init() {
  // load checklist information from storage
  checklist_length = persist_read_int(PERSIST_KEY_CHECKLIST_LENGTH);
  checklist_num_checked = 0;

  // load the rest of the checklist
  for(int i = 0; i < MAX_CHECKLIST_ITEMS; i++) {
    persist_read_data(PERSIST_KEY_CHECKLIST_ITEM_FIRST + i, &checklist_items[i], sizeof(ChecklistItem));

    if(checklist_items[i].isChecked) {
      checklist_num_checked++;
    }
  }
}

void save_data_to_storage() {
  // save checklist information
  persist_write_int(PERSIST_KEY_CHECKLIST_LENGTH, checklist_length);

  // save the rest of the checklist
  for(int i = 0; i < MAX_CHECKLIST_ITEMS; i++) {
    persist_write_data(PERSIST_KEY_CHECKLIST_ITEM_FIRST + i, &checklist_items[i], sizeof(ChecklistItem));
  }
}

void checklist_deinit() {
  save_data_to_storage();
}

int checklist_get_num_items() {
  return checklist_length;
}

int checklist_get_num_items_checked() {
  return checklist_num_checked;
}

void checklist_add_items(char* name) {
  // strncpy(line, name, MAX_NAME_LENGTH - 1);
  char line[MAX_NAME_LENGTH], token[MAX_NAME_LENGTH];

  while(name != NULL) {
      name = strwrd(name, token, sizeof(token), ".,");
      checklist_add_item(token);
  }
}

void checklist_add_item(char* name) {
  if(checklist_length < MAX_CHECKLIST_ITEMS) {
    strncpy(checklist_items[checklist_length].name, trim_whitespace(name), MAX_NAME_LENGTH - 1);

    // save the new item to persist
    persist_write_data(PERSIST_KEY_CHECKLIST_ITEM_FIRST + checklist_length, &checklist_items[checklist_length], sizeof(ChecklistItem));

    checklist_length++;
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to add checklist item; list exceeded maximum size.");
  }
}

void checklist_item_toggle_checked(int id) {
  checklist_items[id].isChecked = !(checklist_items[id].isChecked);

  if(checklist_items[id].isChecked) {
    checklist_num_checked++;
  } else {
    checklist_num_checked--;
  }

  // save the edited item to persist
  persist_write_data(PERSIST_KEY_CHECKLIST_ITEM_FIRST + id, &checklist_items[id], sizeof(ChecklistItem));

  // printf("Num items checked: %i, Num items: %i", checklist_get_num_items_checked(), checklist_get_num_items());

}

int checklist_delete_completed_items() {
  // Clear the completed items
  int num_deleted = 0;

  int i = 0;

  while (i < checklist_length) {
    if(checklist_items[i].isChecked) { // is the item checked?
      // delete the item by shuffling the array backwards
      memmove(&checklist_items[i], &checklist_items[i+1], sizeof(checklist_items[0])*(checklist_length - i));

      num_deleted++;
      checklist_length--;
    } else {
      i++;
    }
  }

  checklist_num_checked -= num_deleted;

  // normally, i would save this change to persist immediately, but rewriting the entire
  // persistent store takes too much time
  // save_data_to_storage();

  return num_deleted;
}

ChecklistItem* checklist_get_item_by_id(int id) {
  return &checklist_items[id];
}

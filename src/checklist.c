#include "checklist.h"

// persistent storage keys
#define PERSIST_KEY_CHECKLIST_LENGTH       100
#define PERSIST_KEY_NUM_CHECKED            101

// the checklist will occupy storage keys from 200 to 200 + MAX_CHECKLIST_ITEMS
#define PERSIST_KEY_CHECKLIST_ITEM_FIRST  200

static ChecklistItem checklist_items[MAX_CHECKLIST_ITEMS];

static int checklist_length;
static int checklist_num_checked;

void checklist_init() {
  // load checklist information from storage
  checklist_length = persist_read_int(PERSIST_KEY_CHECKLIST_LENGTH);
  checklist_num_checked = persist_read_int(PERSIST_KEY_NUM_CHECKED);

  // load the rest of the checklist
  for(int i = 0; i < MAX_CHECKLIST_ITEMS; i++) {
    persist_read_data(PERSIST_KEY_CHECKLIST_ITEM_FIRST + i, &checklist_items[i], sizeof(ChecklistItem));
  }
}

void checklist_deinit() {
  // save checklist information
  persist_write_int(PERSIST_KEY_CHECKLIST_LENGTH, checklist_length);
  persist_write_int(PERSIST_KEY_NUM_CHECKED, checklist_num_checked);

  // save the rest of the checklist
  for(int i = 0; i < MAX_CHECKLIST_ITEMS; i++) {
    persist_write_data(PERSIST_KEY_CHECKLIST_ITEM_FIRST + i, &checklist_items[i], sizeof(ChecklistItem));
  }
}

int checklist_get_num_items() {
  return checklist_length;
}

int checklist_get_num_items_checked() {
  return checklist_num_checked;
}

void checklist_add_item(const char* name) {
  if(checklist_length < MAX_CHECKLIST_ITEMS) {
    strncpy(checklist_items[checklist_length].name, name, MAX_NAME_LENGTH - 1);
    checklist_length++;
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to add checklist item; list exceeded maximum size.");
  }
}

void checklist_item_toggle_checked(int id) {

  printf("String is '%s'", checklist_items[id].name);

  checklist_items[id].isChecked = !(checklist_items[id].isChecked);

  if(checklist_items[id].isChecked) {
    checklist_num_checked++;
  } else {
    checklist_num_checked--;
  }
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

  return num_deleted;
}

ChecklistItem* checklist_get_item_by_id(int id) {
  return &checklist_items[id];
}

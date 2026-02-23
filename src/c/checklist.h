#pragma once

#include <pebble.h>
#include <string.h>

#define MAX_NAME_LENGTH     50
#define MAX_CHECKLIST_ITEMS 52

typedef struct ChecklistItem {
  // the name displayed for the item
  char name[MAX_NAME_LENGTH];

  // is the item checked?
  bool is_checked;

  // reserved for future use
  uint8_t sublist_id;
} ChecklistItem;

extern void checklist_init();
extern void checklist_deinit();

/*
 * Returns the total number of checklist items
 */
extern int checklist_get_num_items();

/*
 * Returns the total number of checked items
 */
extern int checklist_get_num_items_checked();

/*
 * Adds one or more items to the list.
 * Each item is identified by splitting the "name" string by a specific character
 */
extern void checklist_add_items(char *name);

/*
 * Toggles whether or not the specified item is checked
 */
extern void checklist_item_toggle_checked(int id);

/*
 * Deletes all completed items from the checklist
 * Returns the number of items that aren't deleted
 */
extern int checklist_delete_completed_items();

/*
 * Clears all items from the checklist
 */
extern void checklist_clear();

/*
 * Returns the checklist item referred to by the given id
 */
extern ChecklistItem* checklist_get_item_by_id(int id);

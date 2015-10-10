#pragma once

#include <pebble.h>

#define MAX_NAME_LENGTH 30

typedef struct ChecklistItem {
  // actual checklist parameters
  int id;
  char name[MAX_NAME_LENGTH];
  bool isChecked;

  // linked list
  struct ChecklistItem* nextItem;
} ChecklistItem;

extern int Checklist_length();
extern void Checklist_addItem(char* name);
extern ChecklistItem* Checklist_getItemById(int id);
extern void Checklist_deleteCompletedItems(int id);

#include "checklist_item.h"

static ChecklistItem* head;
static int length = 0;

int Checklist_length() {
  return length;
}


ChecklistItem* Checklist_getItemById(int id) {
  ChecklistItem* ptr = head;

  while(ptr->nextItem != NULL) {
    if(ptr->id == id) {
      return head;
    }

    ptr = ptr->nextItem;
  }

  return NULL;
}

void Checklist_deleteCompletedItems(int id) {

}

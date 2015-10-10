
#define MAX_NAME_LENGTH 30

typedef struct CheckListItem {
  // actual checklist parameters
  char name[MAX_NAME_LENGTH];
  bool isChecked;

  // linked list
  struct CheckListItem* nextItem;
} CheckListItem;

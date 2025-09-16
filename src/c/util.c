#include "util.h"

bool menu_layer_menu_index_selected(MenuLayer *menu_layer, MenuIndex *index) {
  MenuIndex selected = menu_layer_get_selected_index(menu_layer);
  return selected.row == index->row && selected.section == index->section;
}

/*
 * Returns if a given character counts as whitespace
 */
char is_space(char c) {
  return (c == ' ' || c == '\t' || c == '\n');
}

char *capitalize(char *str) {
  // capitalize the first letter
  (*str) = toupper((int)(*str));

  return str;
}

/*
 * Trims the whitespace from the specified string.
 * From: http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
 */
char* trim_whitespace(char *str) {
  char *end;

  // Trim leading space
  while(is_space(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && is_space(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

/* find the next word starting at 's', delimited by characters
 * in the string 'delim', and store up to 'len' bytes into *buf
 * returns pointer to immediately after the word, or NULL if done.
 * From http://pjd-notes.blogspot.com/2011/09/alternative-to-strtok3-in-c.html
 */
char *strwrd(char *s, char *buf, size_t len, char *delim) {
    s += strspn(s, delim);
    unsigned int n = strcspn(s, delim);  /* count the span (spn) of bytes in */

    if (len-1 < n) {
      n = len-1;
    }              /* the complement (c) of *delim */

    memcpy(buf, s, n);
    buf[n] = 0;
    s += n;
    return (*s == 0) ? NULL : s;
}

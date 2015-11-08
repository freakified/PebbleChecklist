#pragma once

#include <pebble.h>
#include <ctype.h>

bool menu_layer_menu_index_selected(MenuLayer *menu_layer, MenuIndex *index);

// various string handling functions to make up for the lack of a working strtok

char is_space(char c);
char *capitalize(char *str);
char *trim_whitespace(char *str);
char *strwrd(char *s, char *buf, size_t len, char *delim);

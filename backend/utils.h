#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void *safe_realloc(void *__ptr, size_t __size);
void *safe_malloc(size_t __size);

// file/dir operations
bool is_directory(const char *path);
bool path_is_empty_dir(const char *path);
bool launch_folder_dialog(char *output, int max_len);
bool ensure_empty_directory(const char *path);
void load_browser_dir(const char *path, char browser_entries[][256], int *browser_entry_count);

#endif
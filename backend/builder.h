#ifndef BUILDER_H
#define BUILDER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include "utils.h"
#include <linux/limits.h>

#define MAX_TASKS 100

typedef struct t_task {
    char name[256];
    char description[2048];
    int pickrate;
} t_task;

typedef struct n_Folder {
    char name[256];
    float total_task_time;
    int task_quantity;
    char folder_color_hex[8]; // "#RRGGBB" + '\0'
    t_task *tasks[MAX_TASKS]; // Array of task descriptions
} n_Folder;

typedef struct f_Collection {
    n_Folder **folders;
    int folder_count;
    int folder_capacity;
} f_Collection;

f_Collection *build_collection(const char* directory_path, int folder_current_capacity);
void free_collection(f_Collection *collection);

// .data
bool create_data_dir(const char *base_dir);
bool parse_data(const char *base_dir);

// disk
n_Folder *create_folder(const char* folder_name, const char* folder_path);
bool create_category_on_disk(const char *base_path, const char *category_name, float time_val, const char* color_hex);
bool append_task_to_category(const char *base_path, const char *category_name, const char *task_name, int pickrate, const char *description);

char *strremove(char *str, const char *sub);
void parse_tasks(n_Folder *current_folder, const char* full_path, const char* task_file_name);

bool delete_category_on_disk(const char *base_path, const char *category_name);
bool delete_task_on_disk(const char *base_path, const char *category_name, const char *task_name);
bool update_task_description_on_disk(const char *base_path, const char *category_name, const char *task_name, const char *new_desc);

#endif
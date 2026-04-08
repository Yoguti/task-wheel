#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "utils.h"


#define MAX_TASKS 100

typedef struct t_task {
    char description[2048];
    int pickrate;
} t_task;

typedef struct n_Folder {
    char name[256];
    float total_task_time;
    int task_quantity;
    char folder_color_hex[8]; // "#RRGGBB" + '\0'
    char tasks[MAX_TASKS][256]; // Array of task descriptions
} n_Folder;

typedef struct f_Collection {
    n_Folder **folders;
    int folder_count;
    int folder_capacity;
} f_Collection;

f_Collection *build_collection(const char* directory_path, int folder_current_capacity);

n_Folder *create_folder(const char* folder_name, const char* folder_path);

char *strremove(char *str, const char *sub);

void parse_tasks(n_Folder *current_folder, const char* full_path, const char* task_file_name);
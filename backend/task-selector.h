#ifndef TASK_SELECTOR_H
#define TASK_SELECTOR_H

#include "builder.h"

typedef struct t_task_list {
    t_task **tasks;
    int count;
    int capacity;
}t_task_list;

t_task_list *choose_task(f_Collection *collection, const char *directory_path);

void update_pickrate(t_task *task, n_Folder *folder, const char *full_path);

void reset_task_pickrate(t_task *task, n_Folder *folder, const char *full_path);

void reset_folder_pickrate(n_Folder *folder, const char *full_path);

void reset_collection_pickrate_all(f_Collection *collection, const char *directory_path);

#endif
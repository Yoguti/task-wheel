#include "builder.h"

typedef struct t_task_list {
    t_task **tasks;
    int count;
    int capacity;
}t_task_list;

void parse_data(const char *directory_path);

t_task_list *choose_task(f_Collection *collection);
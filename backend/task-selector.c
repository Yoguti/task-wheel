#include "task-selector.h"

/*
void parse_data(const char *directory_path)
{
    DIR *dir = opendir(directory_path);
    if (dir == NULL)
    {
        perror("Unable to open directory");
        return NULL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        const char *temp = strcpy(temp, entry->d_name);
        if (strncmp(temp, ".data", sizeof(".data")))
        {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, temp);
            FILE *file = fopen(full_path, "r");
            if (file == NULL)
            {
                perror("Unable to open task file");
                return;
            }
            char line[256];
            while (fgets(line, sizeof(line), file))
            {
                line[strcspn(line, "\n")] = '\0';
                if (line[0] == '\0') continue;
                switch (line[0]) {
                case '"': break;
                default: break;
                }
            }
        }
    }   
}
*/
t_task_list *choose_task(f_Collection *collection) {
    t_task_list *tasklist = (t_task_list *)safe_malloc(sizeof(t_task_list));
    tasklist->capacity = 10;
    tasklist->count = 0;    
    tasklist->tasks = (t_task **)safe_malloc(sizeof(t_task *) * tasklist->capacity);

    for(int i = 0; i < collection->folder_count; i++) {
        n_Folder *f = collection->folders[i];
        if (f->task_quantity == 0) continue;

        t_task *t = f->tasks[0];

        for (int j = 0; j < f->task_quantity; j++) {
            t_task *looptask = f->tasks[j];

            if (looptask->pickrate < t->pickrate) {
                t = looptask;
            } else if (looptask->pickrate == t->pickrate) {
                t = (rand() % 2 == 0) ? t : looptask;
            }
        }

        tasklist->tasks[tasklist->count++] = t;
    }
    return tasklist;
}

int main() {
    f_Collection *col = build_collection("../collections", 5);
    if (!col) return 1;

    t_task_list *chosen_tasks = choose_task(col);
    if (chosen_tasks == NULL || chosen_tasks->count == 0) {
        fprintf(stderr, "No task chosen.\n");
        return 1;
    }

    printf("===== CHOSEN TASKS (by folder) =====\n");

    int chosen_index = 0;

    for (int i = 0; i < col->folder_count; i++) {
        n_Folder *f = col->folders[i];
        if (f->task_quantity == 0) continue;

        t_task *chosen = chosen_tasks->tasks[chosen_index++];

        printf("\n[%d] %s\n", i + 1, f->name);
        for (int j = 0; j < f->task_quantity; j++) {
            t_task *task = f->tasks[j];
            if (task == chosen) {
                printf("  -> %s\n", task->name);
                printf("     Rate: %d\n", task->pickrate);
                printf("     Desc: %s\n", task->description);
            } else {
                printf("     %s (Rate: %d, Desc: %s)\n", task->name, task->pickrate, task->description);
            }
        }
    }

    printf("\n==========================================\n");


    free(chosen_tasks->tasks);
    free(chosen_tasks);

    for (int i = 0; i < col->folder_count; i++) {
        n_Folder *f = col->folders[i];
        for (int j = 0; j < f->task_quantity; j++) {
            free(f->tasks[j]);
        }
        free(f);
    }
    free(col->folders);
    free(col);

    return 0;
}
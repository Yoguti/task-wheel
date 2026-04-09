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
t_task_list *choose_task(f_Collection *collection, const char *directory_path) {
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
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, f->name);
        update_pickrate(t, f, full_path);
    }
    return tasklist;
}

void update_pickrate(t_task *task, n_Folder *folder, const char *full_path) {
    task->pickrate += 1;
    
    char task_file_path[512];
    snprintf(task_file_path, sizeof(task_file_path), "%s/%s.txt", full_path, folder->name);
    
    FILE *file = fopen(task_file_path, "r");
    if (file == NULL) {
        perror("Unable to open task file");
        return;
    }
    
    // Read all lines
    char lines[MAX_TASKS][512];
    int line_count = 0;
    char line[512];
    
    while (fgets(line, sizeof(line), file) && line_count < MAX_TASKS) {
        strcpy(lines[line_count], line);
        line_count++;
    }
    fclose(file);
    // Find and update the task line
    for (int i = 0; i < line_count; i++) {
        if (lines[i][0] == '"') {
            char *p = lines[i] + 1; // skip opening quote
            int name_len = strcspn(p, "\"");
            if (name_len < (int)strlen(task->name) || strncmp(p, task->name, name_len) != 0) {
                continue;
            }
            p += name_len + 1; // skip closing quote
            if (*p != '(') continue; // malformed
            p++; // skip (
            int paren_len = strcspn(p, ")");
            if (p[paren_len] != ')') continue; // malformed
            p += paren_len + 1; // skip )
            if (strncmp(p, "->", 2) != 0) continue; // malformed
            p += 2; // skip ->
            // p now points to the description
            char description[512];
            strncpy(description, p, sizeof(description) - 1);
            description[sizeof(description) - 1] = '\0';
            // Reconstruct the line with updated pickrate
            int len = snprintf(lines[i], sizeof(lines[i]), "\"%s\"(%d)->", task->name, task->pickrate);
            if (len > 0 && len < (int)sizeof(lines[i])) {
                strncat(lines[i], description, sizeof(lines[i]) - len - 1);
            }
            break;
        }
    }
    
    // Write the file back
    file = fopen(task_file_path, "w");
    if (file == NULL) {
        perror("Unable to open task file for writing");
        return;
    }
    
    for (int i = 0; i < line_count; i++) {
        fprintf(file, "%s", lines[i]);
    }
    fclose(file);
}

void reset_task_pickrate(t_task *task, n_Folder *folder, const char *full_path) {
    if (task == NULL || folder == NULL) return;
    task->pickrate = 0;
    
    char task_file_path[512];
    snprintf(task_file_path, sizeof(task_file_path), "%s/%s.txt", full_path, folder->name);
    
    FILE *file = fopen(task_file_path, "r");
    if (file == NULL) {
        perror("Unable to open task file");
        return;
    }
    
    // Read all lines
    char lines[MAX_TASKS][512];
    int line_count = 0;
    char line[512];
    
    while (fgets(line, sizeof(line), file) && line_count < MAX_TASKS) {
        strcpy(lines[line_count], line);
        line_count++;
    }
    fclose(file);
    
    // Find and reset the task line
    for (int i = 0; i < line_count; i++) {
        if (lines[i][0] == '"') {
            char *p = lines[i] + 1; // skip opening quote
            int name_len = strcspn(p, "\"");
            if (name_len < (int)strlen(task->name) || strncmp(p, task->name, name_len) != 0) {
                continue;
            }
            p += name_len + 1; // skip closing quote
            if (*p != '(') continue; // malformed
            p++; // skip (
            int paren_len = strcspn(p, ")");
            if (p[paren_len] != ')') continue; // malformed
            p += paren_len + 1; // skip )
            if (strncmp(p, "->", 2) != 0) continue; // malformed
            p += 2; // skip ->
            // p now points to the description
            char description[512];
            strncpy(description, p, sizeof(description) - 1);
            description[sizeof(description) - 1] = '\0';
            // Reconstruct the line with reset pickrate (0)
            int len = snprintf(lines[i], sizeof(lines[i]), "\"%s\"(%d)->", task->name, 0);
            if (len > 0 && len < (int)sizeof(lines[i])) {
                strncat(lines[i], description, sizeof(lines[i]) - len - 1);
            }
            break;
        }
    }
    
    // Write the file back
    file = fopen(task_file_path, "w");
    if (file == NULL) {
        perror("Unable to open task file for writing");
        return;
    }
    
    for (int i = 0; i < line_count; i++) {
        fprintf(file, "%s", lines[i]);
    }
    fclose(file);
}

void reset_folder_pickrate(n_Folder *folder, const char *full_path) {
    if (folder == NULL) return;
    for (int i = 0; i < folder->task_quantity; i++) {
        reset_task_pickrate(folder->tasks[i], folder, full_path);
    }
}

void reset_collection_pickrate_all(f_Collection *collection, const char *directory_path) {
    if (collection == NULL) return;
    for (int i = 0; i < collection->folder_count; i++) {
        n_Folder *f = collection->folders[i];
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, f->name);
        reset_folder_pickrate(f, full_path);
    }
}

int main() {
    f_Collection *col = build_collection("../collections", 5);
    if (!col) return 1;

    t_task_list *chosen_tasks = choose_task(col, "../collections");
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
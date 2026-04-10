#include "builder.h"
#include <unistd.h>
#include <string.h>

f_Collection *build_collection(const char *directory_path, int folder_current_capacity)
{
    f_Collection *collection = (f_Collection *)safe_malloc(sizeof(f_Collection));

    collection->folder_count = 0;
    collection->folder_capacity = folder_current_capacity;
    collection->folders = (n_Folder **)safe_malloc(sizeof(n_Folder *) * collection->folder_capacity);

    // run trough the directory and for each folder
    // create a new n_Folder and add it to the collection
    DIR *dir = opendir(directory_path);
    if (dir == NULL)
    {
        perror("Unable to open directory");
        free(collection->folders);
        free(collection);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".gitkeep") != 0 && (strcmp(entry->d_name, ".data") != 0))
        {

            if (collection->folder_count == collection->folder_capacity)
            {
                collection->folder_capacity *= 2;
                collection->folders =
                    (n_Folder **)safe_realloc(collection->folders, collection->folder_capacity * sizeof(n_Folder *));
            }
            // full_path = directory_path + "/" + entry->d_name
            n_Folder *curr = create_folder(entry->d_name, directory_path);
            if (curr != NULL)
            {
                collection->folders[collection->folder_count++] = curr;
            }
        }
    }
    closedir(dir);
    return collection;
}

static char *xstrdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *dup = safe_malloc(len);
    if (dup == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(dup, str, len);
    return dup;
}

n_Folder *create_folder(const char *folder_name, const char *folder_path)
{
    n_Folder *folder = (n_Folder *)safe_malloc(sizeof(n_Folder));
    strncpy(folder->name, folder_name, sizeof(folder->name) - 1);
    folder->name[sizeof(folder->name) - 1] = '\0';
    folder->total_task_time = 0;
    folder->task_quantity = 0;
    strncpy(folder->folder_color_hex, "#FFFFFF", sizeof(folder->folder_color_hex) - 1);
    folder->folder_color_hex[sizeof(folder->folder_color_hex) - 1] = '\0';
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, folder_name);
    DIR *dir = opendir(full_path);
    if (dir == NULL)
    {
        perror("Unable to open directory");
        free(folder);
        return NULL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0)
        {
            char *temp = xstrdup(entry->d_name);
            char *task_name = strremove(temp, ".txt");
            if (strcmp(task_name, folder->name) == 0)
            {
                parse_tasks(folder, full_path, entry->d_name);
            }
            free(temp);
        }
    }
    closedir(dir);
    return folder;
}

char *strremove(char *str, const char *sub)
{
    size_t len = strlen(sub);
    if (len > 0)
    {
        char *p = str;
        while ((p = strstr(p, sub)) != NULL)
        {
            memmove(p, p + len, strlen(p + len) + 1);
        }
    }
    return str;
}

void parse_tasks(n_Folder *current_folder, const char *full_path, const char *task_file_name)
{
    char task_file_path[512];
    snprintf(task_file_path, sizeof(task_file_path), "%s/%s", full_path, task_file_name);
    FILE *file = fopen(task_file_path, "r");
    if (file == NULL)
    {
        perror("Unable to open task file");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        if (current_folder->task_quantity == MAX_TASKS)
        {
            fprintf(stderr, "Maximum task quantity reached for folder: %s\n", current_folder->name);
            break;
        }
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0')
            continue;
        switch (line[0])
        {         // using &line[1] to skip the identifier
        case '"': // Line begins with ", indicating a task
            // Allocate memory for the new task
            current_folder->tasks[current_folder->task_quantity] = (t_task *)safe_malloc(sizeof(t_task));
            char *p = line;
            // Skip opening quote
            p++;
            int i = strcspn(p, "\"");
            if (p[i] == '\0') break; // malformed
            p[i] = '\0';
            // Copy task name
            strncpy(current_folder->tasks[current_folder->task_quantity]->name, p, sizeof(current_folder->tasks[current_folder->task_quantity]->name) - 1);
            current_folder->tasks[current_folder->task_quantity]->name[sizeof(current_folder->tasks[current_folder->task_quantity]->name) - 1] = '\0';
            // Move past null terminator
            p += i + 1;
            p++; // skip ( after task name
            i = strcspn(p, ")");
            if (p[i] == '\0') break; // malformed
            p[i] = '\0';
            // Copy task pickrate
            current_folder->tasks[current_folder->task_quantity]->pickrate = atoi(p);
            // Move past null terminator and arrow body "-""
            p += i + 2;
            i = strcspn(p, ">");
            if (p[i] == '\0') break; // malformed
            // Copy task description
            p += i + 1; // skip past ">"
            strncpy(current_folder->tasks[current_folder->task_quantity]->description, p, sizeof(current_folder->tasks[current_folder->task_quantity]->description) - 1);
            current_folder->tasks[current_folder->task_quantity]->description[sizeof(current_folder->tasks[current_folder->task_quantity]->description) - 1] = '\0';
            current_folder->task_quantity++;
            break;

        case '#': // Line begins with #, indicating a color
            strncpy(current_folder->folder_color_hex, &line[1], sizeof(current_folder->folder_color_hex) - 1);
            current_folder->folder_color_hex[sizeof(current_folder->folder_color_hex) - 1] = '\0';
            break;

        case '@': // Line begins with a @, indicating task time
            current_folder->total_task_time += atof(&line[1]);
            break;
        default:
            break;
        }
    }
    fclose(file);
}

bool update_task_description_on_disk(const char *base_path, const char *category_name, const char *task_name, const char *new_desc) {
    char category_file[PATH_MAX + 256];
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    FILE *file = fopen(category_file, "r");
    if (!file) return false;

    char lines[512][1024];
    int count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), file) && count < 512) {
        strcpy(lines[count++], line);
    }
    fclose(file);

    file = fopen(category_file, "w");
    if (!file) return false;

    for (int i = 0; i < count; i++) {
        if (lines[i][0] == '"') {
            char temp[1024];
            strcpy(temp, lines[i] + 1);
            char *name_end = strchr(temp, '"');
            if (name_end) {
                *name_end = '\0';
                if (strcmp(temp, task_name) == 0) {
                    char *p = name_end + 1;
                    p++; // Pula '('
                    int pickrate = atoi(p);
                    fprintf(file, "\"%s\"(%d)->%s\n", task_name, pickrate, new_desc ? new_desc : "");
                    continue;
                }
            }
        }
        fprintf(file, "%s", lines[i]);
    }
    fclose(file);
    return true;
}

bool delete_category_on_disk(const char *base_path, const char *category_name) {
    char category_file[512];
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    if (access(category_file, F_OK) == 0) {
        remove(category_file);
    }
    char category_path[512];
    snprintf(category_path, sizeof(category_path), "%s/%s", base_path, category_name);
    if (rmdir(category_path) != 0) return false;
    return true;
}

bool delete_task_on_disk(const char *base_path, const char *category_name, const char *task_name) {
    char category_file[512];
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    FILE *file = fopen(category_file, "r");
    if (!file) return false;

    char lines[512][1024];
    int count = 0;
    char line[1024];
    bool deleted = false;
    while (fgets(line, sizeof(line), file) && count < 512) {
        if (line[0] == '"') {
            char temp[1024];
            strcpy(temp, line + 1);
            char *name_end = strchr(temp, '"');
            if (name_end) {
                *name_end = '\0';
                if (strcmp(temp, task_name) == 0) {
                    deleted = true;
                    continue;
                }
            }
        }
        strcpy(lines[count++], line);
    }
    fclose(file);

    if (!deleted) return false;

    file = fopen(category_file, "w");
    if (!file) return false;
    for (int i = 0; i < count; i++) {
        fputs(lines[i], file);
    }
    fclose(file);
    return true;
}

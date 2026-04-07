#include "builder.h"

f_Collection *build_collection(const char* directory_path, int folder_current_capacity) {
    f_Collection *collection = (f_Collection*)safe_malloc(sizeof(f_Collection));

    collection->folder_count = 0;
    collection->folder_capacity = folder_current_capacity;
    collection->folders = (n_Folder**)safe_malloc(sizeof(n_Folder*) * collection->folder_capacity);

    // run trough the directory and for each folder
    // create a new n_Folder and add it to the collection
    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        free(collection->folders);
        free(collection);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 &&
    strcmp(entry->d_name, "..") != 0) {

        if (collection->folder_count == collection->folder_capacity) {
            collection->folder_capacity *= 2;
            collection->folders = 
            (n_Folder**)safe_realloc(collection->folders, collection->folder_capacity * sizeof(n_Folder*));
        }
        // full_path = directory_path + "/" + entry->d_name
        n_Folder *curr = create_folder(entry->d_name, directory_path);
        collection->folders[collection->folder_count] = curr;
        collection->folder_count++;

    }
    }
    closedir(dir);
    return collection;
}

n_Folder *create_folder(const char* folder_name, const char* folder_path) {
    n_Folder *folder = (n_Folder*)safe_malloc(sizeof(n_Folder));
    strncpy(folder->name, folder_name, sizeof(folder->name) - 1);
    folder->name[sizeof(folder->name) - 1] = '\0';
    folder->total_task_time = 0;
    folder->task_quantity = 0;
    strncpy(folder->folder_color_hex, "#FFFFFF", sizeof(folder->folder_color_hex) - 1);
    folder->folder_color_hex[sizeof(folder->folder_color_hex) - 1] = '\0';
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, folder_name);
    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        free(folder);
        return NULL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0) {
            char *temp = strdup(entry->d_name);
            char *task_name = strremove(temp, ".txt");
            if (strcmp(task_name, folder->name) == 0) {
                parse_tasks(folder, full_path, entry->d_name);
            }
            free(temp);
        }
    }    
    closedir(dir);
    return folder;
}

char *strremove(char *str, const char *sub) {
    size_t len = strlen(sub);
    if (len > 0) {
        char *p = str;
        while ((p = strstr(p, sub)) != NULL) {
            memmove(p, p + len, strlen(p + len) + 1);
        }
    }
    return str;
}

void parse_tasks(n_Folder *current_folder, const char* full_path, const char* task_file_name) {
    char task_file_path[512];
    snprintf(task_file_path, sizeof(task_file_path), "%s/%s", full_path, task_file_name);
    FILE *file = fopen(task_file_path, "r");
    if (file == NULL) {
        perror("Unable to open task file");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        strncpy(current_folder->tasks[current_folder->task_quantity], line, sizeof(current_folder->tasks[current_folder->task_quantity]) - 1);
        current_folder->tasks[current_folder->task_quantity][sizeof(current_folder->tasks[current_folder->task_quantity]) - 1] = '\0';
        current_folder->task_quantity++;
    }
    
    fclose(file);
}

int main() {
    f_Collection *col = build_collection("./collections", 5);

    for (size_t i = 0; i < col->folder_count; i++)
    {
        printf("Folder Name: %s\n", col->folders[i]->name);
        printf("Total Task Time: %.2f\n", col->folders[i]->total_task_time);
        printf("Task Quantity: %d\n", col->folders[i]->task_quantity);
        printf("Folder Color: %s\n", col->folders[i]->folder_color_hex);
        printf("Tasks:\n");
        for (size_t j = 0; j < col->folders[i]->task_quantity; j++)
        {            printf("  - %s", col->folders[i]->tasks[j]);
        }
    }

    for (int i = 0; i < col->folder_count; i++) {
    free(col->folders[i]);}
    free(col->folders);
    free(col);
    return 0;

}
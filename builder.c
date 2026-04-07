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
            char *task_name = strremove(temp, ".json");
            if (strcmp(task_name, folder->name) == 0) {
                parse_tasks(*folder, full_path, entry->d_name);
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

int main() {
    f_Collection *col = build_collection("./collections", 5);

    for (size_t i = 0; i < col->folder_count; i++)
    {
        printf("Folder Name: %s\n", col->folders[i]->name);
    }

    for (int i = 0; i < col->folder_count; i++) {
    free(col->folders[i]);}
    free(col->folders);
    free(col);
    return 0;

}
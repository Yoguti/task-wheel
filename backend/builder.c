#include "builder.h"
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

f_Collection *build_collection(const char *directory_path, int folder_current_capacity)
{  
    bool has_data = false;
    f_Collection *collection = (f_Collection *)safe_malloc(sizeof(f_Collection));

    collection->folder_count = 0;
    collection->folder_capacity = folder_current_capacity;
    collection->folders = (n_Folder **)safe_malloc(sizeof(n_Folder *) * collection->folder_capacity);

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
            strcmp(entry->d_name, "..") != 0 && 
            strcmp(entry->d_name, ".gitkeep") != 0 && 
            strcmp(entry->d_name, ".data") != 0)
        {
            if (collection->folder_count == collection->folder_capacity) {
                collection->folder_capacity *= 2;
                collection->folders = (n_Folder **)safe_realloc(collection->folders, collection->folder_capacity * sizeof(n_Folder *));
            }
            n_Folder *curr = create_folder(entry->d_name, directory_path);
            if (curr != NULL) collection->folders[collection->folder_count++] = curr;
        }
        if (strcmp(entry->d_name, ".data") == 0) has_data = true;
    }
    closedir(dir);

    if (!has_data) create_data_dir(directory_path);
    parse_data(directory_path);

    return collection;
}

void free_collection(f_Collection *collection) {
    if (collection == NULL) return;
    for (int i = 0; i < collection->folder_count; i++) {
        n_Folder *folder = collection->folders[i];
        for (int j = 0; j < folder->task_quantity; j++) {
            free(folder->tasks[j]);
        }
        free(folder);
    }
    free(collection->folders);
    free(collection);
}

bool create_data_dir(const char *base_dir) {
    char path[PATH_MAX + 128];
    snprintf(path, sizeof(path), "%s/.data", base_dir);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return false;

    const char *files[] = {"taskwheel", "statistics", "calendar", "current"};
    for (int i = 0; i < 4; i++) {
        char filepath[PATH_MAX + 256];
        snprintf(filepath, sizeof(filepath), "%s/%s", path, files[i]);
        FILE *f = fopen(filepath, "a+"); 
        if (f) fclose(f);
    }
    return true;
}

bool parse_data(const char *base_dir) {
    char path[PATH_MAX + 128];
    snprintf(path, sizeof(path), "%s/.data", base_dir);
    
    char current_file[PATH_MAX + 256];
    snprintf(current_file, sizeof(current_file), "%s/current", path);
    FILE *fc = fopen(current_file, "r");
    if (fc) fclose(fc);

    char calendar_file[PATH_MAX + 256];
    snprintf(calendar_file, sizeof(calendar_file), "%s/calendar", path);
    
    char stats_file[PATH_MAX + 256];
    snprintf(stats_file, sizeof(stats_file), "%s/statistics", path);
    
    char tw_file[PATH_MAX + 256];
    snprintf(tw_file, sizeof(tw_file), "%s/taskwheel", path);

    return true;
}

static char *xstrdup(const char *str) {
    size_t len = strlen(str) + 1;
    char *dup = safe_malloc(len);
    memcpy(dup, str, len);
    return dup;
}

n_Folder *create_folder(const char *folder_name, const char *folder_path) {
    n_Folder *folder = (n_Folder *)safe_malloc(sizeof(n_Folder));
    strncpy(folder->name, folder_name, sizeof(folder->name) - 1);
    folder->name[sizeof(folder->name) - 1] = '\0';
    folder->total_task_time = 0;
    folder->task_quantity = 0;
    strncpy(folder->folder_color_hex, "#FFFFFF", sizeof(folder->folder_color_hex) - 1);
    folder->folder_color_hex[sizeof(folder->folder_color_hex) - 1] = '\0';
    
    char full_path[PATH_MAX + 256];
    snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, folder_name);
    DIR *dir = opendir(full_path);
    if (dir == NULL) { free(folder); return NULL; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char *temp = xstrdup(entry->d_name);
            char *task_name = strremove(temp, ".txt");
            if (strcmp(task_name, folder->name) == 0) parse_tasks(folder, full_path, entry->d_name);
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
        while ((p = strstr(p, sub)) != NULL) memmove(p, p + len, strlen(p + len) + 1);
    }
    return str;
}

void parse_tasks(n_Folder *current_folder, const char *full_path, const char *task_file_name) {
    char task_file_path[PATH_MAX + 512];
    snprintf(task_file_path, sizeof(task_file_path), "%s/%s", full_path, task_file_name);
    FILE *file = fopen(task_file_path, "r");
    if (file == NULL) return;
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        if (current_folder->task_quantity == MAX_TASKS) break;
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;
        switch (line[0]) {         
        case '"': 
            current_folder->tasks[current_folder->task_quantity] = (t_task *)safe_malloc(sizeof(t_task));
            char *p = line + 1;
            int i = strcspn(p, "\"");
            if (p[i] == '\0') break; 
            p[i] = '\0';
            strncpy(current_folder->tasks[current_folder->task_quantity]->name, p, 255);
            p += i + 2; p++; 
            i = strcspn(p, ")");
            p[i] = '\0';
            current_folder->tasks[current_folder->task_quantity]->pickrate = atoi(p);
            p += i + 2;
            i = strcspn(p, ">");
            p += i + 1; 
            strncpy(current_folder->tasks[current_folder->task_quantity]->description, p, 2047);
            current_folder->task_quantity++;
            break;
        case '#': 
            strncpy(current_folder->folder_color_hex, &line[1], 7);
            break;
        case '@': 
            current_folder->total_task_time += atof(&line[1]);
            break;
        }
    }
    fclose(file);
}

bool create_category_on_disk(const char *base_path, const char *category_name, float time_val, const char* color_hex) {
    if (category_name == NULL || category_name[0] == '\0') return false;
    char category_path[PATH_MAX + 512]; 
    snprintf(category_path, sizeof(category_path), "%s/%s", base_path, category_name);
    if (mkdir(category_path, 0755) != 0) return false;
    
    char category_file[PATH_MAX + 512]; 
    if (snprintf(category_file, sizeof(category_file), "%s/%s.txt", category_path, category_name) < 0) return false;
    
    FILE *file = fopen(category_file, "w"); 
    if (file == NULL) return false;
    fprintf(file, "@%.2f\n%s\n", time_val, color_hex); 
    fclose(file);
    return true;
}

bool append_task_to_category(const char *base_path, const char *category_name, const char *task_name, int pickrate, const char *description) {
    if (task_name == NULL || task_name[0] == '\0') return false;
    char category_file[PATH_MAX + 512]; 
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    
    FILE *file = fopen(category_file, "a"); 
    if (file == NULL) return false;
    fprintf(file, "\"%s\"(%d)->%s\n", task_name, pickrate, description ? description : ""); 
    fclose(file);
    return true;
}

bool update_task_description_on_disk(const char *base_path, const char *category_name, const char *task_name, const char *new_desc) {
    char category_file[PATH_MAX + 512];
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    FILE *file = fopen(category_file, "r");
    if (!file) return false;
    char lines[512][2048]; int count = 0; char line[2048];
    while (fgets(line, sizeof(line), file) && count < 512) strcpy(lines[count++], line);
    fclose(file);
    file = fopen(category_file, "w");
    if (!file) return false;
    for (int i = 0; i < count; i++) {
        if (lines[i][0] == '"') {
            char temp[2048]; strcpy(temp, lines[i] + 1);
            char *name_end = strchr(temp, '"');
            if (name_end) {
                *name_end = '\0';
                if (strcmp(temp, task_name) == 0) {
                    char *p = name_end + 2;
                    int pickrate = atoi(p);
                    fprintf(file, "\"%s\"(%d)->%s\n", task_name, pickrate, new_desc ? new_desc : "");
                    continue;
                }
            }
        }
        fprintf(file, "%s", lines[i]);
    }
    fclose(file); return true;
}

bool delete_category_on_disk(const char *base_path, const char *category_name) {
    char category_file[PATH_MAX + 512];
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    remove(category_file);
    char category_path[PATH_MAX + 512];
    snprintf(category_path, sizeof(category_path), "%s/%s", base_path, category_name);
    return rmdir(category_path) == 0;
}

bool delete_task_on_disk(const char *base_path, const char *category_name, const char *task_name) {
    char category_file[PATH_MAX + 512];
    snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    FILE *file = fopen(category_file, "r");
    if (!file) return false;
    char lines[512][2048]; int count = 0; char line[2048]; bool deleted = false;
    while (fgets(line, sizeof(line), file) && count < 512) {
        if (line[0] == '"') {
            char temp[2048]; strcpy(temp, line + 1);
            char *name_end = strchr(temp, '"');
            if (name_end) {
                *name_end = '\0';
                if (strcmp(temp, task_name) == 0) { deleted = true; continue; }
            }
        }
        strcpy(lines[count++], line);
    }
    fclose(file);
    if (!deleted) return false;
    file = fopen(category_file, "w");
    for (int i = 0; i < count; i++) fputs(lines[i], file);
    fclose(file); return true;
}
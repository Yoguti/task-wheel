#define _POSIX_C_SOURCE 200809L

#include "utils.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>

void *safe_realloc(void *__ptr, size_t __size) {
    void *ptr = realloc(__ptr, __size);
    if (ptr == NULL) {
        fprintf(stderr, "Failed to reallocate memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *safe_malloc(size_t __size) {
    void *ptr = malloc(__size);
    if (ptr == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

bool is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool path_is_empty_dir(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) return false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}

bool launch_folder_dialog(char *output, int max_len) {
    const char *cmd = "zenity --file-selection --directory --create-directory --title=\"Select or create collection folder\"";
    FILE *pipe = popen(cmd, "r");
    if (pipe == NULL) return false;
    if (fgets(output, max_len, pipe) == NULL) {
        pclose(pipe);
        return false;
    }
    int status = pclose(pipe);
    if (status != 0) return false;
    output[strcspn(output, "\n")] = '\0';
    if (output[0] == '\0') return false;
    return true;
}

bool ensure_empty_directory(const char *path) {
    if (path == NULL || path[0] == '\0') return false;
    struct stat st;
    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) return false;
        return path_is_empty_dir(path);
    }
    if (mkdir(path, 0755) != 0) return false;
    return true;
}

void load_browser_dir(const char *path, char browser_entries[][256], int *browser_entry_count) {
    DIR *dir = opendir(path);
    if (!dir) return;
    
    *browser_entry_count = 0;
    strcpy(browser_entries[(*browser_entry_count)++], "..");
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *browser_entry_count < 256) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char full[PATH_MAX + 256];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        if (is_directory(full)) {
            strncpy(browser_entries[(*browser_entry_count)++], entry->d_name, 255);
        }
    }
    closedir(dir);
    
    // Sort
    for (int i = 1; i < *browser_entry_count - 1; i++) {
        for (int j = i + 1; j < *browser_entry_count; j++) {
            if (strcmp(browser_entries[i], browser_entries[j]) > 0) {
                char temp[256];
                strcpy(temp, browser_entries[i]);
                strcpy(browser_entries[i], browser_entries[j]);
                strcpy(browser_entries[j], temp);
            }
        }
    }
}
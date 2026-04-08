#include "builder.h"

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

int main() {
    f_Collection *col = build_collection("../collections", 5);
    if (!col) return 1;

    printf("===== COLLECTION =====\n");

    for (int i = 0; i < col->folder_count; i++) {
        n_Folder *f = col->folders[i];

        printf("\n[%d] %s\n", i + 1, f->name);
        printf(" Time   : %.1f h\n", f->total_task_time);
        printf(" Tasks  : %d\n", f->task_quantity);
        printf(" Color  : %s\n", f->folder_color_hex);

        printf("\n--- TASKS ---\n");

        for (int j = 0; j < f->task_quantity; j++) {
            printf("  (%d) %s\n", j + 1, f->tasks[j]);
        }
            printf("\n==========================================\n");

    }


    for (int i = 0; i < col->folder_count; i++) {
        free(col->folders[i]);
    }
    free(col->folders);
    free(col);

    return 0;
}
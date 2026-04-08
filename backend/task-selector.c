#include "builder.h"

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
#include "builder.h"


int main() {
    f_Collection *col = build_collection("../collections", 5);

    for (int i = 0; i < col->folder_count; i++)
    {
        printf("\nFolder Name: %s\n", col->folders[i]->name);
        printf("Total Task Time: %.1f h\n", col->folders[i]->total_task_time);
        printf("Task Quantity: %d\n", col->folders[i]->task_quantity);
        printf("Folder Color: %s\n", col->folders[i]->folder_color_hex);
        printf("Tasks:\n\n");
        for (int j = 0; j < col->folders[i]->task_quantity; j++)
        {            printf("  - %s", col->folders[i]->tasks[j]);
        }
    }

    for (int i = 0; i < col->folder_count; i++) {
    free(col->folders[i]);}
    free(col->folders);
    free(col);
    return 0;

}
#include <raylib.h>
#include <time.h>
#include "controler.h"
#include "backend/builder.h"
#include "frontend/interface.h"

int main() {
    // Inicialização do RNG para a aleatoriedade do pickrate/roleta
    srand(time(NULL));

    f_Collection *collection = NULL;

    InitWindow(1280, 720, "Task Wheel");
    SetTargetFPS(60);

    AppState appState = { 0 };
    appState.screen = SCREEN_COLLECTION_SELECT;
    appState.state = WHEEL_IDLE;
    appState.rotation_angle = 0.0f;
    appState.spin_velocity = 0.0f;
    appState.chosen_tasks = NULL;
    appState.selected_display_index = -1;
    appState.confirm_reset_all = false;
    appState.confirm_reset_task = false;

    // Loop principal da Raylib
    while (!WindowShouldClose()) {
        UpdateDrawFrame(&collection, &appState);
    }

    CloseWindow();

    // Limpeza de memória
    if (appState.chosen_tasks) {
        free(appState.chosen_tasks->tasks);
        free(appState.chosen_tasks);
    }

    if (collection) {
        for (int i = 0; i < collection->folder_count; i++) {
            n_Folder *f = collection->folders[i];
            for (int j = 0; j < f->task_quantity; j++) {
                free(f->tasks[j]);
            }
            free(f);
        }
        free(collection->folders);
        free(collection);
    }

    return 0;
}
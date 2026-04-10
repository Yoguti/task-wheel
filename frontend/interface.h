#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>
#include <raylib.h>
#include "../backend/task-selector.h"

typedef enum {
    SCREEN_COLLECTION_SELECT,
    SCREEN_WHEEL
} AppScreen;

typedef enum {
    WHEEL_IDLE,
    WHEEL_SPINNING,
    WHEEL_FINISHED
} WheelState;

typedef struct {
    AppScreen screen;
    WheelState state;
    float rotation_angle;
    float spin_velocity;
    t_task_list *chosen_tasks;
    int selected_display_index;
    bool detail_view;
    bool confirm_reset_all;
    bool confirm_reset_task;
} AppState;

void UpdateDrawFrame(f_Collection **collection, AppState *appState);

#endif
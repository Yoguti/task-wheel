#define _POSIX_C_SOURCE 200809L

#include "interface.h"
#include <raymath.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern char *realpath(const char *path, char *resolved_path);

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WHEEL_RADIUS 260.0f
#define CENTER_RADIUS 80.0f

Color HexToColor(const char *hex) {
    Color color = { 200, 200, 200, 255 }; 
    if (hex == NULL) return color;
    if (hex[0] == '#') hex++; 
    if (strlen(hex) < 6) return color; 
    
    int rgb = (int)strtol(hex, NULL, 16);
    color.r = (rgb >> 16) & 0xFF;
    color.g = (rgb >> 8) & 0xFF;
    color.b = rgb & 0xFF;
    return color;
}

void TruncateText(const char *text, char *buffer, int max_len) {
    if ((int)strlen(text) > max_len) {
        strncpy(buffer, text, max_len - 3);
        buffer[max_len - 3] = '\0';
        strcat(buffer, "...");
    } else {
        strcpy(buffer, text);
    }
}

// Helper para desenhar sombras arredondadas
void DrawShadow(Rectangle rect, float roundness, int segments, float offset) {
    Rectangle shadowRect = { rect.x + offset, rect.y + offset, rect.width, rect.height };
    DrawRectangleRounded(shadowRect, roundness, segments, Fade(BLACK, 0.08f));
}

static const char* palette_hex[] = {
    "#EF476F", "#F78C6B", "#FFD166", "#06D6A0", "#118AB2", "#073B4C",
    "#9D4EDD", "#8338EC", "#3A0CA3", "#3F37C9", "#4361EE", "#48CAE4",
    "#F94144", "#F3722C", "#F8961E", "#F9C74F", "#90BE6D", "#43AA8B"
};
#define PALETTE_SIZE 18

typedef enum { EDIT_LIST, EDIT_ADD_CAT, EDIT_ADD_TASK, EDIT_DEL_CAT_CONFIRM, EDIT_DEL_TASK_CONFIRM } EditState;

static char current_collection_path[PATH_MAX] = "";
static char recent_collections[5][PATH_MAX];
static int recent_count = 0;
static bool history_loaded = false;

static bool show_browser = false;
static char browser_path[PATH_MAX] = "";
static bool browser_path_active = false;
static char browser_entries[256][256];
static int browser_entry_count = 0;
static float browser_scroll = 0.0f;
static bool browser_new_folder_active = false;
static char browser_new_folder[256] = "";

static bool show_edit_popup = false;
static EditState edit_state = EDIT_LIST;
static float edit_scroll_y = 0.0f;

static int target_cat_idx = 0;
static int target_task_idx = 0;

static char input_name[256] = "";
static char input_description[1024] = "";
static char input_pickrate[32] = "";
static char input_time[32] = ""; 
static int selected_color_idx = 0;  

static bool input_name_active = false;
static bool input_description_active = false;
static bool input_pickrate_active = false;
static bool input_time_active = false;

static bool task_completed[100] = {false};
static bool is_editing_desc = false;
static char edit_desc_buffer[2048] = "";

static char start_message[512] = "";
static char popup_error_message[512] = "";
static bool category_expanded[100] = {false};

static void get_history_file_path(char *buffer, int max_len) {
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        strncpy(buffer, ".taskwheel_history", max_len - 1);
        buffer[max_len - 1] = '\0';
    } else {
        snprintf(buffer, max_len, "%s/.taskwheel_history", home);
    }
}

static void save_history(void) {
    char history_path[PATH_MAX]; get_history_file_path(history_path, sizeof(history_path));
    FILE *file = fopen(history_path, "w");
    if (file == NULL) return;
    for (int i = 0; i < recent_count; i++) fprintf(file, "%s\n", recent_collections[i]);
    fclose(file);
}

static void add_history_entry(const char *path) {
    if (path == NULL || path[0] == '\0') return;
    char real_path[PATH_MAX];
    if (realpath(path, real_path) == NULL) {
        strncpy(real_path, path, sizeof(real_path) - 1);
        real_path[sizeof(real_path) - 1] = '\0';
    }
    int existing = -1;
    for (int i = 0; i < recent_count; i++) if (strcmp(recent_collections[i], real_path) == 0) { existing = i; break; }
    
    if (existing != -1) {
        for (int j = existing; j > 0; j--) strcpy(recent_collections[j], recent_collections[j - 1]);
        strcpy(recent_collections[0], real_path);
    } else {
        if (recent_count < 5) {
            for (int j = recent_count; j > 0; j--) strcpy(recent_collections[j], recent_collections[j - 1]);
            recent_count++;
        } else {
            for (int j = 4; j > 0; j--) strcpy(recent_collections[j], recent_collections[j - 1]);
        }
        strcpy(recent_collections[0], real_path);
    }
    save_history();
}

static void load_history(void) {
    if (history_loaded) return;
    history_loaded = true;
    char history_path[PATH_MAX]; get_history_file_path(history_path, sizeof(history_path));
    FILE *file = fopen(history_path, "r");
    if (file == NULL) return;
    recent_count = 0;
    char line[PATH_MAX];
    while (fgets(line, sizeof(line), file) && recent_count < 5) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;
        if (access(line, F_OK) != 0) continue;
        strncpy(recent_collections[recent_count], line, PATH_MAX - 1);
        recent_collections[recent_count][PATH_MAX - 1] = '\0';
        recent_count++;
    }
    fclose(file);
}

static bool is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void load_browser_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    browser_entry_count = 0;
    strcpy(browser_entries[browser_entry_count++], "..");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && browser_entry_count < 256) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        if (is_directory(full)) {
            strncpy(browser_entries[browser_entry_count++], entry->d_name, 255);
        }
    }
    closedir(dir);
    for (int i = 1; i < browser_entry_count - 1; i++) {
        for (int j = i + 1; j < browser_entry_count; j++) {
            if (strcmp(browser_entries[i], browser_entries[j]) > 0) {
                char temp[256];
                strcpy(temp, browser_entries[i]); strcpy(browser_entries[i], browser_entries[j]); strcpy(browser_entries[j], temp);
            }
        }
    }
}

static void free_collection(f_Collection *collection) {
    if (collection == NULL) return;
    for (int i = 0; i < collection->folder_count; i++) {
        n_Folder *folder = collection->folders[i];
        for (int j = 0; j < folder->task_quantity; j++) free(folder->tasks[j]);
        free(folder);
    }
    free(collection->folders); free(collection);
}

static bool create_category_on_disk(const char *base_path, const char *category_name, float time_val, const char* color_hex) {
    if (category_name == NULL || category_name[0] == '\0') return false;
    char category_path[PATH_MAX + 512]; snprintf(category_path, sizeof(category_path), "%s/%s", base_path, category_name);
    if (mkdir(category_path, 0755) != 0) return false;
    char category_file[PATH_MAX + 512]; if (snprintf(category_file, sizeof(category_file), "%s/%s.txt", category_path, category_name) < 0) return false;
    FILE *file = fopen(category_file, "w"); if (file == NULL) return false;
    fprintf(file, "@%.2f\n%s\n", time_val, color_hex); fclose(file);
    return true;
}

static bool append_task_to_category(const char *base_path, const char *category_name, const char *task_name, int pickrate, const char *description) {
    if (task_name == NULL || task_name[0] == '\0') return false;
    char category_file[PATH_MAX + 512]; snprintf(category_file, sizeof(category_file), "%s/%s/%s.txt", base_path, category_name, category_name);
    FILE *file = fopen(category_file, "a"); if (file == NULL) return false;
    fprintf(file, "\"%s\"(%d)->%s\n", task_name, pickrate, description ? description : ""); fclose(file);
    return true;
}

extern bool update_task_description_on_disk(const char *base_path, const char *category_name, const char *task_name, const char *new_desc);

static void load_collection_path(f_Collection **collection, AppState *appState, const char *path, bool save_history) {
    if (collection == NULL || path == NULL) return;
    if (!is_directory(path)) { snprintf(start_message, sizeof(start_message), "Selected path is not a directory."); return; }
    if (*collection != NULL) { free_collection(*collection); *collection = NULL; }
    f_Collection *new_collection = build_collection(path, 10);
    if (!new_collection) { snprintf(start_message, sizeof(start_message), "Unable to open collection."); return; }
    if (realpath(path, current_collection_path) == NULL) strcpy(current_collection_path, path);
    
    *collection = new_collection;
    appState->screen = SCREEN_WHEEL;
    appState->state = WHEEL_IDLE;
    appState->rotation_angle = 0.0f;
    appState->spin_velocity = 0.0f;
    appState->selected_display_index = -1;
    appState->detail_view = false;
    appState->confirm_reset_all = false;
    appState->confirm_reset_task = false;
    memset(task_completed, 0, sizeof(task_completed));
    if (save_history) add_history_entry(current_collection_path);
}

static bool reload_collection(f_Collection **collection, AppState *appState) {
    if (collection == NULL || *collection == NULL) return false;
    f_Collection *new_collection = build_collection(current_collection_path, 10);
    if (!new_collection) return false;
    free_collection(*collection);
    *collection = new_collection;
    if (appState->chosen_tasks) { free(appState->chosen_tasks->tasks); free(appState->chosen_tasks); appState->chosen_tasks = NULL; }
    appState->state = WHEEL_IDLE;
    appState->selected_display_index = -1;
    appState->detail_view = false;
    appState->rotation_angle = 0.0f;
    memset(task_completed, 0, sizeof(task_completed));
    return true;
}

static bool collection_has_categories(f_Collection *collection) { return collection != NULL && collection->folder_count > 0; }

static void reset_inputs(void) {
    input_name[0] = '\0'; input_description[0] = '\0'; input_pickrate[0] = '\0'; input_time[0] = '\0'; selected_color_idx = 0;
    input_name_active = false; input_description_active = false; input_pickrate_active = false; input_time_active = false; popup_error_message[0] = '\0';
}

void UpdateDrawFrame(f_Collection **collection, AppState *appState) {
    if (browser_path[0] == '\0') {
        if (getcwd(browser_path, PATH_MAX) == NULL) strcpy(browser_path, "/");
    }

    f_Collection *current_collection = collection ? *collection : NULL;
    Vector2 wheelCenter = { WINDOW_WIDTH * 0.35f, WINDOW_HEIGHT / 2.0f }; 
    Vector2 mousePos = GetMousePosition();
    bool has_collection = current_collection != NULL;
    bool has_categories = collection_has_categories(current_collection);
    
    bool popupActive = appState->confirm_reset_all || appState->confirm_reset_task || show_edit_popup || show_browser;
    bool isCenterHovered = has_collection && CheckCollisionPointCircle(mousePos, wheelCenter, CENTER_RADIUS);
    bool spinDisabled = appState->detail_view || !has_categories;
    float deltaTime = GetFrameTime();
    bool cursorBlink = ((int)(GetTime() * 2) % 2 == 0); 

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (appState->confirm_reset_all || appState->confirm_reset_task) {
            appState->confirm_reset_all = false; appState->confirm_reset_task = false;
        } else if (show_edit_popup) {
            show_edit_popup = false;
        } else if (show_browser) {
            show_browser = false;
        } else if (appState->detail_view) {
            if (is_editing_desc) { is_editing_desc = false; } 
            else { appState->detail_view = false; appState->selected_display_index = -1; }
        }
    }

    if (IsKeyPressed(KEY_TAB)) {
        if (show_edit_popup) {
            if (edit_state == EDIT_ADD_CAT) {
                if (input_name_active) { input_name_active = false; input_time_active = true; }
                else { input_time_active = false; input_name_active = true; }
            } else if (edit_state == EDIT_ADD_TASK) {
                if (input_name_active) { input_name_active = false; input_pickrate_active = true; }
                else if (input_pickrate_active) { input_pickrate_active = false; input_description_active = true; }
                else { input_description_active = false; input_name_active = true; }
            }
        } else if (show_browser) {
            if (browser_path_active) { browser_path_active = false; browser_new_folder_active = true; }
            else { browser_new_folder_active = false; browser_path_active = true; }
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (show_browser && browser_path_active) {
            char resolved[PATH_MAX];
            if (realpath(browser_path, resolved) != NULL) strcpy(browser_path, resolved);
            load_browser_dir(browser_path);
            browser_scroll = 0;
            browser_path_active = false;
        }
    }

    int key = GetCharPressed();
    while (key > 0) {
        if (input_name_active && strlen(input_name) < sizeof(input_name) - 1) { size_t len = strlen(input_name); input_name[len] = (char)key; input_name[len + 1] = '\0'; }
        if (input_description_active && strlen(input_description) < sizeof(input_description) - 1) { size_t len = strlen(input_description); input_description[len] = (char)key; input_description[len + 1] = '\0'; }
        if (input_pickrate_active && key >= '0' && key <= '9' && strlen(input_pickrate) < sizeof(input_pickrate) - 1) { size_t len = strlen(input_pickrate); input_pickrate[len] = (char)key; input_pickrate[len + 1] = '\0'; }
        if (input_time_active && ((key >= '0' && key <= '9') || key == '.') && strlen(input_time) < sizeof(input_time) - 1) { size_t len = strlen(input_time); input_time[len] = (char)key; input_time[len + 1] = '\0'; }
        if (browser_new_folder_active && strlen(browser_new_folder) < sizeof(browser_new_folder) - 1) { size_t len = strlen(browser_new_folder); browser_new_folder[len] = (char)key; browser_new_folder[len + 1] = '\0'; }
        if (is_editing_desc && strlen(edit_desc_buffer) < sizeof(edit_desc_buffer) - 1) { size_t len = strlen(edit_desc_buffer); edit_desc_buffer[len] = (char)key; edit_desc_buffer[len + 1] = '\0'; }
        if (browser_path_active && strlen(browser_path) < sizeof(browser_path) - 1) { size_t len = strlen(browser_path); browser_path[len] = (char)key; browser_path[len + 1] = '\0'; }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (input_name_active && strlen(input_name) > 0) input_name[strlen(input_name) - 1] = '\0';
        if (input_description_active && strlen(input_description) > 0) input_description[strlen(input_description) - 1] = '\0';
        if (input_pickrate_active && strlen(input_pickrate) > 0) input_pickrate[strlen(input_pickrate) - 1] = '\0';
        if (input_time_active && strlen(input_time) > 0) input_time[strlen(input_time) - 1] = '\0';
        if (browser_new_folder_active && strlen(browser_new_folder) > 0) browser_new_folder[strlen(browser_new_folder) - 1] = '\0';
        if (is_editing_desc && strlen(edit_desc_buffer) > 0) edit_desc_buffer[strlen(edit_desc_buffer) - 1] = '\0';
        if (browser_path_active && strlen(browser_path) > 0) browser_path[strlen(browser_path) - 1] = '\0';
    }
    if (IsKeyPressed(KEY_V) && IsKeyDown(KEY_LEFT_CONTROL)) {
        const char *clipboard = GetClipboardText();
        if (clipboard != NULL) {
            if (input_name_active && strlen(input_name) + strlen(clipboard) < sizeof(input_name)) strcat(input_name, clipboard);
            if (input_description_active && strlen(input_description) + strlen(clipboard) < sizeof(input_description)) strcat(input_description, clipboard);
            if (is_editing_desc && strlen(edit_desc_buffer) + strlen(clipboard) < sizeof(edit_desc_buffer)) strcat(edit_desc_buffer, clipboard);
            if (browser_path_active && strlen(browser_path) + strlen(clipboard) < sizeof(browser_path)) strcat(browser_path, clipboard);
        }
    }

    if (appState->screen == SCREEN_COLLECTION_SELECT) {
        load_history();
        BeginDrawing();
        ClearBackground(HexToColor("#F8FAFC")); 

        DrawRectangle(0, 0, 400, WINDOW_HEIGHT, HexToColor("#0F172A"));
        DrawText("TASK", 50, 100, 60, HexToColor("#3B82F6"));
        DrawText("WHEEL", 50, 160, 60, WHITE);
        DrawText("Manage your daily", 50, 260, 20, HexToColor("#94A3B8"));
        DrawText("routines seamlessly.", 50, 290, 20, HexToColor("#94A3B8"));

        DrawText("Recent Collections", 460, 80, 28, HexToColor("#1E293B"));
        int recentY = 140;
        if (recent_count == 0) DrawText("No recent collections yet.", 460, recentY, 20, GRAY);
        else {
            for (int i = 0; i < recent_count; i++) {
                Rectangle recentRect = { 460, recentY, 700, 55 };
                bool hovered = !show_browser && CheckCollisionPointRec(mousePos, recentRect);
                DrawShadow(recentRect, 0.2f, 10, 3.0f);
                DrawRectangleRounded(recentRect, 0.2f, 10, hovered ? WHITE : HexToColor("#F1F5F9"));
                DrawRectangleRoundedLines(recentRect, 0.2f, 10, HexToColor("#CBD5E1"));
                
                char truncRec[128]; TruncateText(recent_collections[i], truncRec, 65);
                DrawText(truncRec, recentRect.x + 20, recentRect.y + 18, 18, HexToColor("#0F172A"));
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered) load_collection_path(collection, appState, recent_collections[i], false);
                recentY += 70;
            }
        }

        Rectangle browseBtn = { 460, recentY + 30, 260, 50 };
        bool browseHov = !show_browser && CheckCollisionPointRec(mousePos, browseBtn);
        DrawShadow(browseBtn, 0.3f, 10, 3.0f);
        DrawRectangleRounded(browseBtn, 0.3f, 10, browseHov ? HexToColor("#2563EB") : HexToColor("#3B82F6"));
        DrawText("Browse / Create...", browseBtn.x + 40, browseBtn.y + 15, 20, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && browseHov) {
            show_browser = true; load_browser_dir(browser_path);
        }

        if (start_message[0] != '\0') DrawText(start_message, 460, WINDOW_HEIGHT - 60, 20, RED);

        if (show_browser) {
            DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Fade(BLACK, 0.6f));
            Rectangle bRect = { 140, 60, 1000, 600 };
            DrawShadow(bRect, 0.05f, 10, 10.0f);
            DrawRectangleRounded(bRect, 0.05f, 10, WHITE);
            DrawRectangleRoundedLines(bRect, 0.05f, 10, HexToColor("#CBD5E1"));

            DrawText("File Explorer", bRect.x + 20, bRect.y + 20, 24, BLACK);
            Rectangle closeBtn = { bRect.x + bRect.width - 40, bRect.y + 10, 30, 30 };
            if (CheckCollisionPointRec(mousePos, closeBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) show_browser = false;
            DrawText("X", closeBtn.x + 8, closeBtn.y + 5, 20, GRAY);

            Rectangle pathRect = { bRect.x + 220, bRect.y + 60, bRect.width - 320, 36 };
            DrawRectangleRounded(pathRect, 0.2f, 10, browser_path_active ? Fade(LIGHTGRAY, 0.3f) : HexToColor("#F8FAFC"));
            DrawRectangleRoundedLines(pathRect, 0.2f, 10, browser_path_active ? HexToColor("#3B82F6") : HexToColor("#CBD5E1"));
            char truncPath[256]; TruncateText(browser_path, truncPath, 70);
            DrawText(truncPath, pathRect.x + 10, pathRect.y + 10, 16, HexToColor("#1E293B"));
            if (browser_path_active && cursorBlink) DrawRectangle(pathRect.x + 10 + MeasureText(truncPath, 16) + 2, pathRect.y + 8, 2, 20, BLACK);
            
            Rectangle goBtn = { pathRect.x + pathRect.width + 10, pathRect.y, 60, 36 };
            bool goHov = CheckCollisionPointRec(mousePos, goBtn);
            DrawRectangleRounded(goBtn, 0.2f, 10, goHov ? LIGHTGRAY : HexToColor("#F1F5F9"));
            DrawRectangleRoundedLines(goBtn, 0.2f, 10, GRAY);
            DrawText("GO", goBtn.x + 18, goBtn.y + 10, 16, BLACK);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                browser_path_active = CheckCollisionPointRec(mousePos, pathRect);
                if (goHov) {
                    char resolved[PATH_MAX];
                    if (realpath(browser_path, resolved) != NULL) strcpy(browser_path, resolved);
                    load_browser_dir(browser_path); browser_scroll = 0;
                }
            }

            Rectangle sideRect = { bRect.x + 20, bRect.y + 60, 180, 430 };
            DrawRectangleRounded(sideRect, 0.05f, 10, HexToColor("#F8FAFC")); 
            DrawRectangleRoundedLines(sideRect, 0.05f, 10, HexToColor("#E2E8F0"));
            const char *home_dir = getenv("HOME");
            char *labels[] = {"Home", "Desktop", "Documents", "Downloads"};
            char paths[4][PATH_MAX];
            snprintf(paths[0], PATH_MAX, "%s", home_dir);
            snprintf(paths[1], PATH_MAX, "%s/Desktop", home_dir);
            snprintf(paths[2], PATH_MAX, "%s/Documents", home_dir);
            snprintf(paths[3], PATH_MAX, "%s/Downloads", home_dir);

            for (int i = 0; i < 4; i++) {
                Rectangle shBtn = { sideRect.x + 10, sideRect.y + 10 + (i * 45), 160, 35 };
                bool shHov = CheckCollisionPointRec(mousePos, shBtn);
                DrawRectangleRounded(shBtn, 0.2f, 10, shHov ? HexToColor("#E2E8F0") : WHITE);
                DrawRectangleRoundedLines(shBtn, 0.2f, 10, HexToColor("#CBD5E1"));
                DrawText(labels[i], shBtn.x + 15, shBtn.y + 10, 16, HexToColor("#334155"));
                if (shHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    strcpy(browser_path, paths[i]); load_browser_dir(browser_path); browser_scroll = 0; browser_path_active = false;
                }
            }

            Rectangle listRect = { bRect.x + 220, bRect.y + 110, bRect.width - 250, 380 };
            DrawRectangleRoundedLines(listRect, 0.05f, 10, HexToColor("#CBD5E1"));

            bool inBrowserList = CheckCollisionPointRec(mousePos, listRect);
            if (inBrowserList) browser_scroll += GetMouseWheelMove() * 30.0f;
            if (browser_scroll > 0) browser_scroll = 0;

            BeginScissorMode(listRect.x, listRect.y, listRect.width, listRect.height);
            float bY = listRect.y + 5 + browser_scroll;
            for (int i = 0; i < browser_entry_count; i++) {
                Rectangle item = { listRect.x + 5, bY, listRect.width - 10, 30 };
                bool iHov = inBrowserList && CheckCollisionPointRec(mousePos, item);
                if (iHov) DrawRectangleRounded(item, 0.2f, 10, HexToColor("#F1F5F9"));
                
                DrawText(">", item.x + 10, item.y + 6, 16, DARKGRAY);
                DrawText(browser_entries[i], item.x + 30, item.y + 6, 18, HexToColor("#0F172A"));

                if (iHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    char new_path[PATH_MAX + 512]; snprintf(new_path, sizeof(new_path), "%s/%s", browser_path, browser_entries[i]);
                    char resolved[PATH_MAX];
                    if (realpath(new_path, resolved) != NULL) {
                        strcpy(browser_path, resolved); load_browser_dir(browser_path); browser_scroll = 0; break; 
                    }
                }
                bY += 35;
            }
            EndScissorMode();

            DrawText("New Folder:", bRect.x + 220, bRect.y + 515, 18, DARKGRAY);
            Rectangle newFoldRect = { bRect.x + 330, bRect.y + 505, 200, 36 };
            DrawRectangleRounded(newFoldRect, 0.2f, 10, browser_new_folder_active ? Fade(LIGHTGRAY, 0.3f) : WHITE);
            DrawRectangleRoundedLines(newFoldRect, 0.2f, 10, browser_new_folder_active ? HexToColor("#3B82F6") : GRAY);
            DrawText(browser_new_folder, newFoldRect.x + 10, newFoldRect.y + 10, 16, BLACK);
            if (browser_new_folder_active && cursorBlink) DrawRectangle(newFoldRect.x + 10 + MeasureText(browser_new_folder, 16), newFoldRect.y + 8, 2, 20, BLACK);
            
            Rectangle createFoldBtn = { newFoldRect.x + 210, newFoldRect.y, 80, 36 };
            bool crHov = CheckCollisionPointRec(mousePos, createFoldBtn);
            DrawRectangleRounded(createFoldBtn, 0.2f, 10, crHov ? LIGHTGRAY : HexToColor("#F8FAFC")); 
            DrawRectangleRoundedLines(createFoldBtn, 0.2f, 10, GRAY);
            DrawText("Create", createFoldBtn.x + 15, createFoldBtn.y + 10, 16, BLACK);
            
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mousePos, newFoldRect)) { browser_new_folder_active = true; browser_path_active = false; }
                else browser_new_folder_active = false;
                
                if (crHov && browser_new_folder[0] != '\0') {
                    char mk[PATH_MAX + 256]; snprintf(mk, sizeof(mk), "%s/%s", browser_path, browser_new_folder);
                    mkdir(mk, 0755); load_browser_dir(browser_path); browser_new_folder[0] = '\0'; browser_new_folder_active = false;
                }
            }

            Rectangle selBtn = { bRect.x + bRect.width - 240, bRect.y + 505, 220, 40 };
            bool selHov = CheckCollisionPointRec(mousePos, selBtn);
            DrawShadow(selBtn, 0.2f, 10, 2.0f);
            DrawRectangleRounded(selBtn, 0.2f, 10, selHov ? HexToColor("#2563EB") : HexToColor("#3B82F6"));
            DrawText("Select This Directory", selBtn.x + 15, selBtn.y + 10, 18, WHITE);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && selHov) {
                load_collection_path(collection, appState, browser_path, true); show_browser = false;
            }
        }
        EndDrawing();
        return;
    }

    if (!has_collection) { appState->screen = SCREEN_COLLECTION_SELECT; return; }

    Rectangle editBtn = { 30, 20, 100, 40 };
    bool editHovered = CheckCollisionPointRec(mousePos, editBtn);

    if (!popupActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && editHovered) {
        show_edit_popup = true; edit_state = EDIT_LIST; edit_scroll_y = 0.0f;
    }

    if (appState->state == WHEEL_SPINNING) {
        appState->rotation_angle += appState->spin_velocity * deltaTime;
        appState->spin_velocity -= 150.0f * deltaTime;
        if (appState->spin_velocity <= 0) {
            appState->spin_velocity = 0; appState->state = WHEEL_FINISHED;
        }
    }

    if (!popupActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isCenterHovered && !spinDisabled) {
        if (appState->state == WHEEL_IDLE || appState->state == WHEEL_FINISHED) {
            if (appState->chosen_tasks) { free(appState->chosen_tasks->tasks); free(appState->chosen_tasks); }
            appState->chosen_tasks = choose_task(current_collection, current_collection_path);
            appState->state = WHEEL_SPINNING;
            appState->spin_velocity = (float)GetRandomValue(700, 1200);
            appState->selected_display_index = -1;
            appState->detail_view = false;
            memset(task_completed, 0, sizeof(task_completed)); 
            is_editing_desc = false;
        }
    }

    BeginDrawing();
    ClearBackground(HexToColor("#F8FAFC"));

    DrawShadow(editBtn, 0.3f, 10, 2.0f);
    DrawRectangleRounded(editBtn, 0.3f, 10, editHovered ? HexToColor("#E2E8F0") : WHITE);
    DrawRectangleRoundedLines(editBtn, 0.3f, 10, HexToColor("#CBD5E1"));
    DrawText("Edit", editBtn.x + 30, editBtn.y + 10, 20, HexToColor("#0F172A"));

    float total_weight = 0.0f;
    if (current_collection->folder_count > 0) {
        for (int i = 0; i < current_collection->folder_count; i++) {
            total_weight += fmaxf(current_collection->folders[i]->total_task_time, 0.1f); 
        }
    }

    if (current_collection->folder_count > 0 && total_weight > 0) {
        DrawCircle(wheelCenter.x + 4, wheelCenter.y + 4, WHEEL_RADIUS, Fade(BLACK, 0.08f));
        float current_start = appState->rotation_angle;
        for (int i = 0; i < current_collection->folder_count; i++) {
            n_Folder *folder = current_collection->folders[i];
            Color sliceColor = HexToColor(folder->folder_color_hex);
            float weight = fmaxf(folder->total_task_time, 0.1f);
            float sliceAngle = (weight / total_weight) * 360.0f;
            
            DrawCircleSector(wheelCenter, WHEEL_RADIUS, current_start, current_start + sliceAngle, 64, sliceColor);
            DrawCircleSectorLines(wheelCenter, WHEEL_RADIUS, current_start, current_start + sliceAngle, 64, HexToColor("#F8FAFC")); 
            
            float midAngle = current_start + (sliceAngle / 2.0f);
            Vector2 textPos = { wheelCenter.x + cosf(midAngle * DEG2RAD) * (WHEEL_RADIUS * 0.65f), wheelCenter.y + sinf(midAngle * DEG2RAD) * (WHEEL_RADIUS * 0.65f) };
            
            if (sliceAngle > 15.0f) {
                char displayText[128];
                if (appState->state == WHEEL_FINISHED && appState->chosen_tasks && i < appState->chosen_tasks->count) TruncateText(appState->chosen_tasks->tasks[i]->name, displayText, 15);
                else TruncateText(folder->name, displayText, 15);
                
                Vector2 textSize = MeasureTextEx(GetFontDefault(), displayText, 20, 2);
                Color textColor = (sliceColor.r + sliceColor.g + sliceColor.b < 300) ? WHITE : BLACK;
                DrawTextPro(GetFontDefault(), displayText, textPos, (Vector2){ textSize.x / 2.0f, textSize.y / 2.0f }, midAngle, 20, 2, textColor);
            }
            current_start += sliceAngle;
        }
    } else {
        DrawCircleV(wheelCenter, WHEEL_RADIUS, HexToColor("#E2E8F0"));
        DrawCircleLines(wheelCenter.x, wheelCenter.y, WHEEL_RADIUS, HexToColor("#CBD5E1"));
    }

    DrawCircleV(wheelCenter, CENTER_RADIUS, HexToColor("#F8FAFC")); 
    Color centerColor = spinDisabled ? HexToColor("#E2E8F0") : (isCenterHovered ? HexToColor("#2563EB") : HexToColor("#3B82F6"));
    DrawCircleV(wheelCenter, CENTER_RADIUS - 10, centerColor); 
    if (!spinDisabled) DrawCircleLines(wheelCenter.x, wheelCenter.y, CENTER_RADIUS - 10, HexToColor("#1D4ED8"));
    
    int spinTextWidth = MeasureText("SPIN", 20);
    DrawText("SPIN", wheelCenter.x - spinTextWidth/2, wheelCenter.y - 10, 20, spinDisabled ? GRAY : WHITE);

    Vector2 p1 = { wheelCenter.x + WHEEL_RADIUS - 5, wheelCenter.y };
    Vector2 p2 = { wheelCenter.x + WHEEL_RADIUS + 25, wheelCenter.y - 15 };
    Vector2 p3 = { wheelCenter.x + WHEEL_RADIUS + 25, wheelCenter.y + 15 };
    DrawTriangle(p1, p2, p3, HexToColor("#0F172A"));

    int listX = WINDOW_WIDTH - 480;
    int listY = 50;
    char headerTitle[128]; TruncateText("CATEGORIES / DAILY TASKS", headerTitle, 30);
    DrawText(headerTitle, listX, listY, 26, HexToColor("#0F172A"));

    Rectangle resetAllRect = { WINDOW_WIDTH - 60, listY - 5, 30, 30 };
    bool resetAllHovered = CheckCollisionPointRec(mousePos, resetAllRect);
    DrawRectangleRounded(resetAllRect, 0.3f, 10, resetAllHovered ? HexToColor("#E2E8F0") : WHITE);
    DrawRectangleRoundedLines(resetAllRect, 0.3f, 10, HexToColor("#CBD5E1"));
    DrawText("X", resetAllRect.x + 10, resetAllRect.y + 5, 20, HexToColor("#475569"));
    if (resetAllHovered) DrawText("Reset all tasks", resetAllRect.x - 120, resetAllRect.y + 35, 14, DARKGRAY);
    if (!popupActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && resetAllHovered) appState->confirm_reset_all = true;

    float total_hours = 0.0f;
    for (int i = 0; i < current_collection->folder_count; i++) total_hours += current_collection->folders[i]->total_task_time;
    int t_hours = (int)total_hours;
    int t_mins = (int)((total_hours - t_hours) * 60);
    char totalTimeStr[128]; snprintf(totalTimeStr, sizeof(totalTimeStr), "Time: %dh %02dm", t_hours, t_mins);
    DrawText(totalTimeStr, listX, listY + 35, 18, HexToColor("#64748B"));

    if (appState->state == WHEEL_FINISHED && appState->chosen_tasks) {
        int comp = 0, total = appState->chosen_tasks->count;
        for (int i = 0; i < total; i++) if (task_completed[i]) comp++;
        if (total > 0) DrawText(TextFormat("Completed: %d/%d (%.0f%%)", comp, total, ((float)comp / total) * 100.0f), listX + 180, listY + 35, 18, HexToColor("#10B981"));
    }

    DrawLine(listX, listY + 65, WINDOW_WIDTH - 20, listY + 65, HexToColor("#CBD5E1"));
    listY += 85;

    bool listClickAllowed = !popupActive && !appState->detail_view;

    for (int i = 0; i < current_collection->folder_count; i++) {
        n_Folder *folder = current_collection->folders[i];
        Color catColor = HexToColor(folder->folder_color_hex);
        
        Rectangle itemRect = { listX, listY, 440, 50 };
        Rectangle cbRect = { listX + 25, listY + 15, 20, 20 }; 
        
        bool isHovered = CheckCollisionPointRec(mousePos, itemRect);
        bool cbHov = CheckCollisionPointRec(mousePos, cbRect);

        if (isHovered && !cbHov) {
            DrawShadow(itemRect, 0.3f, 10, 2.0f);
            DrawRectangleRounded(itemRect, 0.3f, 10, WHITE);
        } else {
            DrawRectangleRounded(itemRect, 0.3f, 10, HexToColor("#F1F5F9"));
        }
        
        if (appState->selected_display_index == i) DrawRectangleRoundedLines(itemRect, 0.3f, 10, HexToColor("#3B82F6"));
        else DrawRectangleRoundedLines(itemRect, 0.3f, 10, HexToColor("#CBD5E1"));

        bool isDailyView = (appState->state == WHEEL_FINISHED && appState->chosen_tasks && i < appState->chosen_tasks->count);
        bool taskIsReset = isDailyView && (appState->chosen_tasks->tasks[i]->pickrate == 0);

        if (listClickAllowed && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (isDailyView && cbHov) {
                task_completed[i] = !task_completed[i]; 
            } else if (isHovered && !cbHov && !taskIsReset) {
                if (isDailyView) { appState->selected_display_index = i; appState->detail_view = true; is_editing_desc = false; } 
                else { appState->selected_display_index = i; }
            }
        }

        if (isDailyView) {
            DrawCircle(listX + 10, listY + 25, 6, catColor); 
            DrawRectangleRounded(cbRect, 0.2f, 10, task_completed[i] ? HexToColor("#10B981") : WHITE);
            DrawRectangleRoundedLines(cbRect, 0.2f, 10, cbHov ? HexToColor("#64748B") : HexToColor("#94A3B8"));
            
            if (task_completed[i]) {
                DrawLineEx((Vector2){cbRect.x+4, cbRect.y+10}, (Vector2){cbRect.x+8, cbRect.y+14}, 2, WHITE);
                DrawLineEx((Vector2){cbRect.x+8, cbRect.y+14}, (Vector2){cbRect.x+16, cbRect.y+6}, 2, WHITE);
            }

            char dText[128]; TruncateText(appState->chosen_tasks->tasks[i]->name, dText, 35);
            if (task_completed[i]) {
                DrawText(dText, listX + 55, listY + 15, 20, HexToColor("#94A3B8"));
                DrawLine(listX + 55, listY + 25, listX + 55 + MeasureText(dText, 20), listY + 25, HexToColor("#94A3B8")); 
            } else {
                DrawText(dText, listX + 55, listY + 15, 20, taskIsReset ? HexToColor("#EF4444") : HexToColor("#0F172A"));
            }
        } else {
            DrawCircle(listX + 20, listY + 25, 8, catColor); 
            char itemText[512]; snprintf(itemText, sizeof(itemText), "[%.1fh] %s", folder->total_task_time, folder->name);
            char dText[128]; TruncateText(itemText, dText, 35);
            DrawText(dText, listX + 40, listY + 15, 20, HexToColor("#0F172A"));
        }
        listY += 60;
    }

    if (appState->detail_view && appState->selected_display_index != -1 && appState->state == WHEEL_FINISHED && appState->chosen_tasks && appState->selected_display_index < appState->chosen_tasks->count) {
        int i = appState->selected_display_index;
        t_task *t = appState->chosen_tasks->tasks[i];

        Rectangle panelRect = { listX - 20, 50, 480, WINDOW_HEIGHT - 100 };
        DrawShadow(panelRect, 0.05f, 10, 5.0f);
        DrawRectangleRounded(panelRect, 0.05f, 10, HexToColor("#F8FAFC"));
        DrawRectangleRoundedLines(panelRect, 0.05f, 10, HexToColor("#CBD5E1"));

        Rectangle backRect = { listX, 70, 40, 32 };
        if (CheckCollisionPointRec(mousePos, backRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !popupActive) {
            appState->detail_view = false; appState->selected_display_index = -1; is_editing_desc = false;
        }
        DrawRectangleRounded(backRect, 0.2f, 10, CheckCollisionPointRec(mousePos, backRect) ? HexToColor("#E2E8F0") : WHITE);
        DrawRectangleRoundedLines(backRect, 0.2f, 10, HexToColor("#CBD5E1")); 
        DrawText("<", backRect.x + 12, backRect.y + 6, 24, HexToColor("#334155"));

        char headerText[512]; snprintf(headerText, sizeof(headerText), "%s | Rate: %d", t->name, t->pickrate);
        char dText[128]; TruncateText(headerText, dText, 30);
        DrawTextEx(GetFontDefault(), dText, (Vector2){ listX, 120 }, 22, 2, (t->pickrate == 0) ? HexToColor("#EF4444") : HexToColor("#0F172A"));

        if (t->pickrate > 0) {
            Rectangle resetTaskRect = { listX + 400, 115, 30, 30 };
            bool rstHov = CheckCollisionPointRec(mousePos, resetTaskRect);
            DrawRectangleRounded(resetTaskRect, 0.2f, 10, rstHov ? HexToColor("#E2E8F0") : WHITE);
            DrawRectangleRoundedLines(resetTaskRect, 0.2f, 10, HexToColor("#CBD5E1")); 
            DrawText("X", resetTaskRect.x + 8, resetTaskRect.y + 4, 24, HexToColor("#475569"));
            if (rstHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !popupActive) appState->confirm_reset_task = true;
        }

        DrawText("DESCRIPTION", listX, 180, 18, HexToColor("#64748B"));
        
        Rectangle editDescBtn = { listX + MeasureText("DESCRIPTION", 18) + 15, 178, 30, 24 };
        bool edHov = CheckCollisionPointRec(mousePos, editDescBtn);
        DrawRectangleRounded(editDescBtn, 0.2f, 10, edHov ? HexToColor("#E2E8F0") : WHITE); 
        DrawRectangleRoundedLines(editDescBtn, 0.2f, 10, HexToColor("#CBD5E1"));
        DrawText("E", editDescBtn.x + 10, editDescBtn.y + 4, 16, HexToColor("#0F172A"));
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && edHov && !popupActive) {
            is_editing_desc = !is_editing_desc;
            if (is_editing_desc) strcpy(edit_desc_buffer, t->description);
        }

        Rectangle ta = { listX, 210, 440, 250 };
        DrawRectangleRounded(ta, 0.05f, 10, is_editing_desc ? WHITE : HexToColor("#F1F5F9"));
        DrawRectangleRoundedLines(ta, 0.05f, 10, is_editing_desc ? HexToColor("#3B82F6") : HexToColor("#CBD5E1"));

        if (is_editing_desc) {
            DrawTextEx(GetFontDefault(), edit_desc_buffer, (Vector2){ta.x+10, ta.y+10}, 18, 1, HexToColor("#0F172A"));
            if (cursorBlink) {
                Vector2 ts = MeasureTextEx(GetFontDefault(), edit_desc_buffer, 18, 1);
                DrawRectangle(ta.x + 10 + ts.x, ta.y + 10 + ts.y - 18, 2, 18, BLACK);
            }
            Rectangle saveBtn = { listX, 480, 100, 36 };
            bool svHov = CheckCollisionPointRec(mousePos, saveBtn);
            DrawRectangleRounded(saveBtn, 0.2f, 10, svHov ? HexToColor("#059669") : HexToColor("#10B981"));
            DrawText("SAVE", saveBtn.x + 28, saveBtn.y + 10, 18, WHITE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && svHov && !popupActive) {
                if(update_task_description_on_disk(current_collection_path, current_collection->folders[appState->selected_display_index]->name, t->name, edit_desc_buffer)) {
                    strcpy(t->description, edit_desc_buffer);
                    is_editing_desc = false;
                }
            }
        } else {
            DrawTextEx(GetFontDefault(), t->description, (Vector2){ta.x+10, ta.y+10}, 18, 1, HexToColor("#334155"));
        }
    } else if (appState->selected_display_index != -1) {
        int i = appState->selected_display_index;
        Rectangle descRect = { listX - 20, 50, 480, WINDOW_HEIGHT - 100 };
        DrawShadow(descRect, 0.05f, 10, 5.0f);
        DrawRectangleRounded(descRect, 0.05f, 10, HexToColor("#F8FAFC")); 
        DrawRectangleRoundedLines(descRect, 0.05f, 10, HexToColor("#CBD5E1"));
        DrawText("DETAILS", listX, 70, 20, HexToColor("#64748B"));

        if (appState->state == WHEEL_FINISHED && appState->chosen_tasks && i < appState->chosen_tasks->count) {
            t_task *t = appState->chosen_tasks->tasks[i];
            DrawText(TextFormat("Task: %s", t->name), listX, 110, 18, HexToColor("#0F172A"));
            DrawText(TextFormat("Pickrate: %d", t->pickrate), listX, 140, 18, HexToColor("#3B82F6"));
            DrawTextEx(GetFontDefault(), t->description, (Vector2){listX, 180}, 18, 1, HexToColor("#334155"));
        } else {
            DrawText("Spin the wheel to", listX, 110, 18, HexToColor("#94A3B8"));
            DrawText("reveal today's tasks.", listX, 140, 18, HexToColor("#94A3B8"));
        }
    }

    if (show_edit_popup) {
        DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Fade(BLACK, 0.6f));
        Rectangle popupRect = { 200, 80, 880, 560 };
        DrawShadow(popupRect, 0.05f, 10, 10.0f);
        DrawRectangleRounded(popupRect, 0.05f, 10, WHITE); 
        DrawRectangleRoundedLines(popupRect, 0.05f, 10, HexToColor("#CBD5E1"));

        DrawText("Edit Categories & Tasks", popupRect.x + 30, popupRect.y + 25, 24, HexToColor("#0F172A"));
        Rectangle closeRect = { popupRect.x + popupRect.width - 50, popupRect.y + 20, 30, 30 };
        bool closeHovered = CheckCollisionPointRec(mousePos, closeRect);
        DrawRectangleRounded(closeRect, 0.2f, 10, closeHovered ? HexToColor("#F1F5F9") : WHITE);
        DrawText("X", closeRect.x + 8, closeRect.y + 5, 20, HexToColor("#64748B"));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && closeHovered) show_edit_popup = false;

        DrawLine(popupRect.x, popupRect.y + 70, popupRect.x + popupRect.width, popupRect.y + 70, HexToColor("#E2E8F0"));
        Rectangle scrollArea = { popupRect.x, popupRect.y + 71, popupRect.width, popupRect.height - 72 };
        bool inScrollArea = CheckCollisionPointRec(mousePos, scrollArea);

        if (edit_state == EDIT_LIST) {
            if (inScrollArea) edit_scroll_y += GetMouseWheelMove() * 30.0f;
            if (edit_scroll_y > 0) edit_scroll_y = 0;

            BeginScissorMode(scrollArea.x, scrollArea.y, scrollArea.width, scrollArea.height);
            float content_y = scrollArea.y + 20 + edit_scroll_y;
            float max_scroll_limit = 0;

            for (int i = 0; i < current_collection->folder_count; i++) {
                n_Folder *folder = current_collection->folders[i];
                Color catColor = HexToColor(folder->folder_color_hex);

                Rectangle catRect = { popupRect.x + 30, content_y, popupRect.width - 60, 45 };
                bool catHovered = inScrollArea && CheckCollisionPointRec(mousePos, catRect);
                DrawRectangleRounded(catRect, 0.2f, 10, catHovered ? HexToColor("#F8FAFC") : WHITE);
                DrawRectangleRoundedLines(catRect, 0.2f, 10, LIGHTGRAY);

                Rectangle expandBtn = { catRect.x + 10, catRect.y + 12, 20, 20 };
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inScrollArea && CheckCollisionPointRec(mousePos, expandBtn)) category_expanded[i] = !category_expanded[i];
                DrawRectangleRounded(expandBtn, 0.2f, 10, HexToColor("#F1F5F9")); 
                DrawText(category_expanded[i] ? "v" : ">", expandBtn.x + 5, expandBtn.y + 2, 16, HexToColor("#475569"));

                DrawCircle(catRect.x + 50, catRect.y + 22, 8, catColor);
                char catTitle[256]; snprintf(catTitle, sizeof(catTitle), "[%.1fh] %s", folder->total_task_time, folder->name);
                DrawText(catTitle, catRect.x + 70, catRect.y + 13, 20, HexToColor("#0F172A"));

                Rectangle delCatBtn = { catRect.x + catRect.width - 35, catRect.y + 12, 20, 20 };
                bool delCatHovered = inScrollArea && CheckCollisionPointRec(mousePos, delCatBtn);
                DrawRectangleRounded(delCatBtn, 0.2f, 10, delCatHovered ? HexToColor("#EF4444") : HexToColor("#FEE2E2"));
                DrawText("x", delCatBtn.x + 6, delCatBtn.y + 2, 16, delCatHovered ? WHITE : HexToColor("#B91C1C"));
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && delCatHovered) { target_cat_idx = i; edit_state = EDIT_DEL_CAT_CONFIRM; }
                content_y += 50;

                if (category_expanded[i]) {
                    for (int j = 0; j < folder->task_quantity; j++) {
                        t_task *task = folder->tasks[j];
                        Rectangle taskRect = { popupRect.x + 70, content_y, popupRect.width - 100, 36 };
                        bool taskHovered = inScrollArea && CheckCollisionPointRec(mousePos, taskRect);
                        DrawRectangleRounded(taskRect, 0.2f, 10, taskHovered ? HexToColor("#F1F5F9") : HexToColor("#F8FAFC"));
                        DrawRectangleRoundedLines(taskRect, 0.2f, 10, HexToColor("#E2E8F0"));
                        
                        DrawCircle(taskRect.x + 15, taskRect.y + 18, 6, catColor); 
                        char tTitle[256]; snprintf(tTitle, sizeof(tTitle), "%s (Rate: %d)", task->name, task->pickrate);
                        DrawText(tTitle, taskRect.x + 35, taskRect.y + 9, 18, HexToColor("#334155"));

                        Rectangle delTaskBtn = { taskRect.x + taskRect.width - 30, taskRect.y + 8, 20, 20 };
                        bool delTaskHovered = inScrollArea && CheckCollisionPointRec(mousePos, delTaskBtn);
                        DrawRectangleRounded(delTaskBtn, 0.2f, 10, delTaskHovered ? HexToColor("#EF4444") : HexToColor("#FEE2E2"));
                        DrawText("x", delTaskBtn.x + 6, delTaskBtn.y + 2, 16, delTaskHovered ? WHITE : HexToColor("#B91C1C"));
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && delTaskHovered) { target_cat_idx = i; target_task_idx = j; edit_state = EDIT_DEL_TASK_CONFIRM; }
                        content_y += 42;
                    }
                    Rectangle addTaskBtn = { popupRect.x + 70, content_y, 110, 30 };
                    bool addTaskHovered = inScrollArea && CheckCollisionPointRec(mousePos, addTaskBtn);
                    DrawRectangleRounded(addTaskBtn, 0.2f, 10, addTaskHovered ? HexToColor("#10B981") : HexToColor("#D1FAE5"));
                    DrawText("+ Task", addTaskBtn.x + 25, addTaskBtn.y + 6, 16, addTaskHovered ? WHITE : HexToColor("#065F46"));
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && addTaskHovered) { reset_inputs(); target_cat_idx = i; edit_state = EDIT_ADD_TASK; }
                    content_y += 45;
                }
            }

            Rectangle addCatBtn = { popupRect.x + 30, content_y, 140, 40 };
            bool addCatHovered = inScrollArea && CheckCollisionPointRec(mousePos, addCatBtn);
            DrawRectangleRounded(addCatBtn, 0.2f, 10, addCatHovered ? HexToColor("#10B981") : HexToColor("#059669"));
            DrawText("+ Category", addCatBtn.x + 20, addCatBtn.y + 10, 18, WHITE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && addCatHovered) { reset_inputs(); edit_state = EDIT_ADD_CAT; }
            content_y += 60;

            max_scroll_limit = scrollArea.height - (content_y - edit_scroll_y);
            if (max_scroll_limit > 0) max_scroll_limit = 0;
            if (edit_scroll_y < max_scroll_limit) edit_scroll_y = max_scroll_limit;
            EndScissorMode();
        } 
        else if (edit_state == EDIT_ADD_CAT || edit_state == EDIT_ADD_TASK) {
            Rectangle backBtn = { popupRect.x + 30, popupRect.y + 90, 40, 36 };
            if (CheckCollisionPointRec(mousePos, backBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) edit_state = EDIT_LIST;
            DrawRectangleRounded(backBtn, 0.2f, 10, CheckCollisionPointRec(mousePos, backBtn) ? HexToColor("#E2E8F0") : WHITE);
            DrawRectangleRoundedLines(backBtn, 0.2f, 10, HexToColor("#CBD5E1")); DrawText("<", backBtn.x + 12, backBtn.y + 8, 20, BLACK);

            if (edit_state == EDIT_ADD_CAT) DrawText("Create New Category", popupRect.x + 90, popupRect.y + 98, 20, HexToColor("#334155"));
            else DrawText(TextFormat("Create Task in '%s'", current_collection->folders[target_cat_idx]->name), popupRect.x + 90, popupRect.y + 98, 20, HexToColor("#334155"));

            int textOffset = 130;
            Rectangle nameRect = { popupRect.x + 30, popupRect.y + 160, popupRect.width - 60, 40 };
            DrawRectangleRounded(nameRect, 0.2f, 10, input_name_active ? WHITE : HexToColor("#F8FAFC"));
            DrawRectangleRoundedLines(nameRect, 0.2f, 10, input_name_active ? HexToColor("#3B82F6") : HexToColor("#CBD5E1"));
            DrawText("Name:", nameRect.x + 15, nameRect.y + 10, 18, HexToColor("#64748B"));
            if (input_name[0] == '\0' && !input_name_active) DrawText("Task/Category name...", nameRect.x + textOffset, nameRect.y + 10, 18, HexToColor("#94A3B8"));
            else DrawText(input_name, nameRect.x + textOffset, nameRect.y + 10, 18, HexToColor("#0F172A"));
            if (input_name_active && cursorBlink) DrawRectangle(nameRect.x + textOffset + MeasureText(input_name, 18) + 2, nameRect.y + 10, 2, 20, BLACK);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) input_name_active = CheckCollisionPointRec(mousePos, nameRect);

            if (edit_state == EDIT_ADD_CAT) {
                Rectangle timeRect = { popupRect.x + 30, popupRect.y + 220, 300, 40 };
                DrawRectangleRounded(timeRect, 0.2f, 10, input_time_active ? WHITE : HexToColor("#F8FAFC"));
                DrawRectangleRoundedLines(timeRect, 0.2f, 10, input_time_active ? HexToColor("#3B82F6") : HexToColor("#CBD5E1"));
                DrawText("Hours:", timeRect.x + 15, timeRect.y + 10, 18, HexToColor("#64748B"));
                if (input_time[0] == '\0' && !input_time_active) DrawText("0.0", timeRect.x + textOffset, timeRect.y + 10, 18, HexToColor("#94A3B8"));
                else DrawText(input_time, timeRect.x + textOffset, timeRect.y + 10, 18, HexToColor("#0F172A"));
                if (input_time_active && cursorBlink) DrawRectangle(timeRect.x + textOffset + MeasureText(input_time, 18) + 2, timeRect.y + 10, 2, 20, BLACK);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, timeRect)) { input_name_active = false; input_time_active = true; }

                DrawText("Color:", popupRect.x + 30, popupRect.y + 295, 18, HexToColor("#64748B"));
                for (int c = 0; c < PALETTE_SIZE; c++) {
                    Rectangle colorRec = { popupRect.x + textOffset + (c % 9) * 44, popupRect.y + 285 + (c / 9) * 44, 36, 36 };
                    DrawRectangleRounded(colorRec, 0.2f, 10, HexToColor(palette_hex[c]));
                    if (selected_color_idx == c) { DrawRectangleRoundedLines(colorRec, 0.2f, 10, BLACK); DrawRectangleRoundedLines((Rectangle){colorRec.x+2, colorRec.y+2, 32, 32}, 0.2f, 10, WHITE); }
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, colorRec)) selected_color_idx = c;
                }
            } else {
                Rectangle pickrect = { popupRect.x + 30, popupRect.y + 220, popupRect.width - 60, 40 };
                DrawRectangleRounded(pickrect, 0.2f, 10, input_pickrate_active ? WHITE : HexToColor("#F8FAFC"));
                DrawRectangleRoundedLines(pickrect, 0.2f, 10, input_pickrate_active ? HexToColor("#3B82F6") : HexToColor("#CBD5E1"));
                DrawText("Pickrate:", pickrect.x + 15, pickrect.y + 10, 18, HexToColor("#64748B"));
                if (input_pickrate[0] == '\0' && !input_pickrate_active) DrawText("0", pickrect.x + textOffset, pickrect.y + 10, 18, HexToColor("#94A3B8"));
                else DrawText(input_pickrate, pickrect.x + textOffset, pickrect.y + 10, 18, HexToColor("#0F172A"));
                if (input_pickrate_active && cursorBlink) DrawRectangle(pickrect.x + textOffset + MeasureText(input_pickrate, 18) + 2, pickrect.y + 10, 2, 20, BLACK);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, pickrect)) { input_name_active = false; input_pickrate_active = true; input_description_active = false; }

                Rectangle descRect = { popupRect.x + 30, popupRect.y + 280, popupRect.width - 60, 120 };
                DrawRectangleRounded(descRect, 0.1f, 10, input_description_active ? WHITE : HexToColor("#F8FAFC"));
                DrawRectangleRoundedLines(descRect, 0.1f, 10, input_description_active ? HexToColor("#3B82F6") : HexToColor("#CBD5E1"));
                DrawText("Desc:", descRect.x + 15, descRect.y + 10, 18, HexToColor("#64748B"));
                if (input_description[0] == '\0' && !input_description_active) DrawText("Detailed description...", descRect.x + textOffset, descRect.y + 10, 18, HexToColor("#94A3B8"));
                else DrawText(input_description, descRect.x + textOffset, descRect.y + 10, 18, HexToColor("#0F172A"));
                if (input_description_active && cursorBlink) DrawRectangle(descRect.x + textOffset + MeasureText(input_description, 18) + 2, descRect.y + 10, 2, 20, BLACK);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, descRect)) { input_name_active = false; input_pickrate_active = false; input_description_active = true; }
            }

            Rectangle saveRect = { popupRect.x + popupRect.width - 150, popupRect.y + popupRect.height - 70, 120, 45 };
            bool saveHovered = CheckCollisionPointRec(mousePos, saveRect);
            DrawRectangleRounded(saveRect, 0.2f, 10, saveHovered ? HexToColor("#059669") : HexToColor("#10B981"));
            DrawText("Save", saveRect.x + 38, saveRect.y + 12, 20, WHITE);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && saveHovered) {
                if (input_name[0] == '\0') snprintf(popup_error_message, sizeof(popup_error_message), "Name is required.");
                else {
                    if (edit_state == EDIT_ADD_CAT) {
                        float time_val = (input_time[0] != '\0') ? atof(input_time) : 0.0f;
                        if (!create_category_on_disk(current_collection_path, input_name, time_val, palette_hex[selected_color_idx])) snprintf(popup_error_message, sizeof(popup_error_message), "Error saving.");
                        else { reload_collection(collection, appState); edit_state = EDIT_LIST; }
                    } else {
                        int pick = (input_pickrate[0] != '\0') ? atoi(input_pickrate) : 0;
                        if (!append_task_to_category(current_collection_path, current_collection->folders[target_cat_idx]->name, input_name, pick, input_description)) snprintf(popup_error_message, sizeof(popup_error_message), "Error saving.");
                        else { reload_collection(collection, appState); edit_state = EDIT_LIST; }
                    }
                }
            }
            if (popup_error_message[0] != '\0') DrawText(popup_error_message, popupRect.x + 30, popupRect.y + popupRect.height - 80, 18, RED);

        } else if (edit_state == EDIT_DEL_CAT_CONFIRM || edit_state == EDIT_DEL_TASK_CONFIRM) {
            DrawText("Are you sure you want to delete this?", popupRect.x + 250, popupRect.y + 200, 20, HexToColor("#0F172A"));
            Rectangle yesRect = { popupRect.x + 280, popupRect.y + 260, 120, 45 }, noRect = { popupRect.x + 480, popupRect.y + 260, 120, 45 };
            
            bool yesHov = CheckCollisionPointRec(mousePos, yesRect), noHov = CheckCollisionPointRec(mousePos, noRect);
            DrawRectangleRounded(yesRect, 0.2f, 10, yesHov ? HexToColor("#DC2626") : HexToColor("#EF4444")); DrawText("DELETE", yesRect.x + 25, yesRect.y + 12, 20, WHITE);
            DrawRectangleRounded(noRect, 0.2f, 10, noHov ? HexToColor("#E2E8F0") : HexToColor("#F1F5F9")); DrawText("CANCEL", noRect.x + 22, noRect.y + 12, 20, HexToColor("#0F172A"));

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (noHov) edit_state = EDIT_LIST;
                if (yesHov) {
                    if (edit_state == EDIT_DEL_CAT_CONFIRM) delete_category_on_disk(current_collection_path, current_collection->folders[target_cat_idx]->name);
                    else delete_task_on_disk(current_collection_path, current_collection->folders[target_cat_idx]->name, current_collection->folders[target_cat_idx]->tasks[target_task_idx]->name);
                    reload_collection(collection, appState); edit_state = EDIT_LIST;
                }
            }
        }
    }

    if (appState->confirm_reset_all || appState->confirm_reset_task) {
        DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Fade(BLACK, 0.6f));
        Rectangle popupRect = { listX, WINDOW_HEIGHT / 2 - 100, 440, 200 };
        DrawShadow(popupRect, 0.05f, 10, 5.0f);
        DrawRectangleRounded(popupRect, 0.05f, 10, WHITE); 
        
        DrawText(appState->confirm_reset_all ? "Reset all task pick rates" : "Reset this task pick rate", popupRect.x + 30, popupRect.y + 30, 20, HexToColor("#0F172A"));
        DrawText(appState->confirm_reset_all ? "This resets the pick rate for every task." : "This resets the pick rate for the current task.", popupRect.x + 30, popupRect.y + 65, 18, HexToColor("#64748B"));
        DrawText("Proceed?", popupRect.x + 30, popupRect.y + 90, 18, HexToColor("#64748B"));

        Rectangle yesRect = { popupRect.x + 30, popupRect.y + 130, 120, 40 }, noRect = { popupRect.x + 170, popupRect.y + 130, 120, 40 };
        bool yesHov = CheckCollisionPointRec(mousePos, yesRect), noHov = CheckCollisionPointRec(mousePos, noRect);

        DrawRectangleRounded(yesRect, 0.2f, 10, yesHov ? HexToColor("#059669") : HexToColor("#10B981")); DrawText("YES", yesRect.x + 40, yesRect.y + 10, 20, WHITE);
        DrawRectangleRounded(noRect, 0.2f, 10, noHov ? HexToColor("#DC2626") : HexToColor("#EF4444")); DrawText("NO", noRect.x + 45, noRect.y + 10, 20, WHITE);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (yesHov) {
                if (appState->confirm_reset_all) reset_collection_pickrate_all(current_collection, current_collection_path);
                else {
                    char fp[PATH_MAX + 512]; snprintf(fp, sizeof(fp), "%s/%s", current_collection_path, current_collection->folders[appState->selected_display_index]->name);
                    reset_task_pickrate(appState->chosen_tasks->tasks[appState->selected_display_index], current_collection->folders[appState->selected_display_index], fp);
                }
                appState->confirm_reset_all = false; appState->confirm_reset_task = false;
            }
            if (noHov) { appState->confirm_reset_all = false; appState->confirm_reset_task = false; }
        }
    }
    EndDrawing();
}
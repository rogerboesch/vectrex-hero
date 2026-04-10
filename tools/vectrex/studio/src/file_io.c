/*
 * file_io.c — JSON project load/save for Vectrex Workbench
 *
 * Uses shared project_io.h for parsing hero.json.
 */

#include "app.h"
#include "native_macos.h"
#include "json.h"
#include "project_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void app_new_project(App *app) {
    project_init(&app->project);
    app->project_path[0] = 0;
    app->cur_level = 0;
    app->cur_room = 0;
    app->modified = false;
    app_log_info(app, "New project created");
}

void app_load_project(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { app_log_err(app, "Cannot open %s", path); return; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *json = malloc(len + 1); fread(json, 1, len, f); json[len] = 0; fclose(f);

    project_init(&app->project);
    project_parse_json(json, &app->project);
    free(json);

    strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    app->cur_level = 0;
    app->cur_room = app->project.levels[0].room_count > 0 ? 0 : -1;
    app->cur_sprite = 0;
    app->cur_frame = 0;
    app->modified = false;
    app_log_info(app, "Loaded: %s (%d levels, %d sprites)", path,
                 app->project.level_count, app->project.sprite_count);
}

void app_open_project(App *app) {
    char *path = dialog_open_file("Open Project", "json");
    if (path) { app_load_project(app, path); free(path); }
}

void app_save_project(App *app, const char *path) {
    if (path) strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    if (!app->project_path[0]) { app_save_project_as(app); return; }
    /* TODO: full JSON save */
    app_log_info(app, "Save not yet implemented");
}

void app_save_project_as(App *app) {
    char *path = dialog_save_file("Save Project", "hero.json");
    if (path) { app_save_project(app, path); free(path); }
}

void app_export(App *app) {
    char *dir = dialog_open_folder("Export to directory");
    if (dir) {
        /* TODO: export levels.h, sprites.h */
        app_log_info(app, "Export not yet implemented");
        free(dir);
    }
}

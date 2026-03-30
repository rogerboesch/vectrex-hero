/*
 * iql_platform.c — Platform stubs replacing Apple UI (rb_macapp.m, rb_platform.m)
 *
 * Provides the platform functions the iQL emulator core expects, without
 * any Objective-C or Apple UI dependencies. Used when embedding the
 * emulator in a Python/Tkinter tool.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "rb_logger.h"

/* Configurable system path — set from Python before calling QLInit */
static char iql_system_path[512] = "";
static char iql_resource_path[512] = "";

void iql_set_system_path(const char *path) {
    strncpy(iql_system_path, path, sizeof(iql_system_path) - 1);
    iql_system_path[sizeof(iql_system_path) - 1] = '\0';
}

/* --- Platform path functions (replace rb_platform.m) --- */

char *rb_get_platform_system_path(void) {
    return iql_system_path;
}

char *rb_get_platform_resource_path(void) {
    if (iql_resource_path[0] == '\0') {
        return iql_system_path;
    }
    return iql_resource_path;
}

/* --- Render/UI stubs (replace rb_macapp.m) --- */

void ql_render_screen(void *buffer) {
    /* No-op: Python reads pixel_buffer directly */
    (void)buffer;
}

void ql_set_title(char *title) {
    /* No-op: Python manages its own window title */
    (void)title;
}

void platform_init(void) {
    /* Not used — Python calls QLInit() + QLStep() directly */
}

/* --- Logger (replace rb_logger.m, pure C) --- */

static RBLogLevel s_current_log_level = RB_LOG_LEVEL_INFO;

void rb_log_set_level(RBLogLevel level) {
    s_current_log_level = level;
}

RBLogLevel rb_log_get_level(void) {
    return s_current_log_level;
}

static void log_msg(FILE *out, const char *prefix, const char *file,
                    int line, const char *format, va_list args) {
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    const char *fname = strrchr(file, '/');
    fname = fname ? fname + 1 : file;

    fprintf(out, "[%02d:%02d:%02d.%03d] %s (%s:%d) ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
            (int)(tv.tv_usec / 1000), prefix, fname, line);
    vfprintf(out, format, args);
    size_t len = strlen(format);
    if (len == 0 || format[len - 1] != '\n') {
        fprintf(out, "\n");
    }
    fflush(out);
}

void rb_log_impl(RBLogLevel level, const char *file, int line,
                 const char *format, ...) {
    if (level < s_current_log_level) return;
    va_list args;
    va_start(args, format);
    log_msg(level >= RB_LOG_LEVEL_ERROR ? stderr : stdout,
            level == RB_LOG_LEVEL_DEBUG ? "DBG" :
            level == RB_LOG_LEVEL_INFO  ? "INF" : "ERR",
            file, line, format, args);
    va_end(args);
}

void rb_log_debug_impl(const char *file, int line, const char *format, ...) {
    if (RB_LOG_LEVEL_DEBUG < s_current_log_level) return;
    va_list args;
    va_start(args, format);
    log_msg(stdout, "DBG", file, line, format, args);
    va_end(args);
}

void rb_log_info_impl(const char *file, int line, const char *format, ...) {
    if (RB_LOG_LEVEL_INFO < s_current_log_level) return;
    va_list args;
    va_start(args, format);
    log_msg(stdout, "INF", file, line, format, args);
    va_end(args);
}

void rb_log_error_impl(const char *file, int line, const char *format, ...) {
    if (RB_LOG_LEVEL_ERROR < s_current_log_level) return;
    va_list args;
    va_start(args, format);
    log_msg(stderr, "ERR", file, line, format, args);
    va_end(args);
}

void rb_log_dbginfo(const char *file, int line, const char *format, ...) {
    if (RB_LOG_LEVEL_DEBUG < s_current_log_level) return;
    va_list args;
    va_start(args, format);
    log_msg(stdout, "DBG", file, line, format, args);
    va_end(args);
}

/* --- Additional platform stubs --- */

char *rb_get_temporary_path(void) {
    static char temp[512];
    snprintf(temp, sizeof(temp), "%stemp/", iql_system_path);
    return temp;
}

int rb_platform_load_file_from_cloud(const char *path) {
    /* No iCloud support in headless mode */
    (void)path;
    return 0;
}

/* Serial port baud rate table — not used but referenced by QL_serial.c */
int tty_baud[] = {
    0, 75, 150, 300, 600, 1200, 2400, 4800, 9600, 19200, 0
};

/* rb_log_register_dump is defined in QL_emulator.c */

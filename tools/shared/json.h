/*
 * json.h — Minimal JSON parsing helpers (header-only)
 *
 * No external dependencies. Parses JSON strings, ints, bools,
 * arrays, objects. Designed for hand-rolled project file parsers.
 */
#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static inline const char *json_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static inline const char *json_skip_comma(const char *p) {
    p = json_skip_ws(p);
    if (*p == ',') p++;
    return json_skip_ws(p);
}

static inline const char *json_parse_str(const char *p, char *out, int max) {
    p = json_skip_ws(p);
    if (*p == '"') p++;
    int i = 0;
    while (*p && *p != '"' && i < max - 1) out[i++] = *p++;
    out[i] = 0;
    if (*p == '"') p++;
    return p;
}

static inline const char *json_parse_int(const char *p, int *out) {
    p = json_skip_ws(p);
    *out = atoi(p);
    if (*p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;
    return p;
}

static inline const char *json_parse_bool(const char *p, bool *out) {
    p = json_skip_ws(p);
    if (strncmp(p, "true", 4) == 0) { *out = true; return p + 4; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return p + 5; }
    *out = false;
    return p;
}

/* Forward declaration needed for mutual recursion */
static inline const char *json_skip_value(const char *p);

static inline const char *json_skip_array(const char *p) {
    if (*p != '[') return p;
    p++;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '[' || *p == '{') depth++;
        else if (*p == ']' || *p == '}') depth--;
        else if (*p == '"') { p++; while (*p && *p != '"') p++; }
        p++;
    }
    return p;
}

static inline const char *json_skip_object(const char *p) {
    if (*p != '{') return p;
    p++;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') depth--;
        else if (*p == '"') { p++; while (*p && *p != '"') p++; }
        p++;
    }
    return p;
}

static inline const char *json_skip_value(const char *p) {
    p = json_skip_ws(p);
    if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p) p++; return p; }
    if (*p == '[') return json_skip_array(p);
    if (*p == '{') return json_skip_object(p);
    /* number, bool, null */
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

/* Check if current position is a null value */
static inline bool json_is_null(const char *p) {
    p = json_skip_ws(p);
    return strncmp(p, "null", 4) == 0;
}

static inline const char *json_skip_null(const char *p) {
    p = json_skip_ws(p);
    if (strncmp(p, "null", 4) == 0) return p + 4;
    return p;
}

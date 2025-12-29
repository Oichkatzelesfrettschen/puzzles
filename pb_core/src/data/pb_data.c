/**
 * @file pb_data.c
 * @brief JSON data loading implementation using cJSON
 */

#include "pb/pb_data.h"
#include "pb/pb_board.h"
#include "pb/pb_color.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/* C99-safe error setting - use snprintf with at least one format arg */
#define SET_ERROR(res, fmt, ...) do { \
    if (res) { \
        snprintf((res)->error, PB_DATA_ERROR_MAX, fmt, __VA_ARGS__); \
        (res)->success = false; \
    } \
} while(0)

/* For messages without format args, pass 0 as dummy */
#define SET_ERROR_MSG(res, msg) SET_ERROR(res, "%s", msg)

#define SAFE_STRNCPY(dst, src, n) do { \
    strncpy(dst, src ? src : "", (n) - 1); \
    dst[(n) - 1] = '\0'; \
} while(0)

/* ============================================================================
 * JSON Helpers
 * ============================================================================ */

static const char* json_get_string(const cJSON* obj, const char* key,
                                   const char* def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    return def;
}

static int json_get_int(const cJSON* obj, const char* key, int def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return def;
}

static double json_get_number(const cJSON* obj, const char* key, double def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return def;
}

static bool json_get_bool(const cJSON* obj, const char* key, bool def) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return def;
}

static char* read_file_contents(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    char* buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, f);
    buffer[read] = '\0';
    fclose(f);

    return buffer;
}

/* ============================================================================
 * Level Loading
 * ============================================================================ */

static pb_bubble parse_bubble(const cJSON* item) {
    pb_bubble b = {0};

    if (cJSON_IsNull(item)) {
        b.kind = PB_KIND_NONE;
        return b;
    }

    const char* kind = json_get_string(item, "kind", "colored");
    if (strcmp(kind, "colored") == 0) {
        b.kind = PB_KIND_COLORED;
    } else if (strcmp(kind, "special") == 0) {
        b.kind = PB_KIND_SPECIAL;
    } else if (strcmp(kind, "blocker") == 0) {
        b.kind = PB_KIND_BLOCKER;
    } else if (strcmp(kind, "wildcard") == 0) {
        b.kind = PB_KIND_WILDCARD;
    }

    b.color_id = (uint8_t)json_get_int(item, "color", 0);

    const char* special = json_get_string(item, "special", NULL);
    if (special) {
        if (strcmp(special, "bomb") == 0) b.special = PB_SPECIAL_BOMB;
        else if (strcmp(special, "lightning") == 0) b.special = PB_SPECIAL_LIGHTNING;
        else if (strcmp(special, "star") == 0) b.special = PB_SPECIAL_STAR;
        else if (strcmp(special, "magnetic") == 0) b.special = PB_SPECIAL_MAGNETIC;
        else if (strcmp(special, "rainbow") == 0) b.special = PB_SPECIAL_RAINBOW;
    }

    /* Parse flags */
    const cJSON* flags = cJSON_GetObjectItemCaseSensitive(item, "flags");
    if (cJSON_IsArray(flags)) {
        const cJSON* flag;
        cJSON_ArrayForEach(flag, flags) {
            if (!cJSON_IsString(flag)) continue;
            if (strcmp(flag->valuestring, "indestructible") == 0) {
                b.flags |= PB_FLAG_INDESTRUCTIBLE;
            } else if (strcmp(flag->valuestring, "sticky") == 0) {
                b.flags |= PB_FLAG_STICKY;
            } else if (strcmp(flag->valuestring, "ghost") == 0) {
                b.flags |= PB_FLAG_GHOST;
            } else if (strcmp(flag->valuestring, "frozen") == 0) {
                b.flags |= PB_FLAG_FROZEN;
            } else if (strcmp(flag->valuestring, "anchor") == 0) {
                b.flags |= PB_FLAG_ANCHOR;
            }
        }
    }

    return b;
}

bool pb_level_load_string(const char* json, pb_level_data* level,
                          pb_data_result* result) {
    if (!json || !level) {
        SET_ERROR_MSG(result, "NULL parameter");
        return false;
    }

    memset(level, 0, sizeof(*level));

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        const char* err = cJSON_GetErrorPtr();
        SET_ERROR(result, "JSON parse error near: %.20s", err ? err : "unknown");
        return false;
    }

    /* Version */
    SAFE_STRNCPY(level->version, json_get_string(root, "version", "1.0"),
                 sizeof(level->version));

    /* Name (required) */
    const char* name = json_get_string(root, "name", NULL);
    if (!name) {
        SET_ERROR_MSG(result, "Missing required field: name");
        cJSON_Delete(root);
        return false;
    }
    SAFE_STRNCPY(level->name, name, sizeof(level->name));

    /* Optional fields */
    SAFE_STRNCPY(level->author, json_get_string(root, "author", ""),
                 sizeof(level->author));
    level->difficulty = json_get_int(root, "difficulty", 5);

    /* Grid (required) */
    const cJSON* grid = cJSON_GetObjectItemCaseSensitive(root, "grid");
    if (!cJSON_IsObject(grid)) {
        SET_ERROR_MSG(result, "Missing required field: grid");
        cJSON_Delete(root);
        return false;
    }

    level->cols_even = json_get_int(grid, "cols_even", 8);
    level->cols_odd = json_get_int(grid, "cols_odd", 7);
    level->rows = json_get_int(grid, "rows", 12);

    /* Parse bubbles array */
    const cJSON* bubbles = cJSON_GetObjectItemCaseSensitive(grid, "bubbles");
    if (!cJSON_IsArray(bubbles)) {
        SET_ERROR_MSG(result, "Missing required field: grid.bubbles");
        cJSON_Delete(root);
        return false;
    }

    int array_size = cJSON_GetArraySize(bubbles);
    level->bubble_count = array_size;
    level->bubbles = calloc((size_t)array_size, sizeof(pb_bubble));

    if (!level->bubbles) {
        SET_ERROR_MSG(result, "Memory allocation failed");
        cJSON_Delete(root);
        return false;
    }

    int idx = 0;
    const cJSON* item;
    cJSON_ArrayForEach(item, bubbles) {
        level->bubbles[idx++] = parse_bubble(item);
    }

    /* Objectives */
    const cJSON* objectives = cJSON_GetObjectItemCaseSensitive(root, "objectives");
    if (cJSON_IsObject(objectives)) {
        level->clear_all = json_get_bool(objectives, "clear_all", true);
        level->target_score = json_get_int(objectives, "target_score", 0);
        level->max_shots = json_get_int(objectives, "max_shots", 0);
        level->time_limit_sec = json_get_int(objectives, "time_limit_seconds", 0);
    } else {
        level->clear_all = true;
    }

    /* Optional theme */
    SAFE_STRNCPY(level->theme_id, json_get_string(root, "theme", ""),
                 sizeof(level->theme_id));

    cJSON_Delete(root);

    if (result) {
        result->success = true;
        result->error[0] = '\0';
    }
    return true;
}

bool pb_level_load_file(const char* path, pb_level_data* level,
                        pb_data_result* result) {
    char* json = read_file_contents(path);
    if (!json) {
        SET_ERROR(result, "Failed to read file: %s", path);
        return false;
    }

    bool ok = pb_level_load_string(json, level, result);
    free(json);
    return ok;
}

void pb_level_data_free(pb_level_data* level) {
    if (level) {
        free(level->bubbles);
        level->bubbles = NULL;
        level->bubble_count = 0;
    }
}

void pb_level_to_board(const pb_level_data* level, pb_board* board) {
    if (!level || !board) return;

    pb_board_init_custom(board, level->rows, level->cols_even, level->cols_odd);

    /* Load bubbles row by row */
    int idx = 0;
    for (int row = 0; row < level->rows && idx < level->bubble_count; row++) {
        int cols = (row % 2 == 0) ? level->cols_even : level->cols_odd;
        for (int col = 0; col < cols && idx < level->bubble_count; col++) {
            pb_offset pos = { row, col };
            pb_board_set(board, pos, level->bubbles[idx++]);
        }
    }
}

/* ============================================================================
 * Theme Loading
 * ============================================================================ */

static pb_rgb8 parse_color_hex(const char* hex) {
    pb_rgb8 rgb = {0};
    pb_hex_to_rgb8(hex, &rgb);
    return rgb;
}

bool pb_theme_load_string(const char* json, pb_theme_data* theme,
                          pb_data_result* result) {
    if (!json || !theme) {
        SET_ERROR_MSG(result, "NULL parameter");
        return false;
    }

    memset(theme, 0, sizeof(*theme));

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        SET_ERROR_MSG(result, "JSON parse error");
        return false;
    }

    SAFE_STRNCPY(theme->version, json_get_string(root, "version", "1.0"),
                 sizeof(theme->version));
    SAFE_STRNCPY(theme->name, json_get_string(root, "name", "Default"),
                 sizeof(theme->name));
    SAFE_STRNCPY(theme->author, json_get_string(root, "author", ""),
                 sizeof(theme->author));

    /* Parse palette */
    const cJSON* palette = cJSON_GetObjectItemCaseSensitive(root, "palette");
    if (cJSON_IsObject(palette)) {
        const cJSON* colors = cJSON_GetObjectItemCaseSensitive(palette, "colors");
        if (cJSON_IsArray(colors)) {
            const cJSON* color;
            int i = 0;
            cJSON_ArrayForEach(color, colors) {
                if (i >= 8) break;

                SAFE_STRNCPY(theme->colors[i].name,
                             json_get_string(color, "name", ""),
                             sizeof(theme->colors[i].name));

                const char* srgb = json_get_string(color, "srgb", "#808080");
                theme->colors[i].srgb = parse_color_hex(srgb);

                /* Compute OKLCH from sRGB */
                pb_rgb rgb = pb_rgb8_to_rgb(theme->colors[i].srgb);
                theme->colors[i].oklch = pb_srgb_to_oklch(rgb);

                /* Outline color */
                const char* outline = json_get_string(color, "outline", NULL);
                if (outline) {
                    theme->colors[i].outline = parse_color_hex(outline);
                } else {
                    /* Auto-generate darker outline */
                    pb_oklch darker = theme->colors[i].oklch;
                    darker.L *= 0.7f;
                    darker = pb_oklch_clip_gamut(darker);
                    theme->colors[i].outline = pb_rgb_to_rgb8(pb_oklch_to_srgb(darker));
                }

                /* Pattern */
                theme->colors[i].pattern = (pb_pattern_id)json_get_int(color, "pattern_id", 0);

                i++;
            }
            theme->color_count = i;
        }

        /* Background colors */
        const char* bg = json_get_string(palette, "background", "#1a1a2e");
        theme->background = parse_color_hex(bg);

        const char* grid = json_get_string(palette, "grid_line", "#333366");
        theme->grid_line = parse_color_hex(grid);

        const char* text = json_get_string(palette, "text", "#ffffff");
        theme->text = parse_color_hex(text);

        const char* hl = json_get_string(palette, "highlight", "#ffff00");
        theme->highlight = parse_color_hex(hl);
    }

    /* Visual style */
    const cJSON* style = cJSON_GetObjectItemCaseSensitive(root, "visual_style");
    if (cJSON_IsObject(style)) {
        theme->bubble_outline_width = (float)json_get_number(style, "bubble_outline_width", 2.0);
        theme->bubble_shine = json_get_bool(style, "bubble_shine", true);
        theme->grid_visible = json_get_bool(style, "grid_visible", false);
        SAFE_STRNCPY(theme->trajectory_style,
                     json_get_string(style, "trajectory_style", "dotted"),
                     sizeof(theme->trajectory_style));
    } else {
        theme->bubble_outline_width = 2.0f;
        theme->bubble_shine = true;
        theme->grid_visible = false;
        strcpy(theme->trajectory_style, "dotted");
    }

    /* CVD safety info */
    const cJSON* cvd = cJSON_GetObjectItemCaseSensitive(root, "cvd_safe");
    if (cJSON_IsObject(cvd)) {
        theme->cvd_safe.verified = json_get_bool(cvd, "verified", false);
        theme->cvd_safe.min_contrast = (float)json_get_number(cvd, "min_contrast", 0.0);

        const cJSON* sims = cJSON_GetObjectItemCaseSensitive(cvd, "simulations");
        if (cJSON_IsObject(sims)) {
            const cJSON* protan = cJSON_GetObjectItemCaseSensitive(sims, "protanopia");
            if (protan) theme->cvd_safe.protanopia_safe = json_get_bool(protan, "pass", false);

            const cJSON* deutan = cJSON_GetObjectItemCaseSensitive(sims, "deuteranopia");
            if (deutan) theme->cvd_safe.deuteranopia_safe = json_get_bool(deutan, "pass", false);

            const cJSON* tritan = cJSON_GetObjectItemCaseSensitive(sims, "tritanopia");
            if (tritan) theme->cvd_safe.tritanopia_safe = json_get_bool(tritan, "pass", false);
        }
    }

    cJSON_Delete(root);

    if (result) {
        result->success = true;
        result->error[0] = '\0';
    }
    return true;
}

bool pb_theme_load_file(const char* path, pb_theme_data* theme,
                        pb_data_result* result) {
    char* json = read_file_contents(path);
    if (!json) {
        SET_ERROR(result, "Failed to read file: %s", path);
        return false;
    }

    bool ok = pb_theme_load_string(json, theme, result);
    free(json);
    return ok;
}

void pb_theme_get_default(pb_theme_data* theme) {
    if (!theme) return;

    memset(theme, 0, sizeof(*theme));
    strcpy(theme->version, "1.0");
    strcpy(theme->name, "Classic");
    strcpy(theme->author, "pb_core");

    /* Classic Puzzle Bobble colors */
    const char* classic_colors[] = {
        "#E63946",  /* Red */
        "#2196F3",  /* Blue */
        "#4CAF50",  /* Green */
        "#FFEB3B",  /* Yellow */
        "#9C27B0",  /* Purple */
        "#FF9800",  /* Orange */
        "#00BCD4",  /* Cyan */
        "#607D8B"   /* Gray */
    };

    const char* color_names[] = {
        "red", "blue", "green", "yellow",
        "purple", "orange", "cyan", "gray"
    };

    pb_pattern_map default_patterns;
    pb_pattern_get_default_map(&default_patterns);

    for (int i = 0; i < 8; i++) {
        strcpy(theme->colors[i].name, color_names[i]);
        theme->colors[i].srgb = parse_color_hex(classic_colors[i]);

        pb_rgb rgb = pb_rgb8_to_rgb(theme->colors[i].srgb);
        theme->colors[i].oklch = pb_srgb_to_oklch(rgb);

        pb_oklch darker = theme->colors[i].oklch;
        darker.L *= 0.6f;
        darker = pb_oklch_clip_gamut(darker);
        theme->colors[i].outline = pb_rgb_to_rgb8(pb_oklch_to_srgb(darker));

        theme->colors[i].pattern = default_patterns.patterns[i];
    }
    theme->color_count = 8;

    theme->background = parse_color_hex("#1a1a2e");
    theme->grid_line = parse_color_hex("#333366");
    theme->text = parse_color_hex("#ffffff");
    theme->highlight = parse_color_hex("#ffff00");

    theme->bubble_outline_width = 2.0f;
    theme->bubble_shine = true;
    theme->grid_visible = false;
    strcpy(theme->trajectory_style, "dotted");
}

/* ============================================================================
 * Ruleset Loading
 * ============================================================================ */

static void ruleset_set_defaults(pb_ruleset* ruleset) {
    ruleset->mode = PB_MODE_PUZZLE;
    ruleset->match_threshold = PB_DEFAULT_MATCH_THRESHOLD;
    ruleset->cols_even = PB_DEFAULT_COLS_EVEN;
    ruleset->cols_odd = PB_DEFAULT_COLS_ODD;
    ruleset->rows = PB_DEFAULT_ROWS;
    ruleset->max_bounces = 2;
    ruleset->shots_per_row_insert = 0;
    ruleset->initial_rows = 4;
    ruleset->lose_on = PB_LOSE_OVERFLOW;
    ruleset->allow_color_switch = true;
    ruleset->restrict_colors_to_board = true;
    ruleset->allowed_colors = 0xFF;     /* All 8 colors enabled */
    ruleset->allowed_specials = 0x00;   /* No specials by default */
}

bool pb_ruleset_load_string(const char* json, pb_ruleset* ruleset,
                            pb_data_result* result) {
    if (!json || !ruleset) {
        SET_ERROR_MSG(result, "NULL parameter");
        return false;
    }

    /* Start with defaults */
    ruleset_set_defaults(ruleset);

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        SET_ERROR_MSG(result, "JSON parse error");
        return false;
    }

    /* Mode */
    const char* mode = json_get_string(root, "mode", "puzzle");
    if (strcmp(mode, "arcade") == 0) ruleset->mode = PB_MODE_ARCADE;
    else if (strcmp(mode, "survival") == 0) ruleset->mode = PB_MODE_SURVIVAL;
    else if (strcmp(mode, "time_attack") == 0) ruleset->mode = PB_MODE_TIME_ATTACK;
    else if (strcmp(mode, "versus") == 0) ruleset->mode = PB_MODE_VERSUS;
    else if (strcmp(mode, "coop") == 0) ruleset->mode = PB_MODE_COOP;
    else if (strcmp(mode, "zen") == 0) ruleset->mode = PB_MODE_ZEN;
    else ruleset->mode = PB_MODE_PUZZLE;

    /* Grid */
    const cJSON* grid = cJSON_GetObjectItemCaseSensitive(root, "grid");
    if (cJSON_IsObject(grid)) {
        ruleset->cols_even = json_get_int(grid, "cols_even", PB_DEFAULT_COLS_EVEN);
        ruleset->cols_odd = json_get_int(grid, "cols_odd", PB_DEFAULT_COLS_ODD);
        ruleset->rows = json_get_int(grid, "rows", PB_DEFAULT_ROWS);
    }

    /* Mechanics */
    const cJSON* mechanics = cJSON_GetObjectItemCaseSensitive(root, "mechanics");
    if (cJSON_IsObject(mechanics)) {
        ruleset->match_threshold = json_get_int(mechanics, "match_threshold", 3);
        ruleset->max_bounces = json_get_int(mechanics, "max_bounces", 2);
        ruleset->allow_color_switch = json_get_bool(mechanics, "allow_color_switch", true);
        ruleset->restrict_colors_to_board = json_get_bool(mechanics, "restrict_colors_to_board", true);
    }

    /* Pressure */
    const cJSON* pressure = cJSON_GetObjectItemCaseSensitive(root, "pressure");
    if (cJSON_IsObject(pressure)) {
        ruleset->shots_per_row_insert = json_get_int(pressure, "shots_per_row_insert", 0);
        ruleset->initial_rows = json_get_int(pressure, "initial_rows", 4);
    }

    /* Lose condition */
    const char* lose_on = json_get_string(root, "lose_on", "overflow");
    if (strcmp(lose_on, "timeout") == 0) ruleset->lose_on = PB_LOSE_TIMEOUT;
    else if (strcmp(lose_on, "shots_exhausted") == 0) ruleset->lose_on = PB_LOSE_SHOTS_EXHAUSTED;
    else ruleset->lose_on = PB_LOSE_OVERFLOW;

    /* Allowed colors/specials bitmasks */
    ruleset->allowed_colors = (uint8_t)json_get_int(root, "allowed_colors", 0xFF);
    ruleset->allowed_specials = (uint8_t)json_get_int(root, "allowed_specials", 0x00);

    cJSON_Delete(root);

    if (result) {
        result->success = true;
        result->error[0] = '\0';
    }
    return true;
}

bool pb_ruleset_load_file(const char* path, pb_ruleset* ruleset,
                          pb_data_result* result) {
    char* json = read_file_contents(path);
    if (!json) {
        SET_ERROR(result, "Failed to read file: %s", path);
        return false;
    }

    bool ok = pb_ruleset_load_string(json, ruleset, result);
    free(json);
    return ok;
}

/* ============================================================================
 * Replay Loading/Saving
 * ============================================================================ */

static pb_event_type parse_event_type(const char* type) {
    if (strcmp(type, "fire") == 0) return PB_EVENT_FIRE;
    if (strcmp(type, "rotate_left") == 0) return PB_EVENT_ROTATE_LEFT;
    if (strcmp(type, "rotate_right") == 0) return PB_EVENT_ROTATE_RIGHT;
    if (strcmp(type, "switch_bubble") == 0) return PB_EVENT_SWITCH_BUBBLE;
    if (strcmp(type, "pause") == 0) return PB_EVENT_PAUSE;
    if (strcmp(type, "unpause") == 0) return PB_EVENT_UNPAUSE;
    return PB_EVENT_NONE;
}

static const char* event_type_name(pb_event_type type) {
    switch (type) {
    case PB_EVENT_FIRE: return "fire";
    case PB_EVENT_ROTATE_LEFT: return "rotate_left";
    case PB_EVENT_ROTATE_RIGHT: return "rotate_right";
    case PB_EVENT_SWITCH_BUBBLE: return "switch_bubble";
    case PB_EVENT_PAUSE: return "pause";
    case PB_EVENT_UNPAUSE: return "unpause";
    default: return "unknown";
    }
}

bool pb_replay_load_string(const char* json, pb_replay_data* replay,
                           pb_data_result* result) {
    if (!json || !replay) {
        SET_ERROR_MSG(result, "NULL parameter");
        return false;
    }

    memset(replay, 0, sizeof(*replay));

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        SET_ERROR_MSG(result, "JSON parse error");
        return false;
    }

    SAFE_STRNCPY(replay->version, json_get_string(root, "version", "1.0"),
                 sizeof(replay->version));

    /* Seed */
    replay->seed = (uint64_t)json_get_int(root, "seed", 0);

    const char* seed_hex = json_get_string(root, "seed_hex", NULL);
    if (seed_hex) {
        replay->seed = strtoull(seed_hex, NULL, 16);
    }

    /* Metadata */
    const cJSON* meta = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    if (cJSON_IsObject(meta)) {
        SAFE_STRNCPY(replay->metadata.level_id, json_get_string(meta, "level_id", ""),
                     sizeof(replay->metadata.level_id));
        SAFE_STRNCPY(replay->metadata.level_name, json_get_string(meta, "level_name", ""),
                     sizeof(replay->metadata.level_name));
        SAFE_STRNCPY(replay->metadata.ruleset_id, json_get_string(meta, "ruleset_id", ""),
                     sizeof(replay->metadata.ruleset_id));
        SAFE_STRNCPY(replay->metadata.player_name, json_get_string(meta, "player_name", ""),
                     sizeof(replay->metadata.player_name));
        SAFE_STRNCPY(replay->metadata.recorded_at, json_get_string(meta, "recorded_at", ""),
                     sizeof(replay->metadata.recorded_at));
        replay->metadata.duration_frames = (uint32_t)json_get_int(meta, "duration_frames", 0);
        replay->metadata.final_score = json_get_int(meta, "final_score", 0);
        SAFE_STRNCPY(replay->metadata.outcome, json_get_string(meta, "outcome", ""),
                     sizeof(replay->metadata.outcome));
    }

    /* Events */
    const cJSON* events = cJSON_GetObjectItemCaseSensitive(root, "events");
    if (cJSON_IsArray(events)) {
        int count = cJSON_GetArraySize(events);
        replay->events = calloc((size_t)count, sizeof(pb_replay_event));
        if (!replay->events) {
            SET_ERROR_MSG(result, "Memory allocation failed");
            cJSON_Delete(root);
            return false;
        }

        const cJSON* ev;
        int i = 0;
        cJSON_ArrayForEach(ev, events) {
            replay->events[i].frame = (uint32_t)json_get_int(ev, "frame", 0);
            replay->events[i].type = parse_event_type(json_get_string(ev, "type", ""));

            const cJSON* data = cJSON_GetObjectItemCaseSensitive(ev, "data");
            if (cJSON_IsObject(data)) {
                replay->events[i].angle = PB_FLOAT_TO_FIXED((float)json_get_number(data, "angle", 0.0));
                replay->events[i].delta = PB_FLOAT_TO_FIXED((float)json_get_number(data, "delta", 0.0));
            }
            i++;
        }
        replay->event_count = i;
    }

    /* Checkpoints */
    const cJSON* checkpoints = cJSON_GetObjectItemCaseSensitive(root, "checkpoints");
    if (cJSON_IsArray(checkpoints)) {
        int count = cJSON_GetArraySize(checkpoints);
        replay->checkpoints = calloc((size_t)count, sizeof(pb_replay_checkpoint));
        if (replay->checkpoints) {
            const cJSON* cp;
            int i = 0;
            cJSON_ArrayForEach(cp, checkpoints) {
                replay->checkpoints[i].frame = (uint32_t)json_get_int(cp, "frame", 0);
                replay->checkpoints[i].checksum = (uint32_t)json_get_int(cp, "checksum", 0);
                replay->checkpoints[i].board_checksum = (uint32_t)json_get_int(cp, "board_checksum", 0);
                replay->checkpoints[i].rng_checksum = (uint32_t)json_get_int(cp, "rng_checksum", 0);
                replay->checkpoints[i].score = json_get_int(cp, "score", 0);
                i++;
            }
            replay->checkpoint_count = i;
        }
    }

    cJSON_Delete(root);

    if (result) {
        result->success = true;
        result->error[0] = '\0';
    }
    return true;
}

bool pb_replay_load_file(const char* path, pb_replay_data* replay,
                         pb_data_result* result) {
    char* json = read_file_contents(path);
    if (!json) {
        SET_ERROR(result, "Failed to read file: %s", path);
        return false;
    }

    bool ok = pb_replay_load_string(json, replay, result);
    free(json);
    return ok;
}

void pb_replay_data_free(pb_replay_data* replay) {
    if (replay) {
        free(replay->events);
        free(replay->checkpoints);
        replay->events = NULL;
        replay->checkpoints = NULL;
        replay->event_count = 0;
        replay->checkpoint_count = 0;
    }
}

char* pb_replay_save_string(const pb_replay_data* replay) {
    if (!replay) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "version", replay->version);
    cJSON_AddNumberToObject(root, "seed", (double)replay->seed);

    /* Metadata */
    cJSON* meta = cJSON_AddObjectToObject(root, "metadata");
    if (meta) {
        if (replay->metadata.level_id[0])
            cJSON_AddStringToObject(meta, "level_id", replay->metadata.level_id);
        if (replay->metadata.level_name[0])
            cJSON_AddStringToObject(meta, "level_name", replay->metadata.level_name);
        if (replay->metadata.player_name[0])
            cJSON_AddStringToObject(meta, "player_name", replay->metadata.player_name);
        if (replay->metadata.recorded_at[0])
            cJSON_AddStringToObject(meta, "recorded_at", replay->metadata.recorded_at);
        cJSON_AddNumberToObject(meta, "duration_frames", replay->metadata.duration_frames);
        cJSON_AddNumberToObject(meta, "final_score", replay->metadata.final_score);
        if (replay->metadata.outcome[0])
            cJSON_AddStringToObject(meta, "outcome", replay->metadata.outcome);
    }

    /* Events */
    cJSON* events = cJSON_AddArrayToObject(root, "events");
    if (events) {
        for (int i = 0; i < replay->event_count; i++) {
            const pb_replay_event* ev = &replay->events[i];
            cJSON* event = cJSON_CreateObject();

            cJSON_AddNumberToObject(event, "frame", ev->frame);
            cJSON_AddStringToObject(event, "type", event_type_name(ev->type));

            if (ev->type == PB_EVENT_FIRE || ev->type == PB_EVENT_ROTATE_LEFT ||
                ev->type == PB_EVENT_ROTATE_RIGHT) {
                cJSON* data = cJSON_AddObjectToObject(event, "data");
                if (ev->type == PB_EVENT_FIRE) {
                    cJSON_AddNumberToObject(data, "angle", PB_FIXED_TO_FLOAT(ev->angle));
                } else {
                    cJSON_AddNumberToObject(data, "delta", PB_FIXED_TO_FLOAT(ev->delta));
                }
            }

            cJSON_AddItemToArray(events, event);
        }
    }

    /* Checkpoints */
    if (replay->checkpoint_count > 0) {
        cJSON* checkpoints = cJSON_AddArrayToObject(root, "checkpoints");
        if (checkpoints) {
            for (int i = 0; i < replay->checkpoint_count; i++) {
                const pb_replay_checkpoint* cp = &replay->checkpoints[i];
                cJSON* checkpoint = cJSON_CreateObject();

                cJSON_AddNumberToObject(checkpoint, "frame", cp->frame);
                cJSON_AddNumberToObject(checkpoint, "checksum", cp->checksum);
                cJSON_AddNumberToObject(checkpoint, "board_checksum", cp->board_checksum);
                cJSON_AddNumberToObject(checkpoint, "score", cp->score);

                cJSON_AddItemToArray(checkpoints, checkpoint);
            }
        }
    }

    char* str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;
}

bool pb_replay_save_file(const char* path, const pb_replay_data* replay,
                         pb_data_result* result) {
    char* json = pb_replay_save_string(replay);
    if (!json) {
        SET_ERROR_MSG(result, "Failed to serialize replay");
        return false;
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        SET_ERROR(result, "Failed to open file for writing: %s", path);
        free(json);
        return false;
    }

    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, f);
    fclose(f);
    free(json);

    if (written != len) {
        SET_ERROR_MSG(result, "Failed to write complete file");
        return false;
    }

    if (result) {
        result->success = true;
        result->error[0] = '\0';
    }
    return true;
}

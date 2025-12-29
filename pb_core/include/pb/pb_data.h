/**
 * @file pb_data.h
 * @brief JSON data loading for levels, themes, rulesets, and replays
 *
 * Provides functions to load game data from JSON files or strings,
 * using cJSON for parsing. All loaded data is validated against
 * expected schema structure.
 */

#ifndef PB_DATA_H
#define PB_DATA_H

#include "pb_types.h"
#include "pb_color.h"
#include "pb_pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/** Maximum length for error messages */
#define PB_DATA_ERROR_MAX 256

/** Data loading result */
typedef struct pb_data_result {
    bool success;
    char error[PB_DATA_ERROR_MAX];
    int error_line;  /* JSON line number if parse error */
} pb_data_result;

/* ============================================================================
 * Level Data
 * ============================================================================ */

/** Loaded level data */
typedef struct pb_level_data {
    char version[16];
    char name[65];
    char author[65];
    int difficulty;

    int cols_even;
    int cols_odd;
    int rows;

    pb_bubble* bubbles;  /* Row-major array, NULL for empty cells */
    int bubble_count;    /* Total cells (cols * rows effectively) */

    /* Objectives */
    bool clear_all;
    int target_score;    /* 0 = no target */
    int max_shots;       /* 0 = unlimited */
    int time_limit_sec;  /* 0 = no limit */

    /* Optional */
    char theme_id[65];
    pb_ruleset ruleset_override;
    bool has_ruleset_override;
} pb_level_data;

/**
 * Load level from JSON string.
 * @param json JSON string
 * @param level Output level data (caller must call pb_level_data_free)
 * @param result Optional result/error info
 * @return true on success
 */
bool pb_level_load_string(const char* json, pb_level_data* level,
                          pb_data_result* result);

/**
 * Load level from file.
 * @param path File path
 * @param level Output level data
 * @param result Optional result/error info
 * @return true on success
 */
bool pb_level_load_file(const char* path, pb_level_data* level,
                        pb_data_result* result);

/**
 * Free level data resources.
 */
void pb_level_data_free(pb_level_data* level);

/**
 * Initialize board from level data.
 */
void pb_level_to_board(const pb_level_data* level, pb_board* board);

/* ============================================================================
 * Theme Data
 * ============================================================================ */

/** Theme color entry */
typedef struct pb_theme_color {
    char name[32];
    pb_rgb8 srgb;
    pb_oklch oklch;
    pb_rgb8 outline;
    pb_pattern_id pattern;
} pb_theme_color;

/** CVD safety info */
typedef struct pb_theme_cvd_info {
    bool verified;
    float min_contrast;
    bool protanopia_safe;
    bool deuteranopia_safe;
    bool tritanopia_safe;
} pb_theme_cvd_info;

/** Loaded theme data */
typedef struct pb_theme_data {
    char version[16];
    char name[65];
    char author[65];

    pb_theme_color colors[8];
    int color_count;

    pb_rgb8 background;
    pb_rgb8 grid_line;
    pb_rgb8 text;
    pb_rgb8 highlight;

    /* Visual style */
    float bubble_outline_width;
    bool bubble_shine;
    bool grid_visible;
    char trajectory_style[16];

    pb_theme_cvd_info cvd_safe;
} pb_theme_data;

/**
 * Load theme from JSON string.
 */
bool pb_theme_load_string(const char* json, pb_theme_data* theme,
                          pb_data_result* result);

/**
 * Load theme from file.
 */
bool pb_theme_load_file(const char* path, pb_theme_data* theme,
                        pb_data_result* result);

/**
 * Get default theme.
 */
void pb_theme_get_default(pb_theme_data* theme);

/* ============================================================================
 * Ruleset Data
 * ============================================================================ */

/**
 * Load ruleset from JSON string.
 */
bool pb_ruleset_load_string(const char* json, pb_ruleset* ruleset,
                            pb_data_result* result);

/**
 * Load ruleset from file.
 */
bool pb_ruleset_load_file(const char* path, pb_ruleset* ruleset,
                          pb_data_result* result);

/* ============================================================================
 * Replay Data
 * ============================================================================ */

/** Replay event */
typedef struct pb_replay_event {
    uint32_t frame;
    pb_event_type type;
    pb_scalar angle;   /* For fire events */
    pb_scalar delta;   /* For rotation events */
} pb_replay_event;

/** Replay checkpoint */
typedef struct pb_replay_checkpoint {
    uint32_t frame;
    uint32_t checksum;
    uint32_t board_checksum;
    uint32_t rng_checksum;
    int score;
} pb_replay_checkpoint;

/** Replay metadata */
typedef struct pb_replay_metadata {
    char level_id[65];
    char level_name[65];
    char ruleset_id[65];
    char player_name[65];
    char recorded_at[32];  /* ISO 8601 */
    uint32_t duration_frames;
    int final_score;
    char outcome[16];  /* "won", "lost", "abandoned" */
} pb_replay_metadata;

/** Loaded replay data */
typedef struct pb_replay_data {
    char version[16];
    uint64_t seed;

    pb_replay_metadata metadata;

    pb_replay_event* events;
    int event_count;

    pb_replay_checkpoint* checkpoints;
    int checkpoint_count;

    /* Optional initial state */
    bool has_initial_state;
    uint32_t initial_board_checksum;
    uint32_t initial_rng_state[4];
} pb_replay_data;

/**
 * Load replay from JSON string.
 */
bool pb_replay_load_string(const char* json, pb_replay_data* replay,
                           pb_data_result* result);

/**
 * Load replay from file.
 */
bool pb_replay_load_file(const char* path, pb_replay_data* replay,
                         pb_data_result* result);

/**
 * Free replay data resources.
 */
void pb_replay_data_free(pb_replay_data* replay);

/**
 * Save replay to JSON string.
 * @return Allocated string (caller must free)
 */
char* pb_replay_save_string(const pb_replay_data* replay);

/**
 * Save replay to file.
 */
bool pb_replay_save_file(const char* path, const pb_replay_data* replay,
                         pb_data_result* result);

#ifdef __cplusplus
}
#endif

#endif /* PB_DATA_H */

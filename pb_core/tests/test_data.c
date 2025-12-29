/**
 * @file test_data.c
 * @brief Tests for pb_data JSON loading
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pb/pb_core.h"

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { \
    tests_total++; \
    printf("  %s... ", #name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("OK\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

/* ============================================================================
 * Level Loading Tests
 * ============================================================================ */

static void test_level_minimal(void) {
    TEST(level_minimal);

    const char* json = "{"
        "\"name\": \"Test Level\","
        "\"grid\": {"
            "\"cols_even\": 8,"
            "\"cols_odd\": 7,"
            "\"rows\": 5,"
            "\"bubbles\": ["
                "{\"kind\": \"colored\", \"color\": 0},"
                "{\"kind\": \"colored\", \"color\": 1},"
                "null,"
                "{\"kind\": \"colored\", \"color\": 2}"
            "]"
        "}"
    "}";

    pb_level_data level;
    pb_data_result result = {0};

    bool ok = pb_level_load_string(json, &level, &result);
    ASSERT(ok, result.error);
    ASSERT(strcmp(level.name, "Test Level") == 0, "name mismatch");
    ASSERT(level.cols_even == 8, "cols_even mismatch");
    ASSERT(level.cols_odd == 7, "cols_odd mismatch");
    ASSERT(level.rows == 5, "rows mismatch");
    ASSERT(level.bubble_count == 4, "bubble_count mismatch");
    ASSERT(level.bubbles[0].kind == PB_KIND_COLORED, "bubble[0] kind");
    ASSERT(level.bubbles[0].color_id == 0, "bubble[0] color");
    ASSERT(level.bubbles[2].kind == PB_KIND_NONE, "null bubble kind");

    pb_level_data_free(&level);
    PASS();
}

static void test_level_objectives(void) {
    TEST(level_objectives);

    const char* json = "{"
        "\"name\": \"Objective Test\","
        "\"grid\": {\"bubbles\": []},"
        "\"objectives\": {"
            "\"clear_all\": false,"
            "\"target_score\": 1000,"
            "\"max_shots\": 50,"
            "\"time_limit_seconds\": 120"
        "}"
    "}";

    pb_level_data level;
    pb_data_result result = {0};

    bool ok = pb_level_load_string(json, &level, &result);
    ASSERT(ok, result.error);
    ASSERT(level.clear_all == false, "clear_all");
    ASSERT(level.target_score == 1000, "target_score");
    ASSERT(level.max_shots == 50, "max_shots");
    ASSERT(level.time_limit_sec == 120, "time_limit");

    pb_level_data_free(&level);
    PASS();
}

static void test_level_special_bubbles(void) {
    TEST(level_special_bubbles);

    const char* json = "{"
        "\"name\": \"Special Test\","
        "\"grid\": {"
            "\"bubbles\": ["
                "{\"kind\": \"special\", \"special\": \"bomb\"},"
                "{\"kind\": \"blocker\", \"flags\": [\"indestructible\"]},"
                "{\"kind\": \"wildcard\"}"
            "]"
        "}"
    "}";

    pb_level_data level;
    pb_data_result result = {0};

    bool ok = pb_level_load_string(json, &level, &result);
    ASSERT(ok, result.error);
    ASSERT(level.bubbles[0].kind == PB_KIND_SPECIAL, "bomb kind");
    ASSERT(level.bubbles[0].special == PB_SPECIAL_BOMB, "bomb special");
    ASSERT(level.bubbles[1].kind == PB_KIND_BLOCKER, "blocker kind");
    ASSERT(level.bubbles[1].flags & PB_FLAG_INDESTRUCTIBLE, "blocker flag");
    ASSERT(level.bubbles[2].kind == PB_KIND_WILDCARD, "wildcard kind");

    pb_level_data_free(&level);
    PASS();
}

static void test_level_to_board(void) {
    TEST(level_to_board);

    const char* json = "{"
        "\"name\": \"Board Test\","
        "\"grid\": {"
            "\"cols_even\": 3,"
            "\"cols_odd\": 2,"
            "\"rows\": 2,"
            "\"bubbles\": ["
                "{\"color\": 0}, {\"color\": 1}, {\"color\": 2},"
                "{\"color\": 3}, {\"color\": 4}"
            "]"
        "}"
    "}";

    pb_level_data level;
    pb_data_result result = {0};

    bool ok = pb_level_load_string(json, &level, &result);
    ASSERT(ok, result.error);

    pb_board board;
    pb_level_to_board(&level, &board);

    ASSERT(board.cols_even == 3, "board cols_even");
    ASSERT(board.cols_odd == 2, "board cols_odd");
    ASSERT(board.rows == 2, "board rows");

    pb_offset pos = {0, 0};
    pb_bubble* b = pb_board_get(&board, pos);
    ASSERT(b != NULL && b->color_id == 0, "cell 0,0");

    pos.row = 1; pos.col = 1;
    b = pb_board_get(&board, pos);
    ASSERT(b != NULL && b->color_id == 4, "cell 1,1");

    pb_level_data_free(&level);
    PASS();
}

/* ============================================================================
 * Theme Loading Tests
 * ============================================================================ */

static void test_theme_minimal(void) {
    TEST(theme_minimal);

    const char* json = "{"
        "\"name\": \"Test Theme\","
        "\"palette\": {"
            "\"colors\": ["
                "{\"name\": \"red\", \"srgb\": \"#ff0000\"},"
                "{\"name\": \"blue\", \"srgb\": \"#0000ff\"}"
            "]"
        "}"
    "}";

    pb_theme_data theme;
    pb_data_result result = {0};

    bool ok = pb_theme_load_string(json, &theme, &result);
    ASSERT(ok, result.error);
    ASSERT(strcmp(theme.name, "Test Theme") == 0, "name");
    ASSERT(theme.color_count == 2, "color_count");
    ASSERT(strcmp(theme.colors[0].name, "red") == 0, "color[0] name");
    ASSERT(theme.colors[0].srgb.r == 255, "red srgb");
    ASSERT(theme.colors[1].srgb.b == 255, "blue srgb");

    PASS();
}

static void test_theme_default(void) {
    TEST(theme_default);

    pb_theme_data theme;
    pb_theme_get_default(&theme);

    ASSERT(strcmp(theme.name, "Classic") == 0, "default name");
    ASSERT(theme.color_count == 8, "default 8 colors");
    ASSERT(theme.bubble_outline_width == 2.0f, "outline width");
    ASSERT(theme.bubble_shine == true, "shine enabled");

    PASS();
}

/* ============================================================================
 * Ruleset Loading Tests
 * ============================================================================ */

static void test_ruleset_defaults(void) {
    TEST(ruleset_defaults);

    const char* json = "{}";

    pb_ruleset ruleset;
    pb_data_result result = {0};

    bool ok = pb_ruleset_load_string(json, &ruleset, &result);
    ASSERT(ok, result.error);
    ASSERT(ruleset.mode == PB_MODE_PUZZLE, "default mode");
    ASSERT(ruleset.match_threshold == 3, "match threshold");
    ASSERT(ruleset.cols_even == 8, "cols_even");
    ASSERT(ruleset.max_bounces == 2, "max_bounces");

    PASS();
}

static void test_ruleset_custom(void) {
    TEST(ruleset_custom);

    const char* json = "{"
        "\"mode\": \"survival\","
        "\"grid\": {\"cols_even\": 10, \"rows\": 16},"
        "\"mechanics\": {\"match_threshold\": 4, \"max_bounces\": 3},"
        "\"pressure\": {\"shots_per_row_insert\": 5}"
    "}";

    pb_ruleset ruleset;
    pb_data_result result = {0};

    bool ok = pb_ruleset_load_string(json, &ruleset, &result);
    ASSERT(ok, result.error);
    ASSERT(ruleset.mode == PB_MODE_SURVIVAL, "survival mode");
    ASSERT(ruleset.cols_even == 10, "cols_even");
    ASSERT(ruleset.rows == 16, "rows");
    ASSERT(ruleset.match_threshold == 4, "match threshold");
    ASSERT(ruleset.max_bounces == 3, "max_bounces");
    ASSERT(ruleset.shots_per_row_insert == 5, "shots per row");

    PASS();
}

/* ============================================================================
 * Replay Loading Tests
 * ============================================================================ */

static void test_replay_minimal(void) {
    TEST(replay_minimal);

    const char* json = "{"
        "\"version\": \"1.0\","
        "\"seed\": 12345,"
        "\"events\": ["
            "{\"frame\": 100, \"type\": \"fire\", \"data\": {\"angle\": 1.57}},"
            "{\"frame\": 200, \"type\": \"rotate_left\"}"
        "]"
    "}";

    pb_replay_data replay;
    pb_data_result result = {0};

    bool ok = pb_replay_load_string(json, &replay, &result);
    ASSERT(ok, result.error);
    ASSERT(replay.seed == 12345, "seed");
    ASSERT(replay.event_count == 2, "event count");
    ASSERT(replay.events[0].frame == 100, "event 0 frame");
    ASSERT(replay.events[0].type == PB_EVENT_FIRE, "event 0 type");
    ASSERT(fabs(PB_FIXED_TO_FLOAT(replay.events[0].angle) - 1.57f) < 0.01f, "angle");
    ASSERT(replay.events[1].type == PB_EVENT_ROTATE_LEFT, "event 1 type");

    pb_replay_data_free(&replay);
    PASS();
}

static void test_replay_save_roundtrip(void) {
    TEST(replay_save_roundtrip);

    /* Create a replay */
    pb_replay_data replay = {0};
    strcpy(replay.version, "1.0");
    replay.seed = 42;

    replay.events = calloc(2, sizeof(pb_replay_event));
    replay.event_count = 2;
    replay.events[0].frame = 50;
    replay.events[0].type = PB_EVENT_FIRE;
    replay.events[0].angle = PB_FLOAT_TO_FIXED(1.0f);
    replay.events[1].frame = 100;
    replay.events[1].type = PB_EVENT_SWITCH_BUBBLE;

    strcpy(replay.metadata.level_name, "Test Level");
    replay.metadata.final_score = 1000;

    /* Save to string */
    char* json = pb_replay_save_string(&replay);
    ASSERT(json != NULL, "save failed");

    /* Load back */
    pb_replay_data replay2;
    pb_data_result result = {0};
    bool ok = pb_replay_load_string(json, &replay2, &result);
    free(json);
    ASSERT(ok, result.error);

    /* Verify */
    ASSERT(replay2.seed == 42, "seed roundtrip");
    ASSERT(replay2.event_count == 2, "event count roundtrip");
    ASSERT(replay2.events[0].type == PB_EVENT_FIRE, "type roundtrip");
    ASSERT(replay2.metadata.final_score == 1000, "score roundtrip");

    free(replay.events);
    pb_replay_data_free(&replay2);
    PASS();
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

static void test_error_null_param(void) {
    TEST(error_null_param);

    pb_level_data level;
    pb_data_result result = {0};

    bool ok = pb_level_load_string(NULL, &level, &result);
    ASSERT(!ok, "should fail with NULL");
    ASSERT(result.success == false, "success should be false");
    ASSERT(strlen(result.error) > 0, "error message set");

    PASS();
}

static void test_error_invalid_json(void) {
    TEST(error_invalid_json);

    pb_level_data level;
    pb_data_result result = {0};

    bool ok = pb_level_load_string("{invalid json", &level, &result);
    ASSERT(!ok, "should fail with invalid JSON");
    ASSERT(strlen(result.error) > 0, "error message set");

    PASS();
}

static void test_error_missing_required(void) {
    TEST(error_missing_required);

    pb_level_data level;
    pb_data_result result = {0};

    /* Missing "name" field */
    bool ok = pb_level_load_string("{\"grid\": {}}", &level, &result);
    ASSERT(!ok, "should fail without name");
    ASSERT(strstr(result.error, "name") != NULL, "error mentions name");

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("pb_data test suite\n");
    printf("==================\n\n");

    printf("Level loading:\n");
    test_level_minimal();
    test_level_objectives();
    test_level_special_bubbles();
    test_level_to_board();

    printf("\nTheme loading:\n");
    test_theme_minimal();
    test_theme_default();

    printf("\nRuleset loading:\n");
    test_ruleset_defaults();
    test_ruleset_custom();

    printf("\nReplay loading:\n");
    test_replay_minimal();
    test_replay_save_roundtrip();

    printf("\nError handling:\n");
    test_error_null_param();
    test_error_invalid_json();
    test_error_missing_required();

    printf("\n==================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

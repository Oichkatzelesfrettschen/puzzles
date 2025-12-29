/**
 * @file test_checksum.c
 * @brief Tests for pb_checksum CRC-32 and state checksumming
 */

#include <stdio.h>
#include <string.h>
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
 * CRC-32 Basic Tests
 * ============================================================================ */

static void test_crc32_empty(void) {
    TEST(crc32_empty);

    uint32_t crc = pb_crc32(NULL, 0);
    ASSERT(crc == 0, "empty data should give 0");

    PASS();
}

static void test_crc32_known_values(void) {
    TEST(crc32_known_values);

    /* "123456789" has well-known CRC-32 = 0xCBF43926 */
    const char* test_string = "123456789";
    uint32_t crc = pb_crc32(test_string, 9);
    ASSERT(crc == 0xCBF43926, "CRC-32 of '123456789'");

    PASS();
}

static void test_crc32_incremental(void) {
    TEST(crc32_incremental);

    const char* data = "Hello, World!";
    size_t len = strlen(data);

    /* Full computation */
    uint32_t crc_full = pb_crc32(data, len);

    /* Incremental computation */
    uint32_t crc_inc = 0;
    crc_inc = pb_crc32_update(crc_inc, data, 7);      /* "Hello, " */
    crc_inc = pb_crc32_update(crc_inc, data + 7, 6);  /* "World!" */

    ASSERT(crc_full == crc_inc, "incremental matches full");

    PASS();
}

static void test_crc32_deterministic(void) {
    TEST(crc32_deterministic);

    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    uint32_t crc1 = pb_crc32(data, sizeof(data));
    uint32_t crc2 = pb_crc32(data, sizeof(data));
    uint32_t crc3 = pb_crc32(data, sizeof(data));

    ASSERT(crc1 == crc2, "same data same CRC");
    ASSERT(crc2 == crc3, "consistent across calls");

    PASS();
}

/* ============================================================================
 * Bubble/Board Checksum Tests
 * ============================================================================ */

static void test_bubble_checksum(void) {
    TEST(bubble_checksum);

    pb_bubble b1 = {.kind = PB_KIND_COLORED, .color_id = 3, .flags = 0};
    pb_bubble b2 = {.kind = PB_KIND_COLORED, .color_id = 3, .flags = 0};
    pb_bubble b3 = {.kind = PB_KIND_COLORED, .color_id = 4, .flags = 0};

    uint32_t crc1 = pb_bubble_checksum(&b1);
    uint32_t crc2 = pb_bubble_checksum(&b2);
    uint32_t crc3 = pb_bubble_checksum(&b3);

    ASSERT(crc1 == crc2, "identical bubbles same checksum");
    ASSERT(crc1 != crc3, "different color different checksum");

    PASS();
}

static void test_board_checksum_empty(void) {
    TEST(board_checksum_empty);

    pb_board board1, board2;
    pb_board_init(&board1);
    pb_board_init(&board2);

    uint32_t crc1 = pb_board_checksum(&board1);
    uint32_t crc2 = pb_board_checksum(&board2);

    ASSERT(crc1 == crc2, "identical empty boards same checksum");
    ASSERT(crc1 != 0, "empty board has non-zero checksum");

    PASS();
}

static void test_board_checksum_with_bubbles(void) {
    TEST(board_checksum_with_bubbles);

    pb_board board1, board2;
    pb_board_init(&board1);
    pb_board_init(&board2);

    /* Same bubble in same position */
    pb_bubble b = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_offset pos = {0, 0};
    pb_board_set(&board1, pos, b);
    pb_board_set(&board2, pos, b);

    uint32_t crc1 = pb_board_checksum(&board1);
    uint32_t crc2 = pb_board_checksum(&board2);

    ASSERT(crc1 == crc2, "same bubble same position same checksum");

    /* Different position should differ */
    pb_board_init(&board2);
    pos.col = 1;
    pb_board_set(&board2, pos, b);

    crc2 = pb_board_checksum(&board2);
    ASSERT(crc1 != crc2, "same bubble different position different checksum");

    PASS();
}

static void test_board_checksum_sensitive(void) {
    TEST(board_checksum_sensitive);

    pb_board board;
    pb_board_init(&board);

    uint32_t crc_empty = pb_board_checksum(&board);

    /* Add one bubble */
    pb_bubble b = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_offset pos = {2, 3};
    pb_board_set(&board, pos, b);

    uint32_t crc_one = pb_board_checksum(&board);
    ASSERT(crc_empty != crc_one, "adding bubble changes checksum");

    /* Change that bubble's color */
    b.color_id = 1;
    pb_board_set(&board, pos, b);

    uint32_t crc_recolor = pb_board_checksum(&board);
    ASSERT(crc_one != crc_recolor, "changing color changes checksum");

    PASS();
}

/* ============================================================================
 * RNG Checksum Tests
 * ============================================================================ */

static void test_rng_checksum(void) {
    TEST(rng_checksum);

    pb_rng rng1, rng2;
    pb_rng_seed(&rng1, 12345);
    pb_rng_seed(&rng2, 12345);

    uint32_t crc1 = pb_rng_state_checksum(&rng1);
    uint32_t crc2 = pb_rng_state_checksum(&rng2);

    ASSERT(crc1 == crc2, "same seed same checksum");

    /* Advance one RNG */
    pb_rng_next(&rng1);
    uint32_t crc1_adv = pb_rng_state_checksum(&rng1);

    ASSERT(crc1 != crc1_adv, "advancing RNG changes checksum");
    ASSERT(crc2 != crc1_adv, "diverged RNG different checksum");

    PASS();
}

/* ============================================================================
 * Game State Checksum Tests
 * ============================================================================ */

static void test_state_checksum(void) {
    TEST(state_checksum);

    pb_game_state state1, state2;
    memset(&state1, 0, sizeof(state1));
    memset(&state2, 0, sizeof(state2));

    pb_board_init(&state1.board);
    pb_board_init(&state2.board);
    pb_rng_seed(&state1.rng, 42);
    pb_rng_seed(&state2.rng, 42);

    uint32_t crc1 = pb_state_checksum(&state1);
    uint32_t crc2 = pb_state_checksum(&state2);

    ASSERT(crc1 == crc2, "identical states same checksum");

    /* Change frame */
    state2.frame = 100;
    crc2 = pb_state_checksum(&state2);
    ASSERT(crc1 != crc2, "different frame different checksum");

    PASS();
}

static void test_frame_checksum(void) {
    TEST(frame_checksum);

    pb_game_state state;
    memset(&state, 0, sizeof(state));
    pb_board_init(&state.board);
    pb_rng_seed(&state.rng, 42);

    uint32_t crc1 = pb_frame_checksum(&state);

    /* Change score */
    state.score = 1000;
    uint32_t crc2 = pb_frame_checksum(&state);

    ASSERT(crc1 != crc2, "score change affects frame checksum");

    PASS();
}

/* ============================================================================
 * State Comparison Tests
 * ============================================================================ */

static void test_state_compare_equal(void) {
    TEST(state_compare_equal);

    pb_game_state state1, state2;
    memset(&state1, 0, sizeof(state1));
    memset(&state2, 0, sizeof(state2));

    pb_board_init(&state1.board);
    pb_board_init(&state2.board);
    pb_rng_seed(&state1.rng, 42);
    pb_rng_seed(&state2.rng, 42);

    pb_desync_info info;
    bool match = pb_state_compare(&state1, &state2, &info);

    ASSERT(match, "identical states should match");
    ASSERT(!info.detected, "no desync detected");

    PASS();
}

static void test_state_compare_rng_desync(void) {
    TEST(state_compare_rng_desync);

    pb_game_state state1, state2;
    memset(&state1, 0, sizeof(state1));
    memset(&state2, 0, sizeof(state2));

    pb_board_init(&state1.board);
    pb_board_init(&state2.board);
    pb_rng_seed(&state1.rng, 42);
    pb_rng_seed(&state2.rng, 42);

    /* Diverge RNG */
    pb_rng_next(&state2.rng);

    pb_desync_info info;
    bool match = pb_state_compare(&state1, &state2, &info);

    ASSERT(!match, "diverged RNG should not match");
    ASSERT(info.detected, "desync detected");
    ASSERT(strcmp(info.component, "rng") == 0, "component is rng");

    PASS();
}

static void test_state_compare_board_desync(void) {
    TEST(state_compare_board_desync);

    pb_game_state state1, state2;
    memset(&state1, 0, sizeof(state1));
    memset(&state2, 0, sizeof(state2));

    pb_board_init(&state1.board);
    pb_board_init(&state2.board);
    pb_rng_seed(&state1.rng, 42);
    pb_rng_seed(&state2.rng, 42);

    /* Diverge board */
    pb_bubble b = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_offset pos = {0, 0};
    pb_board_set(&state2.board, pos, b);

    pb_desync_info info;
    bool match = pb_state_compare(&state1, &state2, &info);

    ASSERT(!match, "diverged board should not match");
    ASSERT(info.detected, "desync detected");
    ASSERT(strcmp(info.component, "board") == 0, "component is board");

    PASS();
}

/* ============================================================================
 * Checksum Buffer Tests
 * ============================================================================ */

static void test_checksum_buffer_record(void) {
    TEST(checksum_buffer_record);

    pb_checksum_buffer buf;
    pb_checksum_buffer_init(&buf);

    pb_checksum_buffer_record(&buf, 10, 0xAAAAAAAA);
    pb_checksum_buffer_record(&buf, 20, 0xBBBBBBBB);
    pb_checksum_buffer_record(&buf, 30, 0xCCCCCCCC);

    uint32_t checksum;
    ASSERT(pb_checksum_buffer_find(&buf, 10, &checksum), "find frame 10");
    ASSERT(checksum == 0xAAAAAAAA, "frame 10 checksum");

    ASSERT(pb_checksum_buffer_find(&buf, 30, &checksum), "find frame 30");
    ASSERT(checksum == 0xCCCCCCCC, "frame 30 checksum");

    ASSERT(!pb_checksum_buffer_find(&buf, 99, &checksum), "frame 99 not found");

    PASS();
}

static void test_checksum_buffer_verify(void) {
    TEST(checksum_buffer_verify);

    pb_checksum_buffer buf;
    pb_checksum_buffer_init(&buf);

    pb_checksum_buffer_record(&buf, 100, 0x12345678);

    ASSERT(pb_checksum_buffer_verify(&buf, 100, 0x12345678), "correct checksum");
    ASSERT(!pb_checksum_buffer_verify(&buf, 100, 0x00000000), "wrong checksum");
    ASSERT(!pb_checksum_buffer_verify(&buf, 999, 0x12345678), "wrong frame");

    PASS();
}

static void test_checksum_buffer_wrap(void) {
    TEST(checksum_buffer_wrap);

    pb_checksum_buffer buf;
    pb_checksum_buffer_init(&buf);

    /* Fill buffer beyond capacity */
    for (uint32_t i = 0; i < PB_CHECKSUM_BUFFER_SIZE + 10; i++) {
        pb_checksum_buffer_record(&buf, i, i * 1000);
    }

    /* Old entries should be gone */
    ASSERT(!pb_checksum_buffer_find(&buf, 0, NULL), "frame 0 evicted");
    ASSERT(!pb_checksum_buffer_find(&buf, 5, NULL), "frame 5 evicted");

    /* Recent entries should be present */
    uint32_t checksum;
    uint32_t last_frame = PB_CHECKSUM_BUFFER_SIZE + 9;
    ASSERT(pb_checksum_buffer_find(&buf, last_frame, &checksum), "last frame found");
    ASSERT(checksum == last_frame * 1000, "last frame checksum correct");

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("pb_checksum test suite\n");
    printf("======================\n\n");

    printf("CRC-32 basic:\n");
    test_crc32_empty();
    test_crc32_known_values();
    test_crc32_incremental();
    test_crc32_deterministic();

    printf("\nBubble/Board checksums:\n");
    test_bubble_checksum();
    test_board_checksum_empty();
    test_board_checksum_with_bubbles();
    test_board_checksum_sensitive();

    printf("\nRNG checksum:\n");
    test_rng_checksum();

    printf("\nGame state checksum:\n");
    test_state_checksum();
    test_frame_checksum();

    printf("\nState comparison:\n");
    test_state_compare_equal();
    test_state_compare_rng_desync();
    test_state_compare_board_desync();

    printf("\nChecksum buffer:\n");
    test_checksum_buffer_record();
    test_checksum_buffer_verify();
    test_checksum_buffer_wrap();

    printf("\n======================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

/*
 * test_solver.c - Tests for pb_solver module
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Test Framework (minimal)
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    tests_run++; \
    printf("  " #name "... "); \
    test_##name(); \
    tests_passed++; \
    printf("OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(a) ASSERT(a)
#define ASSERT_FALSE(a) ASSERT(!(a))

/*============================================================================
 * Board Statistics Tests
 *============================================================================*/

TEST(board_analyze_empty) {
    pb_board board;
    pb_board_init(&board);

    pb_board_stats stats;
    pb_board_analyze(&board, &stats);

    ASSERT_EQ(stats.total_bubbles, 0);
    ASSERT_EQ(stats.height, 0);
    ASSERT_EQ(stats.orphan_count, 0);
    ASSERT_EQ(stats.blocker_count, 0);
    ASSERT_TRUE(stats.density < 0.01f);
}

TEST(board_analyze_with_bubbles) {
    pb_board board;
    pb_board_init(&board);

    /* Place some bubbles */
    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble blue = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};

    pb_board_set(&board, (pb_offset){0, 0}, red);
    pb_board_set(&board, (pb_offset){0, 1}, red);
    pb_board_set(&board, (pb_offset){0, 2}, blue);

    pb_board_stats stats;
    pb_board_analyze(&board, &stats);

    ASSERT_EQ(stats.total_bubbles, 3);
    ASSERT_EQ(stats.height, 1);
    ASSERT_EQ(stats.color_counts[0], 2);  /* red */
    ASSERT_EQ(stats.color_counts[1], 1);  /* blue */
    ASSERT_EQ(stats.orphan_count, 0);
    ASSERT_TRUE(stats.density > 0.0f);
}

TEST(count_orphans_none) {
    pb_board board;
    pb_board_init(&board);

    /* Place bubbles in row 0 - all attached */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);
    pb_board_set(&board, (pb_offset){0, 1}, b);

    int orphans = pb_count_orphans(&board);
    ASSERT_EQ(orphans, 0);
}

TEST(count_orphans_disconnected) {
    pb_board board;
    pb_board_init(&board);

    /* Bubble in row 0 (attached) */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);

    /* Bubble in row 3 (orphaned - no path to row 0) */
    pb_board_set(&board, (pb_offset){3, 0}, b);

    int orphans = pb_count_orphans(&board);
    ASSERT_EQ(orphans, 1);
}

/*============================================================================
 * Validation Tests
 *============================================================================*/

TEST(validate_empty_board) {
    pb_board board;
    pb_board_init(&board);

    pb_validation_info info;
    pb_validation_result result = pb_validate_level(&board, NULL, 0xFF, &info);

    ASSERT_EQ(result, PB_VALID);
}

TEST(validate_with_orphans) {
    pb_board board;
    pb_board_init(&board);

    /* Place orphaned bubble */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){3, 0}, b);  /* No connection to ceiling */

    pb_validation_info info;
    pb_validation_result result = pb_validate_level(&board, NULL, 0xFF, &info);

    ASSERT_EQ(result, PB_INVALID_ORPHANS);
    ASSERT_EQ(info.error_count, 1);
}

TEST(validate_missing_colors) {
    pb_board board;
    pb_board_init(&board);

    /* Place bubble with color 5 */
    pb_bubble b = {PB_KIND_COLORED, 5, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);

    /* Only colors 0-3 available */
    uint8_t available = 0x0F;  /* bits 0-3 */
    pb_validation_info info;
    pb_validation_result result = pb_validate_level(&board, NULL, available, &info);

    ASSERT_EQ(result, PB_INVALID_COLORS);
    ASSERT_TRUE(info.missing_colors & (1 << 5));
}

TEST(validate_all_colors_available) {
    pb_board board;
    pb_board_init(&board);

    pb_bubble b0 = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble b1 = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble b2 = {PB_KIND_COLORED, 2, 0, PB_SPECIAL_NONE, {0}};

    pb_board_set(&board, (pb_offset){0, 0}, b0);
    pb_board_set(&board, (pb_offset){0, 1}, b1);
    pb_board_set(&board, (pb_offset){0, 2}, b2);

    pb_validation_info info;
    pb_validation_result result = pb_validate_level(&board, NULL, 0xFF, &info);

    ASSERT_EQ(result, PB_VALID);
    ASSERT_EQ(info.missing_colors, 0);
}

/*============================================================================
 * Solver Tests
 *============================================================================*/

TEST(solver_init) {
    pb_board board;
    pb_board_init(&board);

    pb_solver solver;
    pb_solver_init(&solver, &board, NULL, 12345);

    ASSERT_EQ(solver.moves_made, 0);
    ASSERT_EQ(solver.queue_length, 0);
    ASSERT_FALSE(solver.deterministic);
}

TEST(solver_set_queue) {
    pb_board board;
    pb_board_init(&board);

    pb_solver solver;
    pb_solver_init(&solver, &board, NULL, 0);

    pb_bubble queue[5];
    for (int i = 0; i < 5; i++) {
        queue[i].kind = PB_KIND_COLORED;
        queue[i].color_id = (uint8_t)i;
    }

    pb_solver_set_queue(&solver, queue, 5);

    ASSERT_EQ(solver.queue_length, 5);
    ASSERT_TRUE(solver.deterministic);
    ASSERT_EQ(solver.queue[0].color_id, 0);
    ASSERT_EQ(solver.queue[4].color_id, 4);
}

TEST(solver_is_cleared_empty) {
    pb_board board;
    pb_board_init(&board);

    pb_solver solver;
    pb_solver_init(&solver, &board, NULL, 0);

    ASSERT_TRUE(pb_solver_is_cleared(&solver));
}

TEST(solver_is_cleared_with_bubbles) {
    pb_board board;
    pb_board_init(&board);

    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);

    pb_solver solver;
    pb_solver_init(&solver, &board, NULL, 0);

    ASSERT_FALSE(pb_solver_is_cleared(&solver));
}

/*============================================================================
 * Difficulty Estimation Tests
 *============================================================================*/

TEST(difficulty_trivial) {
    pb_board board;
    pb_board_init(&board);

    /* Place 3 matching bubbles - trivial to clear */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);
    pb_board_set(&board, (pb_offset){0, 1}, b);
    pb_board_set(&board, (pb_offset){0, 2}, b);

    pb_difficulty_info info;
    pb_difficulty rating = pb_estimate_difficulty(&board, NULL, &info);

    ASSERT_EQ(rating, PB_DIFFICULTY_TRIVIAL);
    ASSERT_TRUE(info.estimated_moves < 5);
}

TEST(difficulty_scales_with_bubbles) {
    pb_board board;
    pb_board_init(&board);

    /* Place many bubbles across multiple rows */
    pb_bubble colors[3] = {
        {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}},
        {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}},
        {PB_KIND_COLORED, 2, 0, PB_SPECIAL_NONE, {0}}
    };

    /* Fill 5 rows */
    for (int row = 0; row < 5; row++) {
        int cols = pb_row_cols(row, board.cols_even, board.cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            pb_board_set(&board, pos, colors[(row + col) % 3]);
        }
    }

    pb_difficulty_info info;
    pb_difficulty rating = pb_estimate_difficulty(&board, NULL, &info);

    /* More bubbles = higher estimated moves */
    ASSERT_TRUE(info.estimated_moves > 10);
    ASSERT_TRUE(rating >= PB_DIFFICULTY_EASY);
}

TEST(difficulty_info_filled) {
    pb_board board;
    pb_board_init(&board);

    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);

    pb_difficulty_info info;
    pb_estimate_difficulty(&board, NULL, &info);

    ASSERT_TRUE(info.notes[0] != '\0');
    ASSERT_TRUE(info.precision_required >= 0.0f);
    ASSERT_TRUE(info.precision_required <= 1.0f);
}

/*============================================================================
 * Solvability Tests
 *============================================================================*/

TEST(solvability_empty) {
    pb_board board;
    pb_board_init(&board);

    pb_solvability result;
    pb_analyze_solvability(&board, NULL, NULL, 0, 100, &result);

    /* Empty board is trivially "solvable" */
    ASSERT_TRUE(result.solvable);
    ASSERT_EQ(result.min_moves, 0);
}

TEST(solvability_with_queue) {
    pb_board board;
    pb_board_init(&board);

    /* Simple board */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, b);
    pb_board_set(&board, (pb_offset){0, 1}, b);

    /* Queue with matching color */
    pb_bubble queue[5];
    for (int i = 0; i < 5; i++) {
        queue[i].kind = PB_KIND_COLORED;
        queue[i].color_id = 0;
    }

    pb_solvability result;
    pb_analyze_solvability(&board, NULL, queue, 5, 100, &result);

    ASSERT_EQ(result.shots_available, 5);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_solver test suite\n");
    printf("====================\n\n");

    printf("Board analysis:\n");
    RUN(board_analyze_empty);
    RUN(board_analyze_with_bubbles);
    RUN(count_orphans_none);
    RUN(count_orphans_disconnected);

    printf("\nValidation:\n");
    RUN(validate_empty_board);
    RUN(validate_with_orphans);
    RUN(validate_missing_colors);
    RUN(validate_all_colors_available);

    printf("\nSolver:\n");
    RUN(solver_init);
    RUN(solver_set_queue);
    RUN(solver_is_cleared_empty);
    RUN(solver_is_cleared_with_bubbles);

    printf("\nDifficulty estimation:\n");
    RUN(difficulty_trivial);
    RUN(difficulty_scales_with_bubbles);
    RUN(difficulty_info_filled);

    printf("\nSolvability:\n");
    RUN(solvability_empty);
    RUN(solvability_with_queue);

    printf("\n====================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

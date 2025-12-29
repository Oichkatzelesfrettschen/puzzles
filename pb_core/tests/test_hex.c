/*
 * test_hex.c - Unit tests for hexagonal grid mathematics
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pb/pb_core.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

/*============================================================================
 * Coordinate Conversion Tests
 *============================================================================*/

TEST(offset_to_axial_row0)
{
    /* Row 0 (even): direct mapping */
    pb_offset off = {0, 3};
    pb_axial ax = pb_offset_to_axial(off);
    ASSERT_EQ(ax.q, 3);
    ASSERT_EQ(ax.r, 0);
}

TEST(offset_to_axial_row1)
{
    /* Row 1 (odd): column shifts */
    pb_offset off = {1, 3};
    pb_axial ax = pb_offset_to_axial(off);
    ASSERT_EQ(ax.q, 3);
    ASSERT_EQ(ax.r, 1);
}

TEST(offset_to_axial_roundtrip)
{
    /* Test that conversions are reversible */
    for (int row = 0; row < 10; row++) {
        for (int col = 0; col < 8; col++) {
            pb_offset orig = {row, col};
            pb_axial ax = pb_offset_to_axial(orig);
            pb_offset back = pb_axial_to_offset(ax);
            ASSERT_EQ(orig.row, back.row);
            ASSERT_EQ(orig.col, back.col);
        }
    }
}

TEST(cube_invariant)
{
    /* Test that q + r + s = 0 for all cube coordinates */
    for (int row = 0; row < 10; row++) {
        for (int col = 0; col < 8; col++) {
            pb_offset off = {row, col};
            pb_cube cb = pb_offset_to_cube(off);
            ASSERT_EQ(cb.q + cb.r + cb.s, 0);
        }
    }
}

/*============================================================================
 * Neighbor Tests
 *============================================================================*/

TEST(neighbors_count)
{
    /* Every cell has exactly 6 neighbors */
    pb_offset off = {5, 3};
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(off, neighbors);

    /* All should be different from center */
    for (int i = 0; i < 6; i++) {
        ASSERT(!pb_offset_eq(neighbors[i], off));
    }

    /* All should be different from each other */
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            ASSERT(!pb_offset_eq(neighbors[i], neighbors[j]));
        }
    }
}

TEST(neighbors_even_row)
{
    /* Even row neighbors */
    pb_offset off = {2, 3};
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(off, neighbors);

    /* E neighbor */
    ASSERT_EQ(neighbors[PB_DIR_E].row, 2);
    ASSERT_EQ(neighbors[PB_DIR_E].col, 4);

    /* W neighbor */
    ASSERT_EQ(neighbors[PB_DIR_W].row, 2);
    ASSERT_EQ(neighbors[PB_DIR_W].col, 2);
}

TEST(neighbors_odd_row)
{
    /* Odd row neighbors (shifted) */
    pb_offset off = {3, 3};
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(off, neighbors);

    /* E neighbor */
    ASSERT_EQ(neighbors[PB_DIR_E].row, 3);
    ASSERT_EQ(neighbors[PB_DIR_E].col, 4);

    /* W neighbor */
    ASSERT_EQ(neighbors[PB_DIR_W].row, 3);
    ASSERT_EQ(neighbors[PB_DIR_W].col, 2);
}

/*============================================================================
 * Distance Tests
 *============================================================================*/

TEST(distance_same_cell)
{
    pb_offset a = {3, 4};
    ASSERT_EQ(pb_hex_distance_offset(a, a), 0);
}

TEST(distance_neighbors)
{
    /* Distance to any neighbor is 1 */
    pb_offset center = {3, 4};
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(center, neighbors);

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(pb_hex_distance_offset(center, neighbors[i]), 1);
    }
}

TEST(distance_symmetry)
{
    pb_offset a = {1, 2};
    pb_offset b = {4, 5};
    ASSERT_EQ(pb_hex_distance_offset(a, b), pb_hex_distance_offset(b, a));
}

/*============================================================================
 * Board Tests
 *============================================================================*/

TEST(board_init)
{
    pb_board board;
    pb_board_init(&board);

    ASSERT_EQ(board.rows, PB_DEFAULT_ROWS);
    ASSERT_EQ(board.cols_even, PB_DEFAULT_COLS_EVEN);
    ASSERT_EQ(board.cols_odd, PB_DEFAULT_COLS_ODD);
    ASSERT(pb_board_is_clear(&board));
}

TEST(board_set_get)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble bubble = {
        .kind = PB_KIND_COLORED,
        .color_id = 2,
        .flags = 0,
        .special = PB_SPECIAL_NONE
    };

    pb_offset pos = {3, 4};
    ASSERT(pb_board_set(&board, pos, bubble));

    const pb_bubble* got = pb_board_get_const(&board, pos);
    ASSERT(got != NULL);
    ASSERT_EQ(got->kind, PB_KIND_COLORED);
    ASSERT_EQ(got->color_id, 2);
}

TEST(board_bounds)
{
    pb_board board;
    pb_board_init(&board);

    /* Valid positions */
    ASSERT(pb_board_in_bounds(&board, (pb_offset){0, 0}));
    ASSERT(pb_board_in_bounds(&board, (pb_offset){0, 7}));  /* Even row: 8 cols */
    ASSERT(pb_board_in_bounds(&board, (pb_offset){1, 6}));  /* Odd row: 7 cols */

    /* Invalid positions */
    ASSERT(!pb_board_in_bounds(&board, (pb_offset){-1, 0}));
    ASSERT(!pb_board_in_bounds(&board, (pb_offset){0, 8}));
    ASSERT(!pb_board_in_bounds(&board, (pb_offset){1, 7}));  /* Odd row only has 7 */
    ASSERT(!pb_board_in_bounds(&board, (pb_offset){12, 0}));  /* Beyond rows */
}

/*============================================================================
 * Match Detection Tests
 *============================================================================*/

TEST(match_single)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, red);

    pb_visit_result result;
    int count = pb_find_matches(&board, (pb_offset){0, 0}, &result);
    ASSERT_EQ(count, 1);
}

TEST(match_three)
{
    pb_board board;
    pb_board_init(&board);

    /* Place 3 adjacent red bubbles */
    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 3}, red);
    pb_board_set(&board, (pb_offset){0, 4}, red);
    pb_board_set(&board, (pb_offset){1, 3}, red);

    pb_visit_result result;
    int count = pb_find_matches(&board, (pb_offset){0, 3}, &result);
    ASSERT_EQ(count, 3);
    ASSERT(pb_has_match(&board, (pb_offset){0, 3}, 3));
}

TEST(match_no_cross_color)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble blue = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};

    pb_board_set(&board, (pb_offset){0, 3}, red);
    pb_board_set(&board, (pb_offset){0, 4}, blue);  /* Different color */
    pb_board_set(&board, (pb_offset){1, 3}, red);

    pb_visit_result result;
    int count = pb_find_matches(&board, (pb_offset){0, 3}, &result);
    ASSERT_EQ(count, 2);  /* Only the 2 reds */
}

/*============================================================================
 * Orphan Detection Tests
 *============================================================================*/

TEST(orphan_all_anchored)
{
    pb_board board;
    pb_board_init(&board);

    /* Place bubbles in ceiling row */
    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){0, 0}, red);
    pb_board_set(&board, (pb_offset){0, 1}, red);

    pb_visit_result orphans;
    int count = pb_find_orphans(&board, &orphans);
    ASSERT_EQ(count, 0);  /* All attached to ceiling */
}

TEST(orphan_disconnected)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};

    /* Ceiling bubble */
    pb_board_set(&board, (pb_offset){0, 0}, red);

    /* Disconnected bubble */
    pb_board_set(&board, (pb_offset){5, 5}, red);

    pb_visit_result orphans;
    int count = pb_find_orphans(&board, &orphans);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(orphans.cells[0].row, 5);
    ASSERT_EQ(orphans.cells[0].col, 5);
}

/*============================================================================
 * RNG Tests
 *============================================================================*/

TEST(rng_deterministic)
{
    pb_rng rng1, rng2;
    pb_rng_seed(&rng1, 12345);
    pb_rng_seed(&rng2, 12345);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(pb_rng_next(&rng1), pb_rng_next(&rng2));
    }
}

TEST(rng_range)
{
    pb_rng rng;
    pb_rng_seed(&rng, 42);

    for (int i = 0; i < 1000; i++) {
        uint32_t val = pb_rng_range(&rng, 10);
        ASSERT(val < 10);
    }
}

TEST(rng_color_pick)
{
    pb_rng rng;
    pb_rng_seed(&rng, 123);

    /* Only colors 1, 3, 5 allowed */
    uint8_t mask = (1 << 1) | (1 << 3) | (1 << 5);

    for (int i = 0; i < 100; i++) {
        int color = pb_rng_pick_color(&rng, mask);
        ASSERT(color == 1 || color == 3 || color == 5);
    }
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_core test suite\n");
    printf("==================\n\n");

    printf("Coordinate conversions:\n");
    RUN_TEST(offset_to_axial_row0);
    RUN_TEST(offset_to_axial_row1);
    RUN_TEST(offset_to_axial_roundtrip);
    RUN_TEST(cube_invariant);

    printf("\nNeighbor operations:\n");
    RUN_TEST(neighbors_count);
    RUN_TEST(neighbors_even_row);
    RUN_TEST(neighbors_odd_row);

    printf("\nDistance calculations:\n");
    RUN_TEST(distance_same_cell);
    RUN_TEST(distance_neighbors);
    RUN_TEST(distance_symmetry);

    printf("\nBoard operations:\n");
    RUN_TEST(board_init);
    RUN_TEST(board_set_get);
    RUN_TEST(board_bounds);

    printf("\nMatch detection:\n");
    RUN_TEST(match_single);
    RUN_TEST(match_three);
    RUN_TEST(match_no_cross_color);

    printf("\nOrphan detection:\n");
    RUN_TEST(orphan_all_anchored);
    RUN_TEST(orphan_disconnected);

    printf("\nRNG:\n");
    RUN_TEST(rng_deterministic);
    RUN_TEST(rng_range);
    RUN_TEST(rng_color_pick);

    printf("\n==================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

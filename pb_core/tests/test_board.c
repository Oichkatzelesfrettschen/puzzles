/*
 * test_board.c - Tests for pb_board module
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  " #name "..."); \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf(" OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAILED at line %d: %s\n", __LINE__, #cond); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))

/*============================================================================
 * Initialization Tests
 *============================================================================*/

TEST(board_init_default)
{
    pb_board board;
    pb_board_init(&board);

    ASSERT_EQ(board.rows, PB_DEFAULT_ROWS);
    ASSERT_EQ(board.cols_even, PB_DEFAULT_COLS_EVEN);
    ASSERT_EQ(board.cols_odd, PB_DEFAULT_COLS_ODD);

    /* All cells should be empty */
    for (int r = 0; r < board.rows; r++) {
        int cols = (r % 2 == 0) ? board.cols_even : board.cols_odd;
        for (int c = 0; c < cols; c++) {
            pb_offset pos = {r, c};
            ASSERT_TRUE(pb_board_is_empty(&board, pos));
        }
    }
}

TEST(board_init_custom)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 10, 9);

    ASSERT_EQ(board.rows, 8);
    ASSERT_EQ(board.cols_even, 10);
    ASSERT_EQ(board.cols_odd, 9);
}

TEST(board_clear)
{
    pb_board board;
    pb_board_init(&board);

    /* Add some bubbles */
    pb_offset pos1 = {0, 0};
    pb_offset pos2 = {1, 1};
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_board_set(&board, pos1, bubble);
    pb_board_set(&board, pos2, bubble);

    ASSERT_FALSE(pb_board_is_empty(&board, pos1));
    ASSERT_FALSE(pb_board_is_empty(&board, pos2));

    /* Clear the board */
    pb_board_clear(&board);

    ASSERT_TRUE(pb_board_is_empty(&board, pos1));
    ASSERT_TRUE(pb_board_is_empty(&board, pos2));
}

/*============================================================================
 * Get/Set/Remove Tests
 *============================================================================*/

TEST(board_get_set)
{
    pb_board board;
    pb_board_init(&board);

    pb_offset pos = {2, 3};
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 2};

    ASSERT_TRUE(pb_board_set(&board, pos, bubble));

    const pb_bubble* got = pb_board_get_const(&board, pos);
    ASSERT_NE(got, NULL);
    ASSERT_EQ(got->kind, PB_KIND_COLORED);
    ASSERT_EQ(got->color_id, 2);
}

TEST(board_set_out_of_bounds)
{
    pb_board board;
    pb_board_init(&board);

    pb_offset bad_pos = {-1, 0};
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 0};

    ASSERT_FALSE(pb_board_set(&board, bad_pos, bubble));

    bad_pos.row = 100;
    ASSERT_FALSE(pb_board_set(&board, bad_pos, bubble));
}

TEST(board_remove)
{
    pb_board board;
    pb_board_init(&board);

    pb_offset pos = {0, 0};
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 3};

    pb_board_set(&board, pos, bubble);
    ASSERT_FALSE(pb_board_is_empty(&board, pos));

    pb_board_remove(&board, pos);
    ASSERT_TRUE(pb_board_is_empty(&board, pos));
}

TEST(board_get_mutable)
{
    pb_board board;
    pb_board_init(&board);

    pb_offset pos = {1, 2};
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_board_set(&board, pos, bubble);

    /* Get mutable pointer and modify */
    pb_bubble* ptr = pb_board_get(&board, pos);
    ASSERT_NE(ptr, NULL);
    ptr->color_id = 5;

    /* Verify modification persisted */
    const pb_bubble* got = pb_board_get_const(&board, pos);
    ASSERT_EQ(got->color_id, 5);
}

/*============================================================================
 * Bounds Checking Tests
 *============================================================================*/

TEST(board_in_bounds)
{
    pb_board board;
    pb_board_init(&board);

    /* Valid positions */
    pb_offset valid1 = {0, 0};
    pb_offset valid2 = {0, board.cols_even - 1};
    pb_offset valid3 = {1, 0};
    pb_offset valid4 = {1, board.cols_odd - 1};

    ASSERT_TRUE(pb_board_in_bounds(&board, valid1));
    ASSERT_TRUE(pb_board_in_bounds(&board, valid2));
    ASSERT_TRUE(pb_board_in_bounds(&board, valid3));
    ASSERT_TRUE(pb_board_in_bounds(&board, valid4));

    /* Invalid positions */
    pb_offset neg_row = {-1, 0};
    pb_offset neg_col = {0, -1};
    pb_offset big_row = {board.rows, 0};
    pb_offset big_col_even = {0, board.cols_even};
    pb_offset big_col_odd = {1, board.cols_odd};

    ASSERT_FALSE(pb_board_in_bounds(&board, neg_row));
    ASSERT_FALSE(pb_board_in_bounds(&board, neg_col));
    ASSERT_FALSE(pb_board_in_bounds(&board, big_row));
    ASSERT_FALSE(pb_board_in_bounds(&board, big_col_even));
    ASSERT_FALSE(pb_board_in_bounds(&board, big_col_odd));
}

/*============================================================================
 * Match Detection Tests
 *============================================================================*/

TEST(find_matches_horizontal)
{
    pb_board board;
    pb_board_init(&board);

    /* Place 4 red bubbles in a row */
    pb_bubble red = {.kind = PB_KIND_COLORED, .color_id = 0};
    for (int c = 0; c < 4; c++) {
        pb_offset pos = {0, c};
        pb_board_set(&board, pos, red);
    }

    pb_visit_result result;
    pb_offset origin = {0, 0};
    int count = pb_find_matches(&board, origin, &result);

    ASSERT_EQ(count, 4);
}

TEST(find_matches_vertical)
{
    pb_board board;
    pb_board_init(&board);

    /* Place bubbles vertically (alternating positions due to hex grid) */
    pb_bubble blue = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_offset pos0 = {0, 0};
    pb_offset pos1 = {1, 0};
    pb_offset pos2 = {2, 0};

    pb_board_set(&board, pos0, blue);
    pb_board_set(&board, pos1, blue);
    pb_board_set(&board, pos2, blue);

    pb_visit_result result;
    int count = pb_find_matches(&board, pos0, &result);

    ASSERT_EQ(count, 3);
}

TEST(find_matches_diagonal)
{
    pb_board board;
    pb_board_init(&board);

    /* Diagonal pattern on hex grid */
    pb_bubble green = {.kind = PB_KIND_COLORED, .color_id = 2};
    pb_offset pos0 = {0, 2};
    pb_offset pos1 = {1, 2};  /* Odd row, so this is "shifted" */
    pb_offset pos2 = {2, 3};

    pb_board_set(&board, pos0, green);
    pb_board_set(&board, pos1, green);
    pb_board_set(&board, pos2, green);

    pb_visit_result result;
    int count = pb_find_matches(&board, pos0, &result);

    ASSERT_EQ(count, 3);
}

TEST(find_matches_isolated)
{
    pb_board board;
    pb_board_init(&board);

    /* Single isolated bubble */
    pb_bubble yellow = {.kind = PB_KIND_COLORED, .color_id = 3};
    pb_offset pos = {5, 5};
    pb_board_set(&board, pos, yellow);

    pb_visit_result result;
    int count = pb_find_matches(&board, pos, &result);

    ASSERT_EQ(count, 1);  /* Just the bubble itself */
}

TEST(has_match_true)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble red = {.kind = PB_KIND_COLORED, .color_id = 0};
    for (int c = 0; c < 3; c++) {
        pb_offset pos = {0, c};
        pb_board_set(&board, pos, red);
    }

    pb_offset origin = {0, 0};
    ASSERT_TRUE(pb_has_match(&board, origin, 3));
}

TEST(has_match_false)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble red = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_offset pos0 = {0, 0};
    pb_offset pos1 = {0, 1};
    pb_board_set(&board, pos0, red);
    pb_board_set(&board, pos1, red);

    /* Only 2 connected, threshold is 3 */
    ASSERT_FALSE(pb_has_match(&board, pos0, 3));
}

/*============================================================================
 * Anchor/Orphan Detection Tests
 *============================================================================*/

TEST(find_anchored_top_row)
{
    pb_board board;
    pb_board_init(&board);

    /* Place bubbles in top row - they're anchored */
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 0};
    for (int c = 0; c < 3; c++) {
        pb_offset pos = {0, c};
        pb_board_set(&board, pos, bubble);
    }

    pb_visit_result result;
    int count = pb_find_anchored(&board, &result);

    ASSERT_EQ(count, 3);
}

TEST(find_orphans_floating)
{
    pb_board board;
    pb_board_init(&board);

    /* Place bubble NOT connected to top row */
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_offset orphan_pos = {5, 3};
    pb_board_set(&board, orphan_pos, bubble);

    pb_visit_result result;
    int count = pb_find_orphans(&board, &result);

    ASSERT_EQ(count, 1);
    ASSERT_EQ(result.cells[0].row, 5);
    ASSERT_EQ(result.cells[0].col, 3);
}

TEST(find_orphans_chain_drop)
{
    pb_board board;
    pb_board_init(&board);

    /* Create a chain: top row connected, then a gap, then floating bubbles */
    pb_bubble anchor = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_bubble floating = {.kind = PB_KIND_COLORED, .color_id = 1};

    /* Anchored bubble at top */
    pb_offset top = {0, 0};
    pb_board_set(&board, top, anchor);

    /* Floating bubbles (not connected to top) */
    pb_offset float1 = {4, 2};
    pb_offset float2 = {5, 2};
    pb_board_set(&board, float1, floating);
    pb_board_set(&board, float2, floating);

    pb_visit_result result;
    int count = pb_find_orphans(&board, &result);

    ASSERT_EQ(count, 2);
}

/*============================================================================
 * Removal Tests
 *============================================================================*/

TEST(board_remove_cells)
{
    pb_board board;
    pb_board_init(&board);

    /* Place some bubbles */
    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_offset pos0 = {0, 0};
    pb_offset pos1 = {0, 1};
    pb_offset pos2 = {0, 2};
    pb_board_set(&board, pos0, bubble);
    pb_board_set(&board, pos1, bubble);
    pb_board_set(&board, pos2, bubble);

    /* Build result to remove */
    pb_visit_result to_remove;
    to_remove.count = 2;
    to_remove.cells[0] = pos0;
    to_remove.cells[1] = pos2;

    int removed = pb_board_remove_cells(&board, &to_remove);

    ASSERT_EQ(removed, 2);
    ASSERT_TRUE(pb_board_is_empty(&board, pos0));
    ASSERT_FALSE(pb_board_is_empty(&board, pos1));  /* Not removed */
    ASSERT_TRUE(pb_board_is_empty(&board, pos2));
}

/*============================================================================
 * Color Counting Tests
 *============================================================================*/

TEST(board_count_colors)
{
    pb_board board;
    pb_board_init(&board);

    /* Add various colored bubbles */
    pb_bubble red = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_bubble blue = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_bubble green = {.kind = PB_KIND_COLORED, .color_id = 2};

    pb_board_set(&board, (pb_offset){0, 0}, red);
    pb_board_set(&board, (pb_offset){0, 1}, red);
    pb_board_set(&board, (pb_offset){0, 2}, red);
    pb_board_set(&board, (pb_offset){1, 0}, blue);
    pb_board_set(&board, (pb_offset){1, 1}, green);

    int counts[PB_MAX_COLORS] = {0};
    pb_board_count_colors(&board, counts);

    ASSERT_EQ(counts[0], 3);  /* Red */
    ASSERT_EQ(counts[1], 1);  /* Blue */
    ASSERT_EQ(counts[2], 1);  /* Green */
    ASSERT_EQ(counts[3], 0);  /* None */
}

TEST(board_color_mask)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble red = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_bubble green = {.kind = PB_KIND_COLORED, .color_id = 2};
    pb_bubble purple = {.kind = PB_KIND_COLORED, .color_id = 5};

    pb_board_set(&board, (pb_offset){0, 0}, red);
    pb_board_set(&board, (pb_offset){0, 1}, green);
    pb_board_set(&board, (pb_offset){0, 2}, purple);

    uint8_t mask = pb_board_color_mask(&board);

    ASSERT_TRUE(mask & (1 << 0));   /* Red present */
    ASSERT_FALSE(mask & (1 << 1)); /* Blue not present */
    ASSERT_TRUE(mask & (1 << 2));   /* Green present */
    ASSERT_TRUE(mask & (1 << 5));   /* Purple present */
}

/*============================================================================
 * Board State Tests
 *============================================================================*/

TEST(board_is_clear_empty)
{
    pb_board board;
    pb_board_init(&board);

    ASSERT_TRUE(pb_board_is_clear(&board));
}

TEST(board_is_clear_not_empty)
{
    pb_board board;
    pb_board_init(&board);

    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_board_set(&board, (pb_offset){0, 0}, bubble);

    ASSERT_FALSE(pb_board_is_clear(&board));
}

TEST(board_checksum_deterministic)
{
    pb_board board1, board2;
    pb_board_init(&board1);
    pb_board_init(&board2);

    pb_bubble bubble = {.kind = PB_KIND_COLORED, .color_id = 3};
    pb_board_set(&board1, (pb_offset){2, 3}, bubble);
    pb_board_set(&board2, (pb_offset){2, 3}, bubble);

    ASSERT_EQ(pb_board_checksum(&board1), pb_board_checksum(&board2));
}

TEST(board_checksum_different)
{
    pb_board board1, board2;
    pb_board_init(&board1);
    pb_board_init(&board2);

    pb_bubble red = {.kind = PB_KIND_COLORED, .color_id = 0};
    pb_bubble blue = {.kind = PB_KIND_COLORED, .color_id = 1};

    pb_board_set(&board1, (pb_offset){0, 0}, red);
    pb_board_set(&board2, (pb_offset){0, 0}, blue);

    ASSERT_NE(pb_board_checksum(&board1), pb_board_checksum(&board2));
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_board tests\n");
    printf("==============\n\n");

    printf("Initialization:\n");
    RUN_TEST(board_init_default);
    RUN_TEST(board_init_custom);
    RUN_TEST(board_clear);

    printf("\nGet/Set/Remove:\n");
    RUN_TEST(board_get_set);
    RUN_TEST(board_set_out_of_bounds);
    RUN_TEST(board_remove);
    RUN_TEST(board_get_mutable);

    printf("\nBounds checking:\n");
    RUN_TEST(board_in_bounds);

    printf("\nMatch detection:\n");
    RUN_TEST(find_matches_horizontal);
    RUN_TEST(find_matches_vertical);
    RUN_TEST(find_matches_diagonal);
    RUN_TEST(find_matches_isolated);
    RUN_TEST(has_match_true);
    RUN_TEST(has_match_false);

    printf("\nAnchor/Orphan detection:\n");
    RUN_TEST(find_anchored_top_row);
    RUN_TEST(find_orphans_floating);
    RUN_TEST(find_orphans_chain_drop);

    printf("\nRemoval:\n");
    RUN_TEST(board_remove_cells);

    printf("\nColor counting:\n");
    RUN_TEST(board_count_colors);
    RUN_TEST(board_color_mask);

    printf("\nBoard state:\n");
    RUN_TEST(board_is_clear_empty);
    RUN_TEST(board_is_clear_not_empty);
    RUN_TEST(board_checksum_deterministic);
    RUN_TEST(board_checksum_different);

    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

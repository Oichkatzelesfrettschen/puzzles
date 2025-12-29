/*
 * minimal_game.c - Minimal console example of pb_core
 *
 * Demonstrates:
 * - Game initialization with custom ruleset
 * - Board setup with bubbles
 * - Game loop with aiming and firing
 * - Score tracking and win/loss detection
 *
 * Build:
 *   make examples
 *   # or manually:
 *   gcc -std=c11 -Iinclude examples/minimal_game.c -Lbuild/lib -lpb_core -lm -o minimal_game
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TICK_LIMIT 1000  /* Maximum simulation ticks */

/*
 * Print a simple ASCII representation of the board
 */
static void print_board(const pb_board* board)
{
    const char* colors = ".RGBYCPMW";  /* None, Red, Green, Blue, Yellow, Cyan, Purple, Magenta, White */

    printf("\n");
    for (int row = 0; row < 8 && row < board->rows; row++) {
        /* Odd rows are offset */
        if (row % 2 == 1) {
            printf(" ");
        }

        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            const pb_bubble* b = pb_board_get_const(board, (pb_offset){row, col});
            if (b->kind == PB_KIND_NONE) {
                printf(". ");
            } else if (b->kind == PB_KIND_COLORED) {
                int idx = (b->color_id < 8) ? b->color_id + 1 : 1;
                printf("%c ", colors[idx]);
            } else {
                printf("X ");  /* Special bubble */
            }
        }
        printf("\n");
    }
    printf("\n");
}

/*
 * Create a simple test level with some bubbles
 */
static void setup_test_level(pb_board* board)
{
    /* Row 0: R R R B B B G G */
    pb_bubble red   = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble blue  = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble green = {PB_KIND_COLORED, 2, 0, PB_SPECIAL_NONE, {0}};

    for (int col = 0; col < 3; col++) {
        pb_board_set(board, (pb_offset){0, col}, red);
    }
    for (int col = 3; col < 6; col++) {
        pb_board_set(board, (pb_offset){0, col}, blue);
    }
    for (int col = 6; col < 8; col++) {
        pb_board_set(board, (pb_offset){0, col}, green);
    }

    /* Row 1: G G R R B B G (7 bubbles, odd row) */
    pb_board_set(board, (pb_offset){1, 0}, green);
    pb_board_set(board, (pb_offset){1, 1}, green);
    pb_board_set(board, (pb_offset){1, 2}, red);
    pb_board_set(board, (pb_offset){1, 3}, red);
    pb_board_set(board, (pb_offset){1, 4}, blue);
    pb_board_set(board, (pb_offset){1, 5}, blue);
    pb_board_set(board, (pb_offset){1, 6}, green);
}

int main(void)
{
    printf("pb_core Minimal Example\n");
    printf("=======================\n");

    /* Initialize game with puzzle mode */
    pb_game_state game;
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_PUZZLE);

    /* Seed with current time for variety */
    uint32_t seed = (uint32_t)time(NULL);
    pb_result res = pb_game_init(&game, &ruleset, seed);
    if (res != PB_OK) {
        fprintf(stderr, "Failed to initialize game: %d\n", res);
        return 1;
    }

    /* Setup the test level */
    setup_test_level(&game.board);

    printf("Initial board state:\n");
    print_board(&game.board);

    printf("Current bubble color: %d\n", game.current_bubble.color_id);
    printf("Preview bubble color: %d\n", game.preview_bubble.color_id);

    /* Count initial bubbles */
    pb_board_stats stats;
    pb_board_analyze(&game.board, &stats);
    printf("Initial bubble count: %d\n", stats.total_bubbles);

    /* Game loop - simulate firing straight up */
    printf("\n--- Simulating game ---\n");

    int tick = 0;
    int shots = 0;
    pb_scalar angle = PB_FLOAT_TO_FIXED(1.5708f);  /* ~90 degrees (straight up) */

    while (!pb_game_is_over(&game) && tick < TICK_LIMIT) {
        /* If not currently shooting, aim and fire */
        if (game.shot.phase == PB_SHOT_IDLE) {
            /* Vary angle slightly for each shot */
            pb_scalar offset = PB_FLOAT_TO_FIXED(0.3f * ((shots % 5) - 2));
            pb_game_set_angle(&game, angle + offset);

            res = pb_game_fire(&game);
            if (res == PB_OK) {
                shots++;
                printf("Shot %d: angle=%.2f, color=%d\n",
                       shots,
                       PB_FIXED_TO_FLOAT(angle + offset),
                       game.current_bubble.color_id);
            }
        }

        /* Advance simulation */
        pb_game_tick(&game);

        /* Check for pop/drop events */
        for (int i = 0; i < game.event_count; i++) {
            pb_event* evt = &game.events[i];
            if (evt->type == PB_EVENT_BUBBLES_POPPED) {
                printf("  POP! %d bubbles\n", evt->data.popped.count);
            } else if (evt->type == PB_EVENT_BUBBLES_DROPPED) {
                printf("  DROP! %d orphans fell\n", evt->data.dropped.count);
            }
        }
        pb_game_clear_events(&game);

        tick++;

        /* Print board every 50 ticks for progress */
        if (tick % 200 == 0) {
            printf("\n--- Tick %d ---\n", tick);
            print_board(&game.board);
        }
    }

    /* Final state */
    printf("\n--- Final State ---\n");
    print_board(&game.board);

    pb_board_analyze(&game.board, &stats);
    printf("Final bubble count: %d\n", stats.total_bubbles);
    printf("Shots fired: %d\n", game.shots_fired);
    printf("Final score: %u\n", game.score);
    printf("Ticks simulated: %d\n", tick);

    if (pb_game_is_won(&game)) {
        printf("\n*** VICTORY! Board cleared! ***\n");
    } else if (pb_game_is_lost(&game)) {
        printf("\n*** GAME OVER ***\n");
    } else if (tick >= TICK_LIMIT) {
        printf("\n(Simulation tick limit reached)\n");
    }

    return 0;
}

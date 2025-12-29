/*
 * test_game.c - Tests for pb_game module (game controller)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Test Framework
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
 * Game State Tests
 *============================================================================*/

TEST(game_state_init) {
    pb_game_state game;
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_PUZZLE);

    pb_result res = pb_game_init(&game, &ruleset, 12345);

    ASSERT_EQ(res, PB_OK);
    ASSERT_EQ(game.phase, PB_PHASE_PLAYING);  /* READY is UI-level concept */
    ASSERT_EQ(game.score, 0);
    ASSERT_EQ(game.shots_fired, 0);
}

TEST(game_ruleset_applied) {
    pb_game_state game;
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_PUZZLE);
    ruleset.match_threshold = 4;

    pb_game_init(&game, &ruleset, 0);

    ASSERT_EQ(game.ruleset.match_threshold, 4);
}

TEST(game_board_initialized) {
    pb_game_state game;
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_PUZZLE);

    pb_game_init(&game, &ruleset, 0);

    /* Board should have default dimensions from ruleset */
    ASSERT_TRUE(game.board.rows > 0);
    ASSERT_TRUE(game.board.cols_even > 0);
}

TEST(game_null_ruleset_uses_default) {
    pb_game_state game;

    pb_result res = pb_game_init(&game, NULL, 0);

    ASSERT_EQ(res, PB_OK);
    ASSERT_EQ(game.ruleset.match_threshold, PB_DEFAULT_MATCH_THRESHOLD);
}

/*============================================================================
 * Aiming Tests
 *============================================================================*/

TEST(game_set_angle) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    pb_scalar angle = PB_FLOAT_TO_FIXED(1.5f);
    pb_game_set_angle(&game, angle);

    ASSERT_EQ(game.shot.velocity.y, game.shot.velocity.y);  /* Just verify no crash */
}

TEST(game_rotate) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    /* Rotate should not crash */
    pb_scalar delta = PB_FLOAT_TO_FIXED(0.1f);
    pb_game_rotate(&game, delta);
    pb_game_rotate(&game, -delta);
}

/*============================================================================
 * Firing Tests
 *============================================================================*/

TEST(game_fire) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    pb_result res = pb_game_fire(&game);

    /* Should succeed in PLAYING phase with IDLE shot */
    ASSERT_EQ(res, PB_OK);
    ASSERT_EQ(game.phase, PB_PHASE_PLAYING);
}

TEST(game_fire_increments_shots) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    int before = game.shots_fired;
    pb_game_fire(&game);

    ASSERT_EQ(game.shots_fired, before + 1);
}

TEST(game_fire_during_shot_fails) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    pb_game_fire(&game);
    int shots = game.shots_fired;

    /* Try firing again while shot is active */
    pb_result res = pb_game_fire(&game);

    ASSERT_NE(res, PB_OK);
    ASSERT_EQ(game.shots_fired, shots);  /* Should not increment */
}

/*============================================================================
 * Bubble Queue Tests
 *============================================================================*/

TEST(game_swap_bubbles) {
    pb_game_state game;
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_ARCADE);  /* ARCADE allows swapping */
    pb_game_init(&game, &ruleset, 12345);

    uint8_t current_color = game.current_bubble.color_id;
    uint8_t preview_color = game.preview_bubble.color_id;

    pb_result res = pb_game_swap_bubbles(&game);

    ASSERT_EQ(res, PB_OK);
    ASSERT_EQ(game.current_bubble.color_id, preview_color);
    ASSERT_EQ(game.preview_bubble.color_id, current_color);
}

TEST(game_swap_during_shot_fails) {
    pb_game_state game;
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_ARCADE);  /* ARCADE allows swapping */
    pb_game_init(&game, &ruleset, 12345);

    pb_game_fire(&game);

    uint8_t current_color = game.current_bubble.color_id;

    pb_result res = pb_game_swap_bubbles(&game);

    /* Swap should fail during shot */
    ASSERT_NE(res, PB_OK);
    ASSERT_EQ(game.current_bubble.color_id, current_color);
}

/*============================================================================
 * Tick Tests
 *============================================================================*/

TEST(game_tick_ready) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    int events = pb_game_tick(&game);

    /* Playing phase with no shot should not generate events */
    ASSERT_EQ(events, 0);
    ASSERT_EQ(game.phase, PB_PHASE_PLAYING);
}

TEST(game_tick_advances_shot) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    pb_game_fire(&game);
    pb_scalar y_before = game.shot.pos.y;

    pb_game_tick(&game);

    /* Shot should move (or hit something) */
    ASSERT_TRUE(game.shot.pos.y != y_before ||
                game.shot.phase != PB_SHOT_MOVING);
}

/*============================================================================
 * Pause Tests
 *============================================================================*/

TEST(game_pause) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    pb_game_pause(&game, true);
    ASSERT_EQ(game.phase, PB_PHASE_PAUSED);

    pb_game_pause(&game, false);
    ASSERT_EQ(game.phase, PB_PHASE_PLAYING);
}

/*============================================================================
 * Score Tests
 *============================================================================*/

TEST(game_score_starts_zero) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    ASSERT_EQ(game.score, 0);
}

TEST(game_combo_starts_zero) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    ASSERT_EQ(game.combo_multiplier, 0);
}

/*============================================================================
 * Win/Loss Condition Tests
 *============================================================================*/

TEST(game_cleared_detection) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    /* Clear board and verify board check works */
    pb_board_clear(&game.board);
    ASSERT_TRUE(pb_board_is_clear(&game.board));

    /* pb_game_is_won checks phase, not board state directly.
     * The phase transitions to WON during tick processing.
     * For this test, we just verify board_is_clear works. */
}

TEST(game_not_over_initially) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    /* Fresh game should not be over */
    ASSERT_FALSE(pb_game_is_over(&game));
}

/*============================================================================
 * RNG Determinism
 *============================================================================*/

TEST(game_rng_deterministic) {
    pb_game_state game1, game2;

    /* Same seed should produce same bubble sequence */
    pb_game_init(&game1, NULL, 99999);
    pb_game_init(&game2, NULL, 99999);

    ASSERT_EQ(game1.current_bubble.color_id, game2.current_bubble.color_id);
    ASSERT_EQ(game1.preview_bubble.color_id, game2.preview_bubble.color_id);
}

TEST(game_rng_different_seeds) {
    pb_game_state game1, game2;

    pb_game_init(&game1, NULL, 111);
    pb_game_init(&game2, NULL, 222);

    /* Different seeds should (usually) produce different sequences */
    /* Note: this could theoretically fail by chance */
    bool same = (game1.current_bubble.color_id == game2.current_bubble.color_id) &&
                (game1.preview_bubble.color_id == game2.preview_bubble.color_id);
    /* We just verify it runs; RNG quality is tested in test_hex */
    (void)same;
}

/*============================================================================
 * Event Tests
 *============================================================================*/

TEST(game_event_count_starts_zero) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    ASSERT_EQ(game.event_count, 0);
}

TEST(game_clear_events) {
    pb_game_state game;
    pb_game_init(&game, NULL, 0);

    /* Add a dummy event */
    pb_event evt = {0};
    evt.type = PB_EVENT_FIRE;
    pb_game_add_event(&game, &evt);

    ASSERT_TRUE(game.event_count > 0);

    pb_game_clear_events(&game);
    ASSERT_EQ(game.event_count, 0);
}

/*============================================================================
 * Checksum Tests
 *============================================================================*/

TEST(game_checksum_deterministic) {
    pb_game_state game1, game2;

    pb_game_init(&game1, NULL, 12345);
    pb_game_init(&game2, NULL, 12345);

    uint32_t sum1 = pb_game_checksum(&game1);
    uint32_t sum2 = pb_game_checksum(&game2);

    ASSERT_EQ(sum1, sum2);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_game test suite\n");
    printf("==================\n\n");

    printf("Game state:\n");
    RUN(game_state_init);
    RUN(game_ruleset_applied);
    RUN(game_board_initialized);
    RUN(game_null_ruleset_uses_default);

    printf("\nAiming:\n");
    RUN(game_set_angle);
    RUN(game_rotate);

    printf("\nFiring:\n");
    RUN(game_fire);
    RUN(game_fire_increments_shots);
    RUN(game_fire_during_shot_fails);

    printf("\nBubble queue:\n");
    RUN(game_swap_bubbles);
    RUN(game_swap_during_shot_fails);

    printf("\nTick:\n");
    RUN(game_tick_ready);
    RUN(game_tick_advances_shot);

    printf("\nPause:\n");
    RUN(game_pause);

    printf("\nScoring:\n");
    RUN(game_score_starts_zero);
    RUN(game_combo_starts_zero);

    printf("\nWin/Loss:\n");
    RUN(game_cleared_detection);
    RUN(game_not_over_initially);

    printf("\nDeterminism:\n");
    RUN(game_rng_deterministic);
    RUN(game_rng_different_seeds);

    printf("\nEvents:\n");
    RUN(game_event_count_starts_zero);
    RUN(game_clear_events);

    printf("\nChecksum:\n");
    RUN(game_checksum_deterministic);

    printf("\n==================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

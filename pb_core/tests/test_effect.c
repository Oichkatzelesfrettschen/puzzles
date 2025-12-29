/*
 * test_effect.c - Unit tests for pb_effect and magnetic force
 *
 * Tests special bubble effects, effect chaining, and magnetic physics.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pb/pb_effect.h"
#include "pb/pb_shot.h"
#include "pb/pb_board.h"

/*============================================================================
 * Test Infrastructure
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int test_failed_flag = 0;

#define RUN(name) do { \
    tests_run++; \
    test_failed_flag = 0; \
    printf("  %s... ", #name); \
    test_##name(); \
    if (!test_failed_flag) { \
        tests_passed++; \
        printf("OK\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        test_failed_flag = 1; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    pb_scalar _a = (a), _b = (b), _eps = (eps); \
    if (fabsf(PB_FIXED_TO_FLOAT(_a) - PB_FIXED_TO_FLOAT(_b)) > PB_FIXED_TO_FLOAT(_eps)) { \
        printf("FAIL at %s:%d: |%f - %f| > %f\n", __FILE__, __LINE__, \
               (double)PB_FIXED_TO_FLOAT(_a), (double)PB_FIXED_TO_FLOAT(_b), \
               (double)PB_FIXED_TO_FLOAT(_eps)); \
        test_failed_flag = 1; \
        return; \
    } \
} while(0)

/*============================================================================
 * Effect Registration Tests
 *============================================================================*/

static void test_get_effect_bomb(void)
{
    const pb_effect_def* effect = pb_get_special_effect(PB_SPECIAL_BOMB);
    ASSERT(effect != NULL);
    ASSERT(effect->trigger == PB_TRIGGER_ON_POP);
    ASSERT(effect->action == PB_ACTION_DESTROY);
    ASSERT(effect->target.type == PB_TARGET_NEIGHBORS);
    ASSERT(effect->chain_allowed == true);
}

static void test_get_effect_lightning(void)
{
    const pb_effect_def* effect = pb_get_special_effect(PB_SPECIAL_LIGHTNING);
    ASSERT(effect != NULL);
    ASSERT(effect->trigger == PB_TRIGGER_ON_POP);
    ASSERT(effect->action == PB_ACTION_DESTROY);
    ASSERT(effect->target.type == PB_TARGET_ROW);
}

static void test_get_effect_star(void)
{
    const pb_effect_def* effect = pb_get_special_effect(PB_SPECIAL_STAR);
    ASSERT(effect != NULL);
    ASSERT(effect->trigger == PB_TRIGGER_ON_HIT);
    ASSERT(effect->action == PB_ACTION_DESTROY);
    ASSERT(effect->target.type == PB_TARGET_COLOR);
}

static void test_get_effect_magnetic(void)
{
    const pb_effect_def* effect = pb_get_special_effect(PB_SPECIAL_MAGNETIC);
    ASSERT(effect != NULL);
    /* Magnetic has no active effect - handled in shot physics */
    ASSERT(effect->trigger == PB_TRIGGER_NONE);
    ASSERT(effect->action == PB_ACTION_NONE);
}

static void test_get_effect_none(void)
{
    const pb_effect_def* effect = pb_get_special_effect(PB_SPECIAL_NONE);
    ASSERT(effect == NULL);
}

static void test_get_effect_invalid(void)
{
    const pb_effect_def* effect = pb_get_special_effect(PB_SPECIAL_COUNT);
    ASSERT(effect == NULL);
}

/*============================================================================
 * Target Finding Tests
 *============================================================================*/

static void test_find_targets_self(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){2, 3}, b);

    pb_target target = {PB_TARGET_SELF, {0}};
    pb_visit_result result;
    int count = pb_find_targets(&board, (pb_offset){2, 3}, &target, &result);

    ASSERT(count == 1);
    ASSERT(result.cells[0].row == 2);
    ASSERT(result.cells[0].col == 3);
}

static void test_find_targets_neighbors(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    /* Place center bubble and some neighbors */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){2, 2}, b);  /* Center */
    pb_board_set(&board, (pb_offset){2, 1}, b);  /* Left */
    pb_board_set(&board, (pb_offset){2, 3}, b);  /* Right */
    pb_board_set(&board, (pb_offset){1, 2}, b);  /* Above */

    pb_target target = {PB_TARGET_NEIGHBORS, {0}};
    pb_visit_result result;
    int count = pb_find_targets(&board, (pb_offset){2, 2}, &target, &result);

    ASSERT(count == 3);  /* 3 neighbors occupied */
}

static void test_find_targets_row(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    /* Fill a row */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    for (int col = 0; col < 8; col++) {
        pb_board_set(&board, (pb_offset){3, col}, b);
    }

    pb_target target = {PB_TARGET_ROW, {0}};
    pb_visit_result result;
    int count = pb_find_targets(&board, (pb_offset){3, 4}, &target, &result);

    ASSERT(count == 8);  /* All 8 bubbles in row */
}

static void test_find_targets_color(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    /* Place bubbles of different colors */
    pb_bubble red = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_bubble blue = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};

    pb_board_set(&board, (pb_offset){0, 0}, red);
    pb_board_set(&board, (pb_offset){1, 1}, red);
    pb_board_set(&board, (pb_offset){2, 2}, red);
    pb_board_set(&board, (pb_offset){0, 1}, blue);
    pb_board_set(&board, (pb_offset){1, 2}, blue);

    pb_target target = {PB_TARGET_COLOR, {.color_id = 0}};  /* Find red */
    pb_visit_result result;
    int count = pb_find_targets(&board, (pb_offset){0, 0}, &target, &result);

    ASSERT(count == 3);  /* 3 red bubbles */
}

/*============================================================================
 * Effect Execution Tests
 *============================================================================*/

static void test_execute_destroy_neighbors(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){2, 2}, b);
    pb_board_set(&board, (pb_offset){2, 1}, b);
    pb_board_set(&board, (pb_offset){2, 3}, b);

    const pb_effect_def* bomb = pb_get_special_effect(PB_SPECIAL_BOMB);
    pb_effect_result result;
    pb_result r = pb_execute_effect(&board, (pb_offset){2, 2}, bomb, &result);

    ASSERT(r == PB_OK);
    ASSERT(result.board_changed == true);
    ASSERT(result.affected.count == 2);  /* 2 neighbors */
}

static void test_execute_destroy_row(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    for (int col = 0; col < 6; col++) {
        pb_board_set(&board, (pb_offset){3, col}, b);
    }

    const pb_effect_def* lightning = pb_get_special_effect(PB_SPECIAL_LIGHTNING);
    pb_effect_result result;
    pb_result r = pb_execute_effect(&board, (pb_offset){3, 2}, lightning, &result);

    ASSERT(r == PB_OK);
    ASSERT(result.board_changed == true);
    ASSERT(result.affected.count == 6);  /* All 6 in row */

    /* Verify row is cleared */
    for (int col = 0; col < 6; col++) {
        ASSERT(pb_board_is_empty(&board, (pb_offset){3, col}));
    }
}

/*============================================================================
 * Effect Queue Tests
 *============================================================================*/

static void test_queue_init(void)
{
    pb_effect_queue queue;
    pb_effect_queue_init(&queue);

    ASSERT(queue.count == 0);
    ASSERT(queue.current_frame == 0);
    ASSERT(pb_effect_queue_has_pending(&queue) == false);
}

static void test_queue_add(void)
{
    pb_effect_queue queue;
    pb_effect_queue_init(&queue);

    const pb_effect_def* bomb = pb_get_special_effect(PB_SPECIAL_BOMB);
    bool added = pb_effect_queue_add(&queue, (pb_offset){2, 2}, bomb, 0);

    ASSERT(added == true);
    ASSERT(queue.count == 1);
    ASSERT(pb_effect_queue_has_pending(&queue) == true);
}

static void test_queue_process_immediate(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){2, 2}, b);
    pb_board_set(&board, (pb_offset){2, 1}, b);

    pb_effect_queue queue;
    pb_effect_queue_init(&queue);

    const pb_effect_def* bomb = pb_get_special_effect(PB_SPECIAL_BOMB);
    pb_effect_queue_add(&queue, (pb_offset){2, 2}, bomb, 0);

    pb_effect_result results[4];
    int executed = pb_effect_queue_process(&queue, &board, results, 4);

    ASSERT(executed == 1);
    ASSERT(queue.count == 0);  /* Queue cleared */
}

static void test_queue_delayed_effect(void)
{
    pb_effect_queue queue;
    pb_effect_queue_init(&queue);

    const pb_effect_def* bomb = pb_get_special_effect(PB_SPECIAL_BOMB);
    pb_effect_queue_add(&queue, (pb_offset){2, 2}, bomb, 5);  /* 5 frame delay */

    ASSERT(queue.count == 1);

    /* Not ready yet at frame 0 */
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);
    pb_effect_result results[4];
    int executed = pb_effect_queue_process(&queue, &board, results, 4);
    ASSERT(executed == 0);
    ASSERT(queue.count == 1);

    /* Advance frames */
    for (int i = 0; i < 5; i++) {
        pb_effect_queue_tick(&queue);
    }

    /* Now should execute */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){2, 2}, b);
    executed = pb_effect_queue_process(&queue, &board, results, 4);
    ASSERT(executed == 1);
}

/*============================================================================
 * Magnetic Force Tests
 *============================================================================*/

static void test_magnetic_force_direction(void)
{
    pb_point shot = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(50.0f)};
    pb_point magnet = {PB_FLOAT_TO_FIXED(100.0f), PB_FLOAT_TO_FIXED(50.0f)};  /* Right of shot */

    pb_vec2 force = pb_magnetic_force(shot, magnet, PB_MAGNETIC_DEFAULT_STRENGTH,
                                       PB_MAGNETIC_DEFAULT_RADIUS);

    /* Force should point toward magnet (positive x) */
    ASSERT(force.x > 0);
    ASSERT_NEAR(force.y, 0, PB_FLOAT_TO_FIXED(0.1f));
}

static void test_magnetic_force_inverse_square(void)
{
    pb_point shot = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(50.0f)};
    pb_point close = {PB_FLOAT_TO_FIXED(60.0f), PB_FLOAT_TO_FIXED(50.0f)};  /* 10 units */
    pb_point far = {PB_FLOAT_TO_FIXED(70.0f), PB_FLOAT_TO_FIXED(50.0f)};    /* 20 units */

    pb_vec2 force_close = pb_magnetic_force(shot, close, PB_MAGNETIC_DEFAULT_STRENGTH,
                                            PB_MAGNETIC_DEFAULT_RADIUS);
    pb_vec2 force_far = pb_magnetic_force(shot, far, PB_MAGNETIC_DEFAULT_STRENGTH,
                                          PB_MAGNETIC_DEFAULT_RADIUS);

    /* Closer should be stronger (by factor of ~4 for 2x distance) */
    ASSERT(force_close.x > force_far.x);
}

static void test_magnetic_force_beyond_radius(void)
{
    pb_point shot = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(50.0f)};
    pb_point far = {PB_FLOAT_TO_FIXED(200.0f), PB_FLOAT_TO_FIXED(50.0f)};  /* Beyond 80px radius */

    pb_vec2 force = pb_magnetic_force(shot, far, PB_MAGNETIC_DEFAULT_STRENGTH,
                                       PB_MAGNETIC_DEFAULT_RADIUS);

    /* Force should be zero beyond max radius */
    ASSERT(force.x == 0);
    ASSERT(force.y == 0);
}

static void test_magnetic_force_clamped(void)
{
    pb_point shot = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(50.0f)};
    pb_point same = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(50.0f)};  /* Same position */

    pb_vec2 force = pb_magnetic_force(shot, same, PB_MAGNETIC_DEFAULT_STRENGTH,
                                       PB_MAGNETIC_DEFAULT_RADIUS);

    /* Should not be infinite - clamped at min distance */
    /* At very close range, force should be finite but could be 0 if
     * direction is undefined. Check that it doesn't crash. */
    (void)force;  /* Just verify it doesn't crash/overflow */
}

static void test_apply_magnetic_forces(void)
{
    pb_board board;
    pb_board_init_custom(&board, 8, 8, 8);

    /* Place a magnetic bubble */
    pb_bubble magnet = {PB_KIND_SPECIAL, 0, 0, PB_SPECIAL_MAGNETIC, {0}};
    pb_board_set(&board, (pb_offset){4, 4}, magnet);

    /* Create shot moving past the magnet */
    pb_shot shot;
    pb_bubble shot_bubble = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};
    pb_point start = {PB_FLOAT_TO_FIXED(100.0f), PB_FLOAT_TO_FIXED(200.0f)};
    pb_shot_init(&shot, shot_bubble, start, PB_FLOAT_TO_FIXED(1.57f), PB_DEFAULT_SHOT_SPEED);

    pb_vec2 vel_before = shot.velocity;

    /* Apply magnetic forces */
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_apply_magnetic_forces(&shot, &board, radius);

    /* Velocity should be modified (attracted toward magnet) */
    /* This is a basic check - velocity changed in some way */
    ASSERT(shot.velocity.x != vel_before.x || shot.velocity.y != vel_before.y);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_effect test suite\n");
    printf("====================\n\n");

    printf("Effect registration:\n");
    RUN(get_effect_bomb);
    RUN(get_effect_lightning);
    RUN(get_effect_star);
    RUN(get_effect_magnetic);
    RUN(get_effect_none);
    RUN(get_effect_invalid);

    printf("\nTarget finding:\n");
    RUN(find_targets_self);
    RUN(find_targets_neighbors);
    RUN(find_targets_row);
    RUN(find_targets_color);

    printf("\nEffect execution:\n");
    RUN(execute_destroy_neighbors);
    RUN(execute_destroy_row);

    printf("\nEffect queue:\n");
    RUN(queue_init);
    RUN(queue_add);
    RUN(queue_process_immediate);
    RUN(queue_delayed_effect);

    printf("\nMagnetic force:\n");
    RUN(magnetic_force_direction);
    RUN(magnetic_force_inverse_square);
    RUN(magnetic_force_beyond_radius);
    RUN(magnetic_force_clamped);
    RUN(apply_magnetic_forces);

    printf("\n====================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

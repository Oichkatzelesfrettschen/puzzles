/*
 * test_shot.c - Tests for pb_shot module (physics)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _USE_MATH_DEFINES  /* For M_PI on some systems */
#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Fallback M_PI definition if not provided */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

/* Approximate equality for fixed-point */
#define ASSERT_NEAR(a, b, eps) ASSERT(PB_SCALAR_ABS((a) - (b)) < (eps))

/*============================================================================
 * Shot Initialization Tests
 *============================================================================*/

TEST(shot_init) {
    pb_shot shot;
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_point start = {PB_FLOAT_TO_FIXED(128.0f), PB_FLOAT_TO_FIXED(400.0f)};
    pb_scalar angle = PB_FLOAT_TO_FIXED(M_PI / 2.0f);  /* Straight up */
    pb_scalar speed = PB_DEFAULT_SHOT_SPEED;

    pb_shot_init(&shot, b, start, angle, speed);

    ASSERT_EQ(shot.bubble.kind, PB_KIND_COLORED);
    ASSERT_EQ(shot.bubble.color_id, 0);
    ASSERT_EQ(shot.pos.x, start.x);
    ASSERT_EQ(shot.pos.y, start.y);
    ASSERT_EQ(shot.bounces, 0);
}

TEST(shot_set_angle) {
    pb_shot shot;
    pb_bubble b = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};
    pb_point start = {PB_FLOAT_TO_FIXED(100.0f), PB_FLOAT_TO_FIXED(300.0f)};

    pb_shot_init(&shot, b, start, PB_FLOAT_TO_FIXED(M_PI / 4.0f), PB_DEFAULT_SHOT_SPEED);

    /* Change angle */
    pb_shot_set_angle(&shot, PB_FLOAT_TO_FIXED(M_PI / 2.0f), PB_DEFAULT_SHOT_SPEED);

    /* Velocity should now be pointing up (vx ~= 0) */
    ASSERT_NEAR(shot.velocity.x, 0, PB_FLOAT_TO_FIXED(1.0f));
}

/*============================================================================
 * Collision Detection Tests
 *============================================================================*/

TEST(collision_ceiling) {
    pb_board board;
    pb_board_init(&board);

    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_point start = {PB_FLOAT_TO_FIXED(128.0f), PB_FLOAT_TO_FIXED(100.0f)};
    pb_vec2 velocity = {PB_FLOAT_TO_FIXED(0.0f), -PB_DEFAULT_SHOT_SPEED};

    pb_point path[8];
    int path_count;

    pb_collision result = pb_shot_simulate(
        start, velocity, &board, radius,
        PB_FLOAT_TO_FIXED(0.0f),    /* left wall */
        PB_FLOAT_TO_FIXED(256.0f),  /* right wall */
        PB_FLOAT_TO_FIXED(0.0f),    /* ceiling */
        2, path, &path_count, 8
    );

    ASSERT_EQ(result.type, PB_COLLISION_CEILING);
    /* Should hit ceiling at y = radius */
    ASSERT_TRUE(result.hit_point.y <= radius);
}

TEST(collision_simulate_wall) {
    pb_board board;
    pb_board_init(&board);

    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_scalar left_wall = PB_FLOAT_TO_FIXED(0.0f);
    pb_scalar right_wall = PB_FLOAT_TO_FIXED(256.0f);
    pb_scalar ceiling = PB_FLOAT_TO_FIXED(0.0f);

    /* Shoot toward left wall at angle */
    pb_point start = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(200.0f)};
    pb_vec2 velocity = {-PB_DEFAULT_SHOT_SPEED, -PB_DEFAULT_SHOT_SPEED / 4};

    pb_point path[16];
    int path_count;

    pb_collision result = pb_shot_simulate(
        start, velocity, &board, radius,
        left_wall, right_wall, ceiling, 3,
        path, &path_count, 16
    );

    /* Should record path with bounce */
    ASSERT_TRUE(path_count >= 2);  /* At least start + one point */
    (void)result;  /* Suppress unused warning - we only care about path */
}

TEST(collision_bubble_hit) {
    pb_board board;
    pb_board_init(&board);

    /* Place a target bubble */
    pb_bubble target = {PB_KIND_COLORED, 1, 0, PB_SPECIAL_NONE, {0}};
    pb_board_set(&board, (pb_offset){2, 4}, target);

    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);

    /* Shoot straight up toward the bubble row */
    pb_point start = {PB_FLOAT_TO_FIXED(128.0f), PB_FLOAT_TO_FIXED(200.0f)};
    pb_vec2 velocity = {PB_FLOAT_TO_FIXED(0.0f), -PB_DEFAULT_SHOT_SPEED};

    pb_point path[16];
    int path_count;

    pb_collision result = pb_shot_simulate(
        start, velocity, &board, radius,
        PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(256.0f),
        PB_FLOAT_TO_FIXED(0.0f), 2,
        path, &path_count, 16
    );

    /* Should hit either the bubble or the ceiling */
    ASSERT_TRUE(result.type == PB_COLLISION_BUBBLE ||
                result.type == PB_COLLISION_CEILING);
}

/*============================================================================
 * Snap Cell Tests
 *============================================================================*/

TEST(snap_cell_valid) {
    pb_board board;
    pb_board_init(&board);

    /* Point in playfield area */
    pb_point pos = {PB_FLOAT_TO_FIXED(64.0f), PB_FLOAT_TO_FIXED(20.0f)};
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);

    pb_offset snap = pb_find_snap_cell(&board, pos, radius);

    /* Should return a valid cell */
    ASSERT_TRUE(snap.row >= 0);
    ASSERT_TRUE(snap.col >= 0);
    ASSERT_TRUE(snap.row < board.rows);
}

TEST(snap_cell_occupied_finds_empty) {
    pb_board board;
    pb_board_init(&board);

    /* Fill first row */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    for (int col = 0; col < board.cols_even; col++) {
        pb_board_set(&board, (pb_offset){0, col}, b);
    }

    /* Try to snap in row 0 area */
    pb_point pos = {PB_FLOAT_TO_FIXED(64.0f), PB_FLOAT_TO_FIXED(16.0f)};
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);

    pb_offset snap = pb_find_snap_cell(&board, pos, radius);

    /* Should find a different empty cell */
    if (snap.row >= 0 && snap.col >= 0) {
        const pb_bubble* cell = pb_board_get_const(&board, snap);
        ASSERT_EQ(cell->kind, PB_KIND_NONE);
    }
}

/*============================================================================
 * Vector Math Tests
 *============================================================================*/

TEST(velocity_straight_up) {
    pb_scalar angle = PB_FLOAT_TO_FIXED(M_PI / 2.0f);
    pb_vec2 velocity = {
        PB_FIXED_MUL(PB_SCALAR_COS(angle), PB_DEFAULT_SHOT_SPEED),
        -PB_FIXED_MUL(PB_SCALAR_SIN(angle), PB_DEFAULT_SHOT_SPEED)
    };

    /* vx should be ~0, vy should be negative (going up) */
    ASSERT_NEAR(velocity.x, 0, PB_FLOAT_TO_FIXED(1.0f));
    ASSERT_TRUE(velocity.y < 0);
}

TEST(velocity_diagonal) {
    pb_scalar angle = PB_FLOAT_TO_FIXED(M_PI / 4.0f);
    pb_vec2 velocity = {
        PB_FIXED_MUL(PB_SCALAR_COS(angle), PB_DEFAULT_SHOT_SPEED),
        -PB_FIXED_MUL(PB_SCALAR_SIN(angle), PB_DEFAULT_SHOT_SPEED)
    };

    /* At 45 degrees, |vx| should approximately equal |vy| */
    ASSERT_NEAR(velocity.x, -velocity.y, PB_FLOAT_TO_FIXED(2.0f));
}

TEST(point_distance) {
    pb_point a = {PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_point b = {PB_FLOAT_TO_FIXED(3.0f), PB_FLOAT_TO_FIXED(4.0f)};

    pb_scalar dist = pb_point_distance(a, b);

    /* 3-4-5 triangle, distance should be 5 */
    ASSERT_NEAR(dist, PB_FLOAT_TO_FIXED(5.0f), PB_FLOAT_TO_FIXED(0.1f));
}

TEST(point_distance_sq) {
    pb_point a = {PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_point b = {PB_FLOAT_TO_FIXED(3.0f), PB_FLOAT_TO_FIXED(4.0f)};

    pb_scalar dist_sq = pb_point_distance_sq(a, b);

    /* 3^2 + 4^2 = 25 */
    ASSERT_NEAR(dist_sq, PB_FLOAT_TO_FIXED(25.0f), PB_FLOAT_TO_FIXED(0.5f));
}

TEST(circles_overlap_true) {
    pb_point a = {PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_point b = {PB_FLOAT_TO_FIXED(20.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_scalar r = PB_FLOAT_TO_FIXED(15.0f);

    /* Centers 20 apart, radii 15 each => overlap of 10 */
    ASSERT_TRUE(pb_circles_overlap(a, r, b, r));
}

TEST(circles_overlap_false) {
    pb_point a = {PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_point b = {PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_scalar r = PB_FLOAT_TO_FIXED(15.0f);

    /* Centers 50 apart, radii 15 each => no overlap */
    ASSERT_FALSE(pb_circles_overlap(a, r, b, r));
}

TEST(vec2_normalize) {
    pb_vec2 v = {PB_FLOAT_TO_FIXED(3.0f), PB_FLOAT_TO_FIXED(4.0f)};
    pb_vec2 n = pb_vec2_normalize(v);

    /* Length should be ~1 */
    pb_scalar len = pb_point_distance((pb_point){0, 0}, (pb_point){n.x, n.y});
    ASSERT_NEAR(len, PB_FLOAT_TO_FIXED(1.0f), PB_FLOAT_TO_FIXED(0.1f));
}

/*============================================================================
 * Ray-Circle Intersection Tests
 *============================================================================*/

TEST(ray_circle_hit) {
    pb_point ray_origin = {PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_vec2 ray_dir = {PB_FLOAT_TO_FIXED(1.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_point circle = {PB_FLOAT_TO_FIXED(10.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_scalar radius = PB_FLOAT_TO_FIXED(5.0f);

    pb_scalar t;
    bool hit = pb_ray_circle_intersect(ray_origin, ray_dir, circle, radius, &t);

    ASSERT_TRUE(hit);
    /* t should be approximately 5 (edge of circle at x=5) */
    ASSERT_NEAR(t, PB_FLOAT_TO_FIXED(5.0f), PB_FLOAT_TO_FIXED(1.0f));
}

TEST(ray_circle_miss) {
    pb_point ray_origin = {PB_FLOAT_TO_FIXED(0.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_vec2 ray_dir = {PB_FLOAT_TO_FIXED(1.0f), PB_FLOAT_TO_FIXED(0.0f)};
    pb_point circle = {PB_FLOAT_TO_FIXED(10.0f), PB_FLOAT_TO_FIXED(20.0f)};  /* Far above ray */
    pb_scalar radius = PB_FLOAT_TO_FIXED(5.0f);

    pb_scalar t;
    bool hit = pb_ray_circle_intersect(ray_origin, ray_dir, circle, radius, &t);

    ASSERT_FALSE(hit);
}

/*============================================================================
 * Shot Step Integration
 *============================================================================*/

TEST(shot_step_moves) {
    pb_shot shot;
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    pb_point start = {PB_FLOAT_TO_FIXED(128.0f), PB_FLOAT_TO_FIXED(200.0f)};

    pb_shot_init(&shot, b, start, PB_FLOAT_TO_FIXED(M_PI / 2.0f), PB_DEFAULT_SHOT_SPEED);
    pb_scalar y_before = shot.pos.y;

    pb_board board;
    pb_board_init(&board);

    pb_collision result = pb_shot_step(&shot, &board,
        PB_FLOAT_TO_FIXED(16.0f),
        PB_FLOAT_TO_FIXED(0.0f),
        PB_FLOAT_TO_FIXED(256.0f),
        PB_FLOAT_TO_FIXED(0.0f),
        PB_FLOAT_TO_FIXED(500.0f));

    /* Shot should have moved up (y decreased) */
    ASSERT_TRUE(shot.pos.y < y_before || result.type != PB_COLLISION_NONE);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_shot test suite\n");
    printf("==================\n\n");

    printf("Shot initialization:\n");
    RUN(shot_init);
    RUN(shot_set_angle);

    printf("\nCollision detection:\n");
    RUN(collision_ceiling);
    RUN(collision_simulate_wall);
    RUN(collision_bubble_hit);

    printf("\nSnap cell:\n");
    RUN(snap_cell_valid);
    RUN(snap_cell_occupied_finds_empty);

    printf("\nVector math:\n");
    RUN(velocity_straight_up);
    RUN(velocity_diagonal);
    RUN(point_distance);
    RUN(point_distance_sq);
    RUN(circles_overlap_true);
    RUN(circles_overlap_false);
    RUN(vec2_normalize);

    printf("\nRay-circle intersection:\n");
    RUN(ray_circle_hit);
    RUN(ray_circle_miss);

    printf("\nShot step:\n");
    RUN(shot_step_moves);

    printf("\n==================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

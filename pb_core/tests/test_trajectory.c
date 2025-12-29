/*
 * test_trajectory.c - Trajectory prediction unit tests
 *
 * Tests for:
 * - Ray-circle quadratic intersection
 * - Wall reflection
 * - Trajectory segment chaining
 * - Landing cell calculation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;
static int test_failed_flag = 0;

#define TEST(name) static void test_##name(void)
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
    pb_scalar diff = (a) - (b); \
    if (diff < 0) diff = -diff; \
    if (diff > (eps)) { \
        printf("FAIL at %s:%d: |%f - %f| = %f > %f\n", \
               __FILE__, __LINE__, \
               (double)PB_FIXED_TO_FLOAT(a), \
               (double)PB_FIXED_TO_FLOAT(b), \
               (double)PB_FIXED_TO_FLOAT(diff), \
               (double)PB_FIXED_TO_FLOAT(eps)); \
        test_failed_flag = 1; \
        return; \
    } \
} while(0)

/*============================================================================
 * Ray-Circle Intersection Tests
 *============================================================================*/

TEST(ray_circle_direct_hit)
{
    /* Ray from (0,0) going right (+x) toward circle at (10,0) */
    pb_point origin = {0, 0};
    pb_vec2 dir = {PB_FLOAT_TO_FIXED(1.0f), 0};
    pb_point center = {PB_FLOAT_TO_FIXED(10.0f), 0};
    pb_scalar radius = PB_FLOAT_TO_FIXED(2.0f);
    pb_scalar t;

    bool hit = pb_trajectory_ray_circle(origin, dir, center, radius, &t);
    ASSERT(hit);
    /* Should hit at t=8 (10 - 2 = 8) */
    ASSERT_NEAR(t, PB_FLOAT_TO_FIXED(8.0f), PB_FLOAT_TO_FIXED(0.1f));
}

TEST(ray_circle_miss)
{
    /* Ray from (0,0) going up (+y) toward circle at (10,0) - should miss */
    pb_point origin = {0, 0};
    pb_vec2 dir = {0, PB_FLOAT_TO_FIXED(-1.0f)};
    pb_point center = {PB_FLOAT_TO_FIXED(10.0f), 0};
    pb_scalar radius = PB_FLOAT_TO_FIXED(2.0f);
    pb_scalar t;

    bool hit = pb_trajectory_ray_circle(origin, dir, center, radius, &t);
    ASSERT(!hit);
}

TEST(ray_circle_behind_origin)
{
    /* Ray from (10,0) going right, circle at (0,0) - behind ray origin */
    pb_point origin = {PB_FLOAT_TO_FIXED(10.0f), 0};
    pb_vec2 dir = {PB_FLOAT_TO_FIXED(1.0f), 0};
    pb_point center = {0, 0};
    pb_scalar radius = PB_FLOAT_TO_FIXED(2.0f);
    pb_scalar t;

    bool hit = pb_trajectory_ray_circle(origin, dir, center, radius, &t);
    ASSERT(!hit);
}

TEST(ray_circle_tangent)
{
    /* Ray grazes the circle edge */
    pb_point origin = {0, PB_FLOAT_TO_FIXED(2.0f)};
    pb_vec2 dir = {PB_FLOAT_TO_FIXED(1.0f), 0};
    pb_point center = {PB_FLOAT_TO_FIXED(5.0f), 0};
    pb_scalar radius = PB_FLOAT_TO_FIXED(2.0f);
    pb_scalar t;

    bool hit = pb_trajectory_ray_circle(origin, dir, center, radius, &t);
    /* Should just barely hit (tangent point) */
    ASSERT(hit);
    ASSERT_NEAR(t, PB_FLOAT_TO_FIXED(5.0f), PB_FLOAT_TO_FIXED(0.5f));
}

TEST(ray_circle_inside_circle)
{
    /* Ray origin is inside the circle */
    pb_point origin = {PB_FLOAT_TO_FIXED(5.0f), 0};
    pb_vec2 dir = {PB_FLOAT_TO_FIXED(1.0f), 0};
    pb_point center = {PB_FLOAT_TO_FIXED(5.0f), 0};
    pb_scalar radius = PB_FLOAT_TO_FIXED(3.0f);
    pb_scalar t;

    bool hit = pb_trajectory_ray_circle(origin, dir, center, radius, &t);
    /* Should hit the exit point */
    ASSERT(hit);
    ASSERT_NEAR(t, PB_FLOAT_TO_FIXED(3.0f), PB_FLOAT_TO_FIXED(0.1f));
}

/*============================================================================
 * Wall Reflection Tests
 *============================================================================*/

TEST(reflect_wall_right_to_left)
{
    pb_vec2 v = {PB_FLOAT_TO_FIXED(1.0f), PB_FLOAT_TO_FIXED(-1.0f)};
    pb_vec2 r = pb_trajectory_reflect_wall(v);
    ASSERT_NEAR(r.x, PB_FLOAT_TO_FIXED(-1.0f), PB_FLOAT_TO_FIXED(0.001f));
    ASSERT_NEAR(r.y, PB_FLOAT_TO_FIXED(-1.0f), PB_FLOAT_TO_FIXED(0.001f));
}

TEST(reflect_wall_left_to_right)
{
    pb_vec2 v = {PB_FLOAT_TO_FIXED(-1.0f), PB_FLOAT_TO_FIXED(-0.5f)};
    pb_vec2 r = pb_trajectory_reflect_wall(v);
    ASSERT_NEAR(r.x, PB_FLOAT_TO_FIXED(1.0f), PB_FLOAT_TO_FIXED(0.001f));
    ASSERT_NEAR(r.y, PB_FLOAT_TO_FIXED(-0.5f), PB_FLOAT_TO_FIXED(0.001f));
}

TEST(reflect_wall_straight_up)
{
    pb_vec2 v = {0, PB_FLOAT_TO_FIXED(-1.0f)};
    pb_vec2 r = pb_trajectory_reflect_wall(v);
    ASSERT_NEAR(r.x, 0, PB_FLOAT_TO_FIXED(0.001f));
    ASSERT_NEAR(r.y, PB_FLOAT_TO_FIXED(-1.0f), PB_FLOAT_TO_FIXED(0.001f));
}

/*============================================================================
 * Trajectory Computation Tests
 *============================================================================*/

TEST(trajectory_straight_up_ceiling)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    pb_trajectory traj;
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_point origin = {PB_FLOAT_TO_FIXED(128.0f), PB_FLOAT_TO_FIXED(400.0f)};
    pb_scalar angle = PB_FLOAT_TO_FIXED(1.5708f); /* PI/2 = straight up */

    int segments = pb_trajectory_compute(
        origin, angle, PB_FLOAT_TO_FIXED(8.0f),
        &board, radius,
        0, PB_FLOAT_TO_FIXED(256.0f),  /* walls */
        0,  /* ceiling */
        4,  /* max bounces */
        &traj
    );

    ASSERT(segments > 0);
    ASSERT(traj.is_valid);
    ASSERT(traj.segments[traj.segment_count - 1].type == PB_TRAJ_ENDPOINT_CEILING);
}

TEST(trajectory_wall_bounce)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    pb_trajectory traj;
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_point origin = {PB_FLOAT_TO_FIXED(128.0f), PB_FLOAT_TO_FIXED(400.0f)};
    pb_scalar angle = PB_FLOAT_TO_FIXED(0.785f); /* 45 degrees toward right */

    int segments = pb_trajectory_compute(
        origin, angle, PB_FLOAT_TO_FIXED(8.0f),
        &board, radius,
        0, PB_FLOAT_TO_FIXED(256.0f),
        0,
        4,
        &traj
    );

    ASSERT(segments >= 2);
    ASSERT(traj.is_valid);
    /* Should have at least one wall hit */
    bool has_wall = false;
    for (int i = 0; i < traj.segment_count - 1; i++) {
        if (traj.segments[i].type == PB_TRAJ_ENDPOINT_WALL) {
            has_wall = true;
            break;
        }
    }
    ASSERT(has_wall);
}

TEST(trajectory_bubble_collision)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    /* Place a bubble in the path */
    pb_offset cell = {5, 4};
    pb_bubble b = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_board_set(&board, cell, b);

    pb_trajectory traj;
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    /* Calculate origin below the bubble */
    pb_point bubble_center = pb_offset_to_pixel(cell, radius);
    pb_point origin = {bubble_center.x, PB_FLOAT_TO_FIXED(400.0f)};
    pb_scalar angle = PB_FLOAT_TO_FIXED(1.5708f); /* straight up */

    int segments = pb_trajectory_compute(
        origin, angle, PB_FLOAT_TO_FIXED(8.0f),
        &board, radius,
        0, PB_FLOAT_TO_FIXED(256.0f),
        0,
        4,
        &traj
    );

    ASSERT(segments > 0);
    ASSERT(traj.is_valid);
    ASSERT(traj.segments[traj.segment_count - 1].type == PB_TRAJ_ENDPOINT_BUBBLE);
}

TEST(trajectory_null_inputs)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);
    pb_trajectory traj;
    pb_point origin = {0, 0};

    int segments = pb_trajectory_compute(
        origin, 0, 0, &board, 0, 0, 0, 0, 0, NULL
    );
    ASSERT(segments == 0);

    segments = pb_trajectory_compute(
        origin, 0, 0, NULL, 0, 0, 0, 0, 0, &traj
    );
    ASSERT(segments == 0);
}

TEST(trajectory_max_bounces_limit)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    pb_trajectory traj;
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_point origin = {PB_FLOAT_TO_FIXED(16.0f), PB_FLOAT_TO_FIXED(400.0f)};
    /* Shallow angle will cause many bounces */
    pb_scalar angle = PB_FLOAT_TO_FIXED(0.5f); /* ~28 degrees */

    int segments = pb_trajectory_compute(
        origin, angle, PB_FLOAT_TO_FIXED(8.0f),
        &board, radius,
        0, PB_FLOAT_TO_FIXED(256.0f),
        0,
        2,  /* only 2 bounces allowed */
        &traj
    );

    ASSERT(segments > 0);
    ASSERT(traj.is_valid);
    /* Should terminate when bounce limit is exceeded.
     * The terminating wall segment is recorded (max_bounces + 1 max). */
    int wall_count = 0;
    for (int i = 0; i < traj.segment_count; i++) {
        if (traj.segments[i].type == PB_TRAJ_ENDPOINT_WALL) {
            wall_count++;
        }
    }
    /* With max_bounces=2, we can have up to 3 wall segments: 2 allowed + 1 terminating */
    ASSERT(wall_count <= 3);
}

/*============================================================================
 * Landing Cell Tests
 *============================================================================*/

TEST(landing_cell_empty_board)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    pb_trajectory traj;
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_point origin = {PB_FLOAT_TO_FIXED(64.0f), PB_FLOAT_TO_FIXED(400.0f)};
    pb_scalar angle = PB_FLOAT_TO_FIXED(1.5708f);

    pb_trajectory_compute(
        origin, angle, PB_FLOAT_TO_FIXED(8.0f),
        &board, radius,
        0, PB_FLOAT_TO_FIXED(256.0f),
        0, 4, &traj
    );

    pb_offset landing = pb_trajectory_get_landing(&traj, &board, radius);
    /* Should find a valid landing cell in row 0 */
    ASSERT(landing.row >= 0);
    ASSERT(landing.col >= 0);
}

TEST(landing_cell_near_bubble)
{
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    /* Place a bubble */
    pb_offset cell = {2, 3};
    pb_bubble b = {.kind = PB_KIND_COLORED, .color_id = 1};
    pb_board_set(&board, cell, b);

    pb_trajectory traj;
    pb_scalar radius = PB_FLOAT_TO_FIXED(16.0f);
    pb_point bubble_center = pb_offset_to_pixel(cell, radius);
    pb_point origin = {bubble_center.x + PB_FLOAT_TO_FIXED(8.0f),
                       PB_FLOAT_TO_FIXED(400.0f)};
    pb_scalar angle = PB_FLOAT_TO_FIXED(1.5708f);

    pb_trajectory_compute(
        origin, angle, PB_FLOAT_TO_FIXED(8.0f),
        &board, radius,
        0, PB_FLOAT_TO_FIXED(256.0f),
        0, 4, &traj
    );

    pb_offset landing = pb_trajectory_get_landing(&traj, &board, radius);
    /* Should land adjacent to the bubble, not on it */
    ASSERT(landing.row >= 0);
    ASSERT(!(landing.row == cell.row && landing.col == cell.col));
}

TEST(landing_cell_invalid_trajectory)
{
    pb_trajectory traj = {0};
    pb_board board;
    pb_board_init_custom(&board, 12, 8, 7);

    pb_offset landing = pb_trajectory_get_landing(&traj, &board,
                                                   PB_FLOAT_TO_FIXED(16.0f));
    ASSERT(landing.row == -1);
    ASSERT(landing.col == -1);
}

/*============================================================================
 * Utility Function Tests
 *============================================================================*/

TEST(trajectory_is_valid)
{
    pb_trajectory traj = {0};
    ASSERT(!pb_trajectory_is_valid(&traj));
    ASSERT(!pb_trajectory_is_valid(NULL));

    traj.is_valid = true;
    traj.segment_count = 1;
    ASSERT(pb_trajectory_is_valid(&traj));
}

TEST(trajectory_end_point)
{
    pb_trajectory traj = {0};
    pb_point end = pb_trajectory_end_point(&traj);
    ASSERT(end.x == 0 && end.y == 0);

    traj.is_valid = true;
    traj.segment_count = 1;
    traj.segments[0].end.x = PB_FLOAT_TO_FIXED(100.0f);
    traj.segments[0].end.y = PB_FLOAT_TO_FIXED(50.0f);

    end = pb_trajectory_end_point(&traj);
    ASSERT_NEAR(end.x, PB_FLOAT_TO_FIXED(100.0f), PB_FLOAT_TO_FIXED(0.001f));
    ASSERT_NEAR(end.y, PB_FLOAT_TO_FIXED(50.0f), PB_FLOAT_TO_FIXED(0.001f));
}

TEST(trajectory_total_distance)
{
    pb_trajectory traj = {0};
    ASSERT(pb_trajectory_total_distance(&traj) == 0);

    traj.is_valid = true;
    traj.segment_count = 2;
    traj.segments[0].start.x = 0;
    traj.segments[0].start.y = 0;
    traj.segments[0].end.x = PB_FLOAT_TO_FIXED(3.0f);
    traj.segments[0].end.y = PB_FLOAT_TO_FIXED(4.0f);
    traj.segments[1].start = traj.segments[0].end;
    traj.segments[1].end.x = PB_FLOAT_TO_FIXED(6.0f);
    traj.segments[1].end.y = PB_FLOAT_TO_FIXED(8.0f);

    pb_scalar dist = pb_trajectory_total_distance(&traj);
    /* 3-4-5 triangle = 5, then same again = 10 */
    ASSERT_NEAR(dist, PB_FLOAT_TO_FIXED(10.0f), PB_FLOAT_TO_FIXED(0.5f));
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_trajectory test suite\n");
    printf("========================\n\n");

    printf("Ray-circle intersection:\n");
    RUN(ray_circle_direct_hit);
    RUN(ray_circle_miss);
    RUN(ray_circle_behind_origin);
    RUN(ray_circle_tangent);
    RUN(ray_circle_inside_circle);

    printf("\nWall reflection:\n");
    RUN(reflect_wall_right_to_left);
    RUN(reflect_wall_left_to_right);
    RUN(reflect_wall_straight_up);

    printf("\nTrajectory computation:\n");
    RUN(trajectory_straight_up_ceiling);
    RUN(trajectory_wall_bounce);
    RUN(trajectory_bubble_collision);
    RUN(trajectory_null_inputs);
    RUN(trajectory_max_bounces_limit);

    printf("\nLanding cell:\n");
    RUN(landing_cell_empty_board);
    RUN(landing_cell_near_bubble);
    RUN(landing_cell_invalid_trajectory);

    printf("\nUtility functions:\n");
    RUN(trajectory_is_valid);
    RUN(trajectory_end_point);
    RUN(trajectory_total_distance);

    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

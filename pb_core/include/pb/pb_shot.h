/*
 * pb_shot.h - Shot mechanics and collision detection
 *
 * Implements projectile physics, wall bouncing, bubble collision,
 * and snap-to-grid logic for the bubble shooter.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_SHOT_H
#define PB_SHOT_H

#include "pb_types.h"
#include "pb_hex.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/* Default shot speed (pixels per frame at 60fps) */
#define PB_DEFAULT_SHOT_SPEED PB_FLOAT_TO_FIXED(8.0f)

/* Minimum angle from horizontal (prevents near-horizontal shots) */
#define PB_MIN_ANGLE PB_FLOAT_TO_FIXED(0.17453f)  /* ~10 degrees */

/* Maximum angle from horizontal */
#define PB_MAX_ANGLE PB_FLOAT_TO_FIXED(2.96706f)  /* pi - 10 degrees */

/* Default maximum wall bounces */
#define PB_DEFAULT_MAX_BOUNCES 2

/*============================================================================
 * Collision Result
 *============================================================================*/

typedef enum pb_collision_type {
    PB_COLLISION_NONE = 0,
    PB_COLLISION_BUBBLE,    /* Hit another bubble */
    PB_COLLISION_WALL,      /* Hit left/right wall */
    PB_COLLISION_CEILING,   /* Hit top of playfield */
    PB_COLLISION_FLOOR      /* Fell below playfield (lose condition) */
} pb_collision_type;

typedef struct pb_collision {
    pb_collision_type type;
    pb_point hit_point;         /* Exact collision point */
    pb_offset hit_cell;         /* Cell that was hit (for bubble collision) */
    pb_scalar distance;         /* Distance traveled to collision */
} pb_collision;

/*============================================================================
 * Shot Initialization
 *============================================================================*/

/**
 * Initialize shot state for a new shot.
 *
 * @param shot      Shot state to initialize
 * @param bubble    The bubble being shot
 * @param start_pos Starting position (cannon mouth)
 * @param angle     Angle in radians (0 = right, PI/2 = up, PI = left)
 * @param speed     Pixels per step
 */
void pb_shot_init(pb_shot* shot, pb_bubble bubble, pb_point start_pos,
                  pb_scalar angle, pb_scalar speed);

/**
 * Set velocity from angle and speed.
 */
void pb_shot_set_angle(pb_shot* shot, pb_scalar angle, pb_scalar speed);

/*============================================================================
 * Physics Simulation
 *============================================================================*/

/**
 * Step the shot forward by one physics tick.
 * Handles movement, wall bouncing, and collision detection.
 *
 * @param shot        Shot state
 * @param board       Board for collision testing
 * @param radius      Bubble radius
 * @param left_wall   Left boundary x-coordinate
 * @param right_wall  Right boundary x-coordinate
 * @param ceiling     Top boundary y-coordinate
 * @param floor       Bottom boundary y-coordinate (for lose detection)
 * @return            Collision information (type=NONE if no collision this step)
 */
pb_collision pb_shot_step(pb_shot* shot, const pb_board* board, pb_scalar radius,
                          pb_scalar left_wall, pb_scalar right_wall,
                          pb_scalar ceiling, pb_scalar floor);

/**
 * Simulate shot until collision (for trajectory preview).
 * Does not modify shot state.
 *
 * @param start       Starting position
 * @param velocity    Initial velocity
 * @param board       Board for collision testing
 * @param radius      Bubble radius
 * @param left_wall   Left boundary
 * @param right_wall  Right boundary
 * @param ceiling     Top boundary
 * @param max_bounces Maximum wall bounces to simulate
 * @param path_out    Output array of path points (may be NULL)
 * @param path_max    Maximum path points to record
 * @return            Final collision result
 */
pb_collision pb_shot_simulate(pb_point start, pb_vec2 velocity,
                              const pb_board* board, pb_scalar radius,
                              pb_scalar left_wall, pb_scalar right_wall,
                              pb_scalar ceiling, int max_bounces,
                              pb_point* path_out, int* path_count, int path_max);

/*============================================================================
 * Snap-to-Grid
 *============================================================================*/

/**
 * Find the best cell to snap the shot to after collision.
 * Searches the neighborhood around the collision point.
 *
 * @param board      The board
 * @param hit_point  Where the shot hit
 * @param radius     Bubble radius
 * @return           Best empty cell for placement, or {-1,-1} if none found
 */
pb_offset pb_find_snap_cell(const pb_board* board, pb_point hit_point,
                            pb_scalar radius);

/**
 * Find snap cell when hitting a specific bubble.
 * Places the shot in the empty neighbor closest to the approach direction.
 *
 * @param board      The board
 * @param hit_cell   Cell that was hit
 * @param approach   Direction of approach (normalized velocity)
 * @return           Best empty cell for placement
 */
pb_offset pb_find_snap_cell_directed(const pb_board* board, pb_offset hit_cell,
                                      pb_vec2 approach);

/*============================================================================
 * Collision Detection Primitives
 *============================================================================*/

/**
 * Ray-circle intersection test.
 *
 * @param ray_origin   Start of ray
 * @param ray_dir      Direction of ray (should be normalized)
 * @param circle_center Center of circle
 * @param circle_radius Radius of circle
 * @param t_out        Output: parameter t where ray hits circle (ray_origin + t * ray_dir)
 * @return             true if intersection found
 */
bool pb_ray_circle_intersect(pb_point ray_origin, pb_vec2 ray_dir,
                             pb_point circle_center, pb_scalar circle_radius,
                             pb_scalar* t_out);

/**
 * Check if two circles overlap.
 */
bool pb_circles_overlap(pb_point a, pb_scalar ra, pb_point b, pb_scalar rb);

/**
 * Get squared distance between two points (avoids sqrt).
 */
pb_scalar pb_point_distance_sq(pb_point a, pb_point b);

/**
 * Get distance between two points.
 */
pb_scalar pb_point_distance(pb_point a, pb_point b);

/**
 * Normalize a vector to unit length.
 */
pb_vec2 pb_vec2_normalize(pb_vec2 v);

/**
 * Reflect vector off a surface with given normal.
 */
pb_vec2 pb_vec2_reflect(pb_vec2 v, pb_vec2 normal);

#ifdef __cplusplus
}
#endif

#endif /* PB_SHOT_H */

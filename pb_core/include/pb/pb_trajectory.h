/*
 * pb_trajectory.h - Trajectory prediction for aiming assistance
 *
 * Cleanroom implementation of trajectory computation using:
 * - Quadratic line-circle intersection for collision detection
 * - Wall reflection for bounce prediction
 * - Segment chaining for full path visualization
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_TRAJECTORY_H
#define PB_TRAJECTORY_H

#include "pb_compat.h"
#include "pb_types.h"
#include "pb_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/* Maximum trajectory segments (bounces + final) */
#define PB_TRAJECTORY_MAX_SEGMENTS 8

/* Collision radius reduction factor (allows near-misses) */
#define PB_TRAJECTORY_RADIUS_FACTOR PB_FLOAT_TO_FIXED(0.925f)

/*============================================================================
 * Types
 *============================================================================*/

/* Trajectory segment endpoint type */
typedef enum pb_trajectory_endpoint {
    PB_TRAJ_ENDPOINT_NONE = 0,   /* Segment continues */
    PB_TRAJ_ENDPOINT_WALL,       /* Hit side wall (will bounce) */
    PB_TRAJ_ENDPOINT_CEILING,    /* Hit ceiling (terminal) */
    PB_TRAJ_ENDPOINT_BUBBLE,     /* Hit bubble (terminal) */
    PB_TRAJ_ENDPOINT_FLOOR       /* Hit floor (terminal, lose condition) */
} pb_trajectory_endpoint;

/* Single trajectory segment */
typedef struct pb_trajectory_segment {
    pb_point start;              /* Segment start position */
    pb_point end;                /* Segment end position */
    pb_trajectory_endpoint type; /* What ended this segment */
    pb_offset hit_cell;          /* If type == BUBBLE, the cell hit */
} pb_trajectory_segment;

/* Complete trajectory (chain of segments) */
typedef struct pb_trajectory {
    pb_trajectory_segment segments[PB_TRAJECTORY_MAX_SEGMENTS];
    int segment_count;           /* Number of valid segments */
    pb_offset landing_cell;      /* Final snap-to-grid cell */
    bool is_valid;               /* True if trajectory computed successfully */
} pb_trajectory;

/*============================================================================
 * Trajectory Computation
 *============================================================================*/

/**
 * Compute complete trajectory for a shot.
 *
 * Traces the path from origin at given angle, computing all bounces
 * and the final landing position.
 *
 * @param origin      Starting position (cannon tip)
 * @param angle       Fire angle in radians (0 = right, PI/2 = up)
 * @param speed       Shot speed (pixels per step)
 * @param board       Current board state for collision detection
 * @param radius      Bubble radius
 * @param left_wall   Left boundary x-coordinate
 * @param right_wall  Right boundary x-coordinate
 * @param ceiling     Top boundary y-coordinate
 * @param max_bounces Maximum wall bounces allowed
 * @param out         Output trajectory structure
 * @return            Number of segments computed (0 on error)
 */
int pb_trajectory_compute(
    pb_point origin,
    pb_scalar angle,
    pb_scalar speed,
    const pb_board* board,
    pb_scalar radius,
    pb_scalar left_wall,
    pb_scalar right_wall,
    pb_scalar ceiling,
    int max_bounces,
    pb_trajectory* out
);

/**
 * Find the grid cell where trajectory ends.
 *
 * Snaps the final collision point to the nearest valid empty cell.
 *
 * @param trajectory  Computed trajectory
 * @param board       Board for finding empty neighbors
 * @param radius      Bubble radius for coordinate conversion
 * @return            Grid cell for bubble placement, or {-1,-1} if invalid
 */
pb_offset pb_trajectory_get_landing(
    const pb_trajectory* trajectory,
    const pb_board* board,
    pb_scalar radius
);

/*============================================================================
 * Line-Circle Intersection (Quadratic Solver)
 *============================================================================*/

/**
 * Find intersection between ray and circle using quadratic formula.
 *
 * Solves: |P0 + t*D - C|^2 = R^2 for smallest positive t.
 *
 * @param ray_origin  Ray starting point P0
 * @param ray_dir     Ray direction D (should be normalized)
 * @param center      Circle center C
 * @param radius      Circle radius R
 * @param t_out       Output: distance along ray to intersection
 * @return            true if intersection found (t_out valid)
 */
bool pb_trajectory_ray_circle(
    pb_point ray_origin,
    pb_vec2 ray_dir,
    pb_point center,
    pb_scalar radius,
    pb_scalar* t_out
);

/*============================================================================
 * Wall Reflection
 *============================================================================*/

/**
 * Reflect velocity vector off vertical wall.
 *
 * Negates horizontal component, preserves vertical component.
 *
 * @param velocity    Input velocity vector
 * @return            Reflected velocity vector
 */
pb_vec2 pb_trajectory_reflect_wall(pb_vec2 velocity);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Check if trajectory has any valid segments.
 */
PB_INLINE bool pb_trajectory_is_valid(const pb_trajectory* traj)
{
    return traj && traj->is_valid && traj->segment_count > 0;
}

/**
 * Get the final endpoint of trajectory.
 */
PB_INLINE pb_point pb_trajectory_end_point(const pb_trajectory* traj)
{
    if (!pb_trajectory_is_valid(traj)) {
        return (pb_point){0, 0};
    }
    return traj->segments[traj->segment_count - 1].end;
}

/**
 * Get total trajectory distance.
 */
pb_scalar pb_trajectory_total_distance(const pb_trajectory* traj);

#ifdef __cplusplus
}
#endif

#endif /* PB_TRAJECTORY_H */

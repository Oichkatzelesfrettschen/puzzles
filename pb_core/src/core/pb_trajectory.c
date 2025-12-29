/*
 * pb_trajectory.c - Trajectory prediction implementation
 *
 * Cleanroom implementation based on mathematical principles:
 * - Quadratic formula for line-circle intersection
 * - Vector reflection for wall bounces
 * - Segment chaining for complete path
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_trajectory.h"
#include "pb/pb_shot.h"
#include "pb/pb_hex.h"
#include "pb/pb_freestanding.h"

/*============================================================================
 * Line-Circle Intersection (Quadratic Solver)
 *
 * Given ray P(t) = origin + t*dir and circle at center with radius R,
 * find smallest positive t where |P(t) - center|^2 = R^2.
 *
 * Expanding: |origin + t*dir - center|^2 = R^2
 * Let D = origin - center
 * |D + t*dir|^2 = R^2
 * (D.x + t*dir.x)^2 + (D.y + t*dir.y)^2 = R^2
 *
 * Expanding and collecting terms:
 * (dir.x^2 + dir.y^2)*t^2 + 2*(D.x*dir.x + D.y*dir.y)*t + (D.x^2 + D.y^2 - R^2) = 0
 *
 * This is a*t^2 + b*t + c = 0 where:
 * a = dir.x^2 + dir.y^2
 * b = 2*(D.x*dir.x + D.y*dir.y)
 * c = D.x^2 + D.y^2 - R^2
 *
 * Solution: t = (-b +/- sqrt(b^2 - 4ac)) / 2a
 *============================================================================*/

bool pb_trajectory_ray_circle(
    pb_point ray_origin,
    pb_vec2 ray_dir,
    pb_point center,
    pb_scalar radius,
    pb_scalar* t_out)
{
    /* Vector from origin to center */
    pb_scalar dx = ray_origin.x - center.x;
    pb_scalar dy = ray_origin.y - center.y;

    /* Quadratic coefficients */
    pb_scalar a = PB_FIXED_MUL(ray_dir.x, ray_dir.x) +
                  PB_FIXED_MUL(ray_dir.y, ray_dir.y);

    pb_scalar two = PB_FLOAT_TO_FIXED(2.0f);
    pb_scalar b = PB_FIXED_MUL(two,
                  PB_FIXED_MUL(dx, ray_dir.x) + PB_FIXED_MUL(dy, ray_dir.y));

    pb_scalar c = PB_FIXED_MUL(dx, dx) + PB_FIXED_MUL(dy, dy) -
                  PB_FIXED_MUL(radius, radius);

    /* Discriminant: b^2 - 4ac */
    pb_scalar four = PB_FLOAT_TO_FIXED(4.0f);
    pb_scalar discriminant = PB_FIXED_MUL(b, b) -
                             PB_FIXED_MUL(four, PB_FIXED_MUL(a, c));

    if (discriminant < 0) {
        return false;  /* No intersection */
    }

    /* Two solutions: t1 = (-b - sqrt(disc)) / 2a, t2 = (-b + sqrt(disc)) / 2a */
    pb_scalar sqrt_disc = PB_SCALAR_SQRT(discriminant);
    pb_scalar two_a = PB_FIXED_MUL(two, a);

    /* Guard against division by zero */
    if (two_a < PB_EPSILON && two_a > -PB_EPSILON) {
        return false;
    }

    /* Try smaller t first (closer intersection) */
    pb_scalar t1 = PB_FIXED_DIV(-b - sqrt_disc, two_a);
    if (t1 > PB_EPSILON) {
        *t_out = t1;
        return true;
    }

    /* If t1 is negative, try t2 */
    pb_scalar t2 = PB_FIXED_DIV(-b + sqrt_disc, two_a);
    if (t2 > PB_EPSILON) {
        *t_out = t2;
        return true;
    }

    return false;  /* Both intersections behind ray origin */
}

/*============================================================================
 * Wall Reflection
 *
 * Reflects velocity vector off a vertical wall by negating x component.
 *============================================================================*/

pb_vec2 pb_trajectory_reflect_wall(pb_vec2 velocity)
{
    return (pb_vec2){-velocity.x, velocity.y};
}

/*============================================================================
 * Find Closest Bubble Collision
 *============================================================================*/

static bool find_closest_bubble(
    pb_point origin,
    pb_vec2 dir,
    pb_scalar max_dist,
    const pb_board* board,
    pb_scalar radius,
    pb_offset* out_cell,
    pb_scalar* out_dist)
{
    bool found = false;
    pb_scalar best_t = max_dist;
    pb_scalar combined_radius = PB_FIXED_MUL(radius * 2, PB_TRAJECTORY_RADIUS_FACTOR);

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};

            if (pb_board_is_empty(board, pos)) {
                continue;
            }

            /* Skip ghost bubbles */
            const pb_bubble* b = pb_board_get_const(board, pos);
            if (b->flags & PB_FLAG_GHOST) {
                continue;
            }

            /* Get bubble center in pixel coords */
            pb_point center = pb_offset_to_pixel(pos, radius);

            pb_scalar t;
            if (pb_trajectory_ray_circle(origin, dir, center, combined_radius, &t)) {
                if (t > PB_EPSILON && t < best_t) {
                    best_t = t;
                    *out_cell = pos;
                    found = true;
                }
            }
        }
    }

    if (found) {
        *out_dist = best_t;
    }
    return found;
}

/*============================================================================
 * Check Wall Collision
 *============================================================================*/

static bool check_wall_intersection(
    pb_point origin,
    pb_vec2 dir,
    pb_scalar radius,
    pb_scalar left_wall,
    pb_scalar right_wall,
    pb_scalar* out_dist,
    bool* is_left)
{
    pb_scalar best_t = PB_FLOAT_TO_FIXED(1e6f);
    bool found = false;

    /* Left wall */
    if (dir.x < -PB_EPSILON) {
        pb_scalar t = PB_FIXED_DIV(left_wall + radius - origin.x, dir.x);
        if (t > PB_EPSILON && t < best_t) {
            best_t = t;
            *is_left = true;
            found = true;
        }
    }

    /* Right wall */
    if (dir.x > PB_EPSILON) {
        pb_scalar t = PB_FIXED_DIV(right_wall - radius - origin.x, dir.x);
        if (t > PB_EPSILON && t < best_t) {
            best_t = t;
            *is_left = false;
            found = true;
        }
    }

    if (found) {
        *out_dist = best_t;
    }
    return found;
}

/*============================================================================
 * Check Ceiling Collision
 *============================================================================*/

static bool check_ceiling_intersection(
    pb_point origin,
    pb_vec2 dir,
    pb_scalar radius,
    pb_scalar ceiling,
    pb_scalar* out_dist)
{
    /* Ceiling is above (y decreases upward) */
    if (dir.y < -PB_EPSILON) {
        pb_scalar t = PB_FIXED_DIV(ceiling + radius - origin.y, dir.y);
        if (t > PB_EPSILON) {
            *out_dist = t;
            return true;
        }
    }
    return false;
}

/*============================================================================
 * Compute Single Trajectory Segment
 *============================================================================*/

static pb_trajectory_endpoint compute_segment(
    pb_point* pos,
    pb_vec2* dir,
    const pb_board* board,
    pb_scalar radius,
    pb_scalar left_wall,
    pb_scalar right_wall,
    pb_scalar ceiling,
    pb_point* end_point,
    pb_offset* hit_cell)
{
    pb_scalar bubble_dist = PB_FLOAT_TO_FIXED(1e6f);
    pb_scalar wall_dist = PB_FLOAT_TO_FIXED(1e6f);
    pb_scalar ceiling_dist = PB_FLOAT_TO_FIXED(1e6f);
    pb_offset bubble_cell = {-1, -1};
    bool wall_is_left = false;

    /* Find all potential collisions */
    bool has_bubble = find_closest_bubble(*pos, *dir, PB_FLOAT_TO_FIXED(1e5f),
                                          board, radius, &bubble_cell, &bubble_dist);
    bool has_wall = check_wall_intersection(*pos, *dir, radius,
                                            left_wall, right_wall, &wall_dist, &wall_is_left);
    bool has_ceiling = check_ceiling_intersection(*pos, *dir, radius, ceiling, &ceiling_dist);

    /* Find closest collision */
    pb_trajectory_endpoint type = PB_TRAJ_ENDPOINT_NONE;
    pb_scalar closest = PB_FLOAT_TO_FIXED(1e6f);

    if (has_bubble && bubble_dist < closest) {
        closest = bubble_dist;
        type = PB_TRAJ_ENDPOINT_BUBBLE;
        *hit_cell = bubble_cell;
    }
    if (has_wall && wall_dist < closest) {
        closest = wall_dist;
        type = PB_TRAJ_ENDPOINT_WALL;
        hit_cell->row = -1;
        hit_cell->col = -1;
    }
    if (has_ceiling && ceiling_dist < closest) {
        closest = ceiling_dist;
        type = PB_TRAJ_ENDPOINT_CEILING;
        hit_cell->row = -1;
        hit_cell->col = -1;
    }

    if (type == PB_TRAJ_ENDPOINT_NONE) {
        /* No collision found - shouldn't happen with valid boundaries */
        return PB_TRAJ_ENDPOINT_NONE;
    }

    /* Move to collision point */
    end_point->x = pos->x + PB_FIXED_MUL(dir->x, closest);
    end_point->y = pos->y + PB_FIXED_MUL(dir->y, closest);

    /* Update position for next segment */
    *pos = *end_point;

    /* If wall hit, reflect direction */
    if (type == PB_TRAJ_ENDPOINT_WALL) {
        *dir = pb_trajectory_reflect_wall(*dir);
    }

    return type;
}

/*============================================================================
 * Main Trajectory Computation
 *============================================================================*/

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
    pb_trajectory* out)
{
    if (!out || !board) {
        return 0;
    }

    /* Speed is provided for API consistency but not used in trajectory
     * computation - we only care about the path, not travel time. */
    (void)speed;

    /* Initialize output */
    out->segment_count = 0;
    out->is_valid = false;
    out->landing_cell = (pb_offset){-1, -1};

    /* Calculate initial direction */
    pb_vec2 dir = {
        PB_SCALAR_COS(angle),
        -PB_SCALAR_SIN(angle)  /* Negative because y increases downward */
    };

    /* Normalize and scale by speed */
    pb_scalar len_sq = PB_FIXED_MUL(dir.x, dir.x) + PB_FIXED_MUL(dir.y, dir.y);
    if (len_sq < PB_EPSILON) {
        return 0;
    }
    pb_scalar len = PB_SCALAR_SQRT(len_sq);
    dir.x = PB_FIXED_DIV(dir.x, len);
    dir.y = PB_FIXED_DIV(dir.y, len);

    pb_point pos = origin;
    int bounces = 0;

    while (out->segment_count < PB_TRAJECTORY_MAX_SEGMENTS) {
        pb_trajectory_segment* seg = &out->segments[out->segment_count];
        seg->start = pos;

        pb_trajectory_endpoint type = compute_segment(
            &pos, &dir, board, radius, left_wall, right_wall, ceiling,
            &seg->end, &seg->hit_cell);

        if (type == PB_TRAJ_ENDPOINT_NONE) {
            break;
        }

        seg->type = type;
        out->segment_count++;

        /* Check for terminal collision */
        if (type == PB_TRAJ_ENDPOINT_CEILING ||
            type == PB_TRAJ_ENDPOINT_BUBBLE ||
            type == PB_TRAJ_ENDPOINT_FLOOR) {
            out->is_valid = true;
            out->landing_cell = seg->hit_cell;
            break;
        }

        /* Wall bounce */
        if (type == PB_TRAJ_ENDPOINT_WALL) {
            bounces++;
            if (bounces > max_bounces) {
                /* Treat as terminal */
                out->is_valid = true;
                break;
            }
        }
    }

    return out->segment_count;
}

/*============================================================================
 * Get Landing Cell
 *============================================================================*/

pb_offset pb_trajectory_get_landing(
    const pb_trajectory* trajectory,
    const pb_board* board,
    pb_scalar radius)
{
    if (!pb_trajectory_is_valid(trajectory) || !board) {
        return (pb_offset){-1, -1};
    }

    /* Get final endpoint */
    pb_point end = pb_trajectory_end_point(trajectory);

    /* Convert to grid coordinates */
    pb_offset cell = pb_pixel_to_offset(end, radius);

    /* If cell is empty, use it directly */
    if (pb_offset_in_bounds(cell, board->rows, board->cols_even, board->cols_odd) &&
        pb_board_is_empty(board, cell)) {
        return cell;
    }

    /* Otherwise, find nearest empty neighbor */
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(cell, neighbors);

    pb_scalar best_dist = PB_FLOAT_TO_FIXED(1e6f);
    pb_offset best_cell = {-1, -1};

    for (int i = 0; i < 6; i++) {
        if (!pb_offset_in_bounds(neighbors[i], board->rows,
                                 board->cols_even, board->cols_odd)) {
            continue;
        }
        if (!pb_board_is_empty(board, neighbors[i])) {
            continue;
        }

        pb_point neighbor_center = pb_offset_to_pixel(neighbors[i], radius);
        pb_scalar dist = pb_point_distance_sq(end, neighbor_center);
        if (dist < best_dist) {
            best_dist = dist;
            best_cell = neighbors[i];
        }
    }

    return best_cell;
}

/*============================================================================
 * Total Distance
 *============================================================================*/

pb_scalar pb_trajectory_total_distance(const pb_trajectory* traj)
{
    if (!pb_trajectory_is_valid(traj)) {
        return 0;
    }

    pb_scalar total = 0;
    for (int i = 0; i < traj->segment_count; i++) {
        const pb_trajectory_segment* seg = &traj->segments[i];
        total += pb_point_distance(seg->start, seg->end);
    }
    return total;
}

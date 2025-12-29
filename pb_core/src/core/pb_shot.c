/*
 * pb_shot.c - Shot mechanics implementation
 *
 * Collision detection uses ray-circle intersection with quadratic formula.
 * Wall bouncing reflects velocity, preserving speed.
 * Snap-to-grid finds best empty neighbor cell.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_shot.h"
#include "pb/pb_board.h"
#include "pb/pb_freestanding.h"

/*============================================================================
 * Vector Math Helpers
 *============================================================================*/

pb_scalar pb_point_distance_sq(pb_point a, pb_point b)
{
    pb_scalar dx = b.x - a.x;
    pb_scalar dy = b.y - a.y;
    return PB_FIXED_MUL(dx, dx) + PB_FIXED_MUL(dy, dy);
}

pb_scalar pb_point_distance(pb_point a, pb_point b)
{
    return PB_SCALAR_SQRT(pb_point_distance_sq(a, b));
}

pb_vec2 pb_vec2_normalize(pb_vec2 v)
{
    pb_scalar len_sq = PB_FIXED_MUL(v.x, v.x) + PB_FIXED_MUL(v.y, v.y);
    if (len_sq < PB_EPSILON) {
        return (pb_vec2){0, PB_FLOAT_TO_FIXED(-1.0f)};  /* Default to up */
    }
    pb_scalar len = PB_SCALAR_SQRT(len_sq);
    /* Guard against sqrt returning zero due to fixed-point truncation */
    if (len < PB_EPSILON) {
        return (pb_vec2){0, PB_FLOAT_TO_FIXED(-1.0f)};  /* Default to up */
    }
    return (pb_vec2){PB_FIXED_DIV(v.x, len), PB_FIXED_DIV(v.y, len)};
}

pb_vec2 pb_vec2_reflect(pb_vec2 v, pb_vec2 normal)
{
    /* r = v - 2(v.n)n */
    pb_scalar dot = PB_FIXED_MUL(v.x, normal.x) + PB_FIXED_MUL(v.y, normal.y);
    pb_scalar two = PB_FLOAT_TO_FIXED(2.0f);
    return (pb_vec2){
        v.x - PB_FIXED_MUL(PB_FIXED_MUL(two, dot), normal.x),
        v.y - PB_FIXED_MUL(PB_FIXED_MUL(two, dot), normal.y)
    };
}

bool pb_circles_overlap(pb_point a, pb_scalar ra, pb_point b, pb_scalar rb)
{
    pb_scalar dist_sq = pb_point_distance_sq(a, b);
    pb_scalar sum_r = ra + rb;
    return dist_sq < PB_FIXED_MUL(sum_r, sum_r);
}

/*============================================================================
 * Ray-Circle Intersection
 *============================================================================*/

bool pb_ray_circle_intersect(pb_point ray_origin, pb_vec2 ray_dir,
                             pb_point circle_center, pb_scalar circle_radius,
                             pb_scalar* t_out)
{
    /* Vector from ray origin to circle center */
    pb_scalar ox = ray_origin.x - circle_center.x;
    pb_scalar oy = ray_origin.y - circle_center.y;

    /* Quadratic coefficients: at^2 + bt + c = 0 */
    /* a = ray_dir . ray_dir = 1 (assuming normalized) */
    pb_scalar a = PB_FIXED_MUL(ray_dir.x, ray_dir.x) + PB_FIXED_MUL(ray_dir.y, ray_dir.y);
    pb_scalar two = PB_FLOAT_TO_FIXED(2.0f);
    pb_scalar b = PB_FIXED_MUL(two, PB_FIXED_MUL(ox, ray_dir.x) + PB_FIXED_MUL(oy, ray_dir.y));
    pb_scalar c = PB_FIXED_MUL(ox, ox) + PB_FIXED_MUL(oy, oy) - PB_FIXED_MUL(circle_radius, circle_radius);

    pb_scalar four = PB_FLOAT_TO_FIXED(4.0f);
    pb_scalar discriminant = PB_FIXED_MUL(b, b) - PB_FIXED_MUL(four, PB_FIXED_MUL(a, c));

    if (discriminant < 0) {
        return false;  /* No intersection */
    }

    pb_scalar sqrt_disc = PB_SCALAR_SQRT(discriminant);
    pb_scalar two_a = PB_FIXED_MUL(two, a);
    pb_scalar t1 = PB_FIXED_DIV(-b - sqrt_disc, two_a);
    pb_scalar t2 = PB_FIXED_DIV(-b + sqrt_disc, two_a);

    /* We want the smallest positive t */
    pb_scalar t = PB_FLOAT_TO_FIXED(-1.0f);
    if (t1 > 0) {
        t = t1;
    } else if (t2 > 0) {
        t = t2;
    }

    if (t < 0) {
        return false;  /* Intersection behind ray */
    }

    *t_out = t;
    return true;
}

/*============================================================================
 * Shot Initialization
 *============================================================================*/

void pb_shot_init(pb_shot* shot, pb_bubble bubble, pb_point start_pos,
                  pb_scalar angle, pb_scalar speed)
{
    shot->phase = PB_SHOT_MOVING;
    shot->bubble = bubble;
    shot->pos = start_pos;
    shot->bounces = 0;
    shot->max_bounces = PB_DEFAULT_MAX_BOUNCES;

    pb_shot_set_angle(shot, angle, speed);
}

void pb_shot_set_angle(pb_shot* shot, pb_scalar angle, pb_scalar speed)
{
    /* Clamp angle to valid range */
    if (angle < PB_MIN_ANGLE) angle = PB_MIN_ANGLE;
    if (angle > PB_MAX_ANGLE) angle = PB_MAX_ANGLE;

    shot->velocity.x = PB_FIXED_MUL(PB_SCALAR_COS(angle), speed);
    shot->velocity.y = -PB_FIXED_MUL(PB_SCALAR_SIN(angle), speed);  /* Negative because y increases downward */
}

/*============================================================================
 * Collision Detection
 *============================================================================*/

/* Find closest bubble collision along ray */
static bool find_bubble_collision(pb_point origin, pb_vec2 dir, pb_scalar max_dist,
                                   const pb_board* board, pb_scalar radius,
                                   pb_offset* hit_cell, pb_scalar* hit_dist)
{
    bool found = false;
    pb_scalar best_t = max_dist;

    /* Check all non-empty cells */
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

            /* Combined radius (shot radius + target radius) */
            pb_scalar combined_r = radius * 2;

            pb_scalar t;
            if (pb_ray_circle_intersect(origin, dir, center, combined_r, &t)) {
                if (t < best_t && t > 0) {
                    best_t = t;
                    *hit_cell = pos;
                    found = true;
                }
            }
        }
    }

    if (found) {
        *hit_dist = best_t;
    }
    return found;
}

/*============================================================================
 * Physics Step - Helper Functions
 *============================================================================*/

/* Check and update wall collision distance */
static void check_wall_collision(pb_point pos, pb_vec2 dir, pb_scalar radius,
                                  pb_scalar left_wall, pb_scalar right_wall,
                                  pb_scalar* collision_dist, pb_collision_type* collision_type)
{
    pb_scalar t;
    if (dir.x < 0) {
        t = PB_FIXED_DIV(left_wall + radius - pos.x, dir.x);
        if (t > 0 && t < *collision_dist) {
            *collision_dist = t;
            *collision_type = PB_COLLISION_WALL;
        }
    } else if (dir.x > 0) {
        t = PB_FIXED_DIV(right_wall - radius - pos.x, dir.x);
        if (t > 0 && t < *collision_dist) {
            *collision_dist = t;
            *collision_type = PB_COLLISION_WALL;
        }
    }
}

/* Check and update vertical boundary collision (ceiling or floor) */
static void check_vertical_collision(pb_point pos, pb_vec2 dir, pb_scalar radius,
                                      pb_scalar boundary, bool is_ceiling,
                                      pb_scalar* collision_dist, pb_collision_type* collision_type)
{
    pb_scalar t;
    if (is_ceiling && dir.y < 0) {
        t = PB_FIXED_DIV(boundary + radius - pos.y, dir.y);
        if (t > 0 && t < *collision_dist) {
            *collision_dist = t;
            *collision_type = PB_COLLISION_CEILING;
        }
    } else if (!is_ceiling && dir.y > 0) {
        t = PB_FIXED_DIV(boundary - radius - pos.y, dir.y);
        if (t > 0 && t < *collision_dist) {
            *collision_dist = t;
            *collision_type = PB_COLLISION_FLOOR;
        }
    }
}

/* Handle wall bounce, returns true if terminal (too many bounces) */
static bool handle_wall_bounce(pb_shot* shot, pb_vec2* dir, pb_collision* result)
{
    if (shot->bounces >= shot->max_bounces) {
        result->type = PB_COLLISION_BUBBLE;
        shot->phase = PB_SHOT_COLLIDED;
        return true;
    }

    pb_vec2 normal = {(dir->x > 0) ? PB_FLOAT_TO_FIXED(-1.0f) : PB_FLOAT_TO_FIXED(1.0f), 0};
    pb_vec2 reflected = pb_vec2_reflect(shot->velocity, normal);
    shot->velocity = reflected;
    *dir = pb_vec2_normalize(reflected);
    shot->bounces++;
    return false;
}

/*============================================================================
 * Physics Step - Main Function
 *============================================================================*/

pb_collision pb_shot_step(pb_shot* shot, const pb_board* board, pb_scalar radius,
                          pb_scalar left_wall, pb_scalar right_wall,
                          pb_scalar ceiling, pb_scalar floor)
{
    pb_collision result = {PB_COLLISION_NONE, {0,0}, {-1,-1}, 0};

    if (shot->phase != PB_SHOT_MOVING) {
        return result;
    }

    pb_scalar speed = PB_SCALAR_SQRT(PB_FIXED_MUL(shot->velocity.x, shot->velocity.x) +
                                      PB_FIXED_MUL(shot->velocity.y, shot->velocity.y));
    if (speed < PB_EPSILON) {
        return result;
    }

    pb_vec2 dir = pb_vec2_normalize(shot->velocity);
    pb_scalar remaining = speed;
    pb_scalar threshold = PB_FLOAT_TO_FIXED(0.001f);

    while (remaining > threshold) {
        pb_collision_type collision_type = PB_COLLISION_NONE;
        pb_scalar collision_dist = remaining + PB_FLOAT_TO_FIXED(1.0f);

        /* Check bubble collision */
        pb_offset hit_cell = {-1, -1};
        pb_scalar bubble_dist = 0;
        if (find_bubble_collision(shot->pos, dir, remaining, board, radius,
                                   &hit_cell, &bubble_dist)) {
            if (bubble_dist < collision_dist) {
                collision_dist = bubble_dist;
                collision_type = PB_COLLISION_BUBBLE;
                result.hit_cell = hit_cell;
            }
        }

        /* Check wall, ceiling, floor collisions */
        check_wall_collision(shot->pos, dir, radius, left_wall, right_wall,
                             &collision_dist, &collision_type);
        check_vertical_collision(shot->pos, dir, radius, ceiling, true,
                                  &collision_dist, &collision_type);
        check_vertical_collision(shot->pos, dir, radius, floor, false,
                                  &collision_dist, &collision_type);

        /* No collision: move full step */
        if (collision_type == PB_COLLISION_NONE) {
            shot->pos.x += PB_FIXED_MUL(dir.x, remaining);
            shot->pos.y += PB_FIXED_MUL(dir.y, remaining);
            remaining = 0;
            continue;
        }

        /* Move to collision point */
        shot->pos.x += PB_FIXED_MUL(dir.x, collision_dist);
        shot->pos.y += PB_FIXED_MUL(dir.y, collision_dist);
        remaining -= collision_dist;

        result.type = collision_type;
        result.hit_point = shot->pos;
        result.distance = speed - remaining;

        /* Handle collision by type */
        if (collision_type == PB_COLLISION_WALL) {
            if (handle_wall_bounce(shot, &dir, &result)) {
                return result;
            }
        } else {
            /* Terminal collision: ceiling, bubble, or floor */
            shot->phase = PB_SHOT_COLLIDED;
            return result;
        }
    }

    return result;
}

/*============================================================================
 * Shot Simulation (for trajectory preview)
 *============================================================================*/

pb_collision pb_shot_simulate(pb_point start, pb_vec2 velocity,
                              const pb_board* board, pb_scalar radius,
                              pb_scalar left_wall, pb_scalar right_wall,
                              pb_scalar ceiling, int max_bounces,
                              pb_point* path_out, int* path_count, int path_max)
{
    pb_shot sim;
    sim.phase = PB_SHOT_MOVING;
    sim.pos = start;
    sim.velocity = velocity;
    sim.bounces = 0;
    sim.max_bounces = max_bounces;

    int path_idx = 0;
    if (path_out && path_max > 0) {
        path_out[path_idx++] = start;
    }

    pb_collision final_result = {PB_COLLISION_NONE, start, {-1,-1}, 0};
    pb_scalar total_distance = 0;

    /* Simulate for up to 1000 steps to prevent infinite loops */
    for (int step = 0; step < 1000 && sim.phase == PB_SHOT_MOVING; step++) {
        pb_collision result = pb_shot_step(&sim, board, radius,
                                           left_wall, right_wall,
                                           ceiling, 9999.0f);  /* Large floor = no floor check */

        if (result.type != PB_COLLISION_NONE) {
            total_distance += result.distance;

            /* Record bounce points */
            if (result.type == PB_COLLISION_WALL && path_out && path_idx < path_max) {
                path_out[path_idx++] = result.hit_point;
            }

            if (result.type != PB_COLLISION_WALL) {
                /* Terminal collision */
                if (path_out && path_idx < path_max) {
                    path_out[path_idx++] = result.hit_point;
                }
                final_result = result;
                final_result.distance = total_distance;
                break;
            }
        }
    }

    if (path_count) {
        *path_count = path_idx;
    }

    return final_result;
}

/*============================================================================
 * Snap-to-Grid
 *============================================================================*/

pb_offset pb_find_snap_cell(const pb_board* board, pb_point hit_point,
                            pb_scalar radius)
{
    /* Convert hit point to offset coordinates */
    pb_offset center = pb_pixel_to_offset(hit_point, radius);

    /* If center is empty and in bounds, use it */
    if (pb_board_in_bounds(board, center) && pb_board_is_empty(board, center)) {
        return center;
    }

    /* Search neighbors for closest empty cell */
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(center, neighbors);

    pb_offset best = {-1, -1};
    pb_scalar best_dist_sq = PB_INT_TO_FIXED(10000);  /* Large sentinel value */

    for (int i = 0; i < 6; i++) {
        pb_offset n = neighbors[i];

        if (!pb_board_in_bounds(board, n)) {
            continue;
        }
        if (!pb_board_is_empty(board, n)) {
            continue;
        }

        pb_point cell_center = pb_offset_to_pixel(n, radius);
        pb_scalar dist_sq = pb_point_distance_sq(hit_point, cell_center);

        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best = n;
        }
    }

    return best;
}

pb_offset pb_find_snap_cell_directed(const pb_board* board, pb_offset hit_cell,
                                      pb_vec2 approach)
{
    /* Get neighbors of hit cell */
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(hit_cell, neighbors);

    pb_offset best = {-1, -1};
    pb_scalar best_dot = PB_FLOAT_TO_FIXED(-2.0f);  /* Dot product ranges from -1 to 1 */

    /* Direction vectors for each neighbor (approximate, normalized) */
    /* Use macros for portable fixed-point values */
    const pb_vec2 dir_vectors[6] = {
        {PB_FLOAT_TO_FIXED(1.0f), PB_FLOAT_TO_FIXED(0.0f)},        /* E */
        {PB_FLOAT_TO_FIXED(0.5f), PB_FLOAT_TO_FIXED(-0.866f)},     /* NE */
        {PB_FLOAT_TO_FIXED(-0.5f), PB_FLOAT_TO_FIXED(-0.866f)},    /* NW */
        {PB_FLOAT_TO_FIXED(-1.0f), PB_FLOAT_TO_FIXED(0.0f)},       /* W */
        {PB_FLOAT_TO_FIXED(-0.5f), PB_FLOAT_TO_FIXED(0.866f)},     /* SW */
        {PB_FLOAT_TO_FIXED(0.5f), PB_FLOAT_TO_FIXED(0.866f)},      /* SE */
    };

    pb_vec2 approach_norm = pb_vec2_normalize(approach);

    for (int i = 0; i < 6; i++) {
        pb_offset n = neighbors[i];

        if (!pb_board_in_bounds(board, n)) {
            continue;
        }
        if (!pb_board_is_empty(board, n)) {
            continue;
        }

        /* We want the neighbor most opposite to approach direction */
        /* (the one we're coming from) */
        pb_scalar dot = -(approach_norm.x * dir_vectors[i].x +
                          approach_norm.y * dir_vectors[i].y);

        if (dot > best_dot) {
            best_dot = dot;
            best = n;
        }
    }

    return best;
}

/*============================================================================
 * Magnetic Force
 *============================================================================*/

pb_vec2 pb_magnetic_force(pb_point shot_pos, pb_point magnet_pos,
                          pb_scalar magnet_strength, pb_scalar max_radius)
{
    pb_vec2 zero = {0, 0};

    /* Direction from shot to magnet */
    pb_scalar dx = magnet_pos.x - shot_pos.x;
    pb_scalar dy = magnet_pos.y - shot_pos.y;

    /* Distance squared */
    pb_scalar dist_sq = PB_FIXED_MUL(dx, dx) + PB_FIXED_MUL(dy, dy);

    /* Beyond max radius: no force */
    pb_scalar max_radius_sq = PB_FIXED_MUL(max_radius, max_radius);
    if (dist_sq > max_radius_sq) {
        return zero;
    }

    /* Clamp to minimum distance to avoid singularity */
    pb_scalar min_dist_sq = PB_FIXED_MUL(PB_MAGNETIC_MIN_DISTANCE, PB_MAGNETIC_MIN_DISTANCE);
    if (dist_sq < min_dist_sq) {
        dist_sq = min_dist_sq;
    }

    /* Distance for normalization */
    pb_scalar dist = PB_SCALAR_SQRT(dist_sq);
    if (dist < PB_EPSILON) {
        return zero;
    }

    /* Force magnitude: F = strength / distance^2 */
    pb_scalar force_mag = PB_FIXED_DIV(magnet_strength, dist_sq);

    /* Normalize direction and scale by force magnitude */
    pb_vec2 force;
    force.x = PB_FIXED_MUL(PB_FIXED_DIV(dx, dist), force_mag);
    force.y = PB_FIXED_MUL(PB_FIXED_DIV(dy, dist), force_mag);

    return force;
}

void pb_apply_magnetic_forces(pb_shot* shot, const pb_board* board,
                              pb_scalar radius)
{
    if (shot->phase != PB_SHOT_MOVING) {
        return;
    }

    /* Accumulated force from all magnetic bubbles */
    pb_scalar total_fx = 0;
    pb_scalar total_fy = 0;

    /* Scan board for magnetic bubbles */
    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            const pb_bubble* b = pb_board_get_const(board, pos);

            if (b == NULL || b->kind != PB_KIND_SPECIAL) {
                continue;
            }
            if (b->special != PB_SPECIAL_MAGNETIC) {
                continue;
            }

            /* Get magnetic bubble position in pixels */
            pb_point magnet_pos = pb_offset_to_pixel(pos, radius);

            /* Get strength from bubble or use default */
            pb_scalar strength = (b->payload.magnet_strength > 0)
                ? PB_FLOAT_TO_FIXED((float)b->payload.magnet_strength)
                : PB_MAGNETIC_DEFAULT_STRENGTH;

            /* Calculate force from this magnet */
            pb_vec2 force = pb_magnetic_force(shot->pos, magnet_pos,
                                              strength, PB_MAGNETIC_DEFAULT_RADIUS);

            total_fx += force.x;
            total_fy += force.y;
        }
    }

    /* Apply accumulated force to velocity */
    shot->velocity.x += total_fx;
    shot->velocity.y += total_fy;
}

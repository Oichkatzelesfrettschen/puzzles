/*
 * pb_solver.c - Level solvability analysis implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_solver.h"
#include "pb/pb_shot.h"
#include "pb/pb_rng.h"

#include <stdio.h>
#include "pb/pb_freestanding.h"

/*============================================================================
 * Board Analysis
 *============================================================================*/

void pb_board_analyze(const pb_board* board, pb_board_stats* stats)
{
    memset(stats, 0, sizeof(*stats));

    int total_cells = 0;
    int lowest_row = 0;

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        total_cells += cols;

        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            const pb_bubble* b = pb_board_get_const(board, pos);

            if (b->kind == PB_KIND_NONE) continue;

            stats->total_bubbles++;
            lowest_row = row;

            if (b->kind == PB_KIND_COLORED && b->color_id < PB_MAX_COLORS) {
                stats->color_counts[b->color_id]++;
            }
            if (b->kind == PB_KIND_SPECIAL && b->special < PB_SPECIAL_COUNT) {
                stats->special_counts[b->special]++;
            }
            if (b->kind == PB_KIND_BLOCKER) {
                stats->blocker_count++;
            }
        }
    }

    stats->height = stats->total_bubbles > 0 ? lowest_row + 1 : 0;
    stats->density = total_cells > 0 ?
                     (float)stats->total_bubbles / (float)total_cells : 0.0f;

    /* Find orphans */
    stats->orphan_count = pb_count_orphans(board);

    /* Find largest group (simplified - just check each color) */
    for (int c = 0; c < PB_MAX_COLORS; c++) {
        if (stats->color_counts[c] > stats->max_group_size) {
            stats->max_group_size = stats->color_counts[c];
        }
    }
}

int pb_count_orphans(const pb_board* board)
{
    /* Mark cells reachable from ceiling using BFS */
    bool attached[PB_MAX_ROWS][PB_MAX_COLS] = {0};
    pb_offset queue[PB_MAX_CELLS];
    int head = 0, tail = 0;

    /* Seed with all bubbles in row 0 */
    int row0_cols = pb_row_cols(0, board->cols_even, board->cols_odd);
    for (int col = 0; col < row0_cols; col++) {
        pb_offset pos = {0, col};
        const pb_bubble* b = pb_board_get_const(board, pos);
        if (b->kind != PB_KIND_NONE) {
            attached[0][col] = true;
            queue[tail++] = pos;
        }
    }

    /* BFS to find all attached bubbles */
    while (head < tail) {
        pb_offset cur = queue[head++];

        pb_offset neighbors[6];
        pb_hex_neighbors_offset(cur, neighbors);

        for (int i = 0; i < 6; i++) {
            pb_offset n = neighbors[i];
            if (!pb_board_in_bounds(board, n)) continue;
            if (attached[n.row][n.col]) continue;

            const pb_bubble* nb = pb_board_get_const(board, n);
            if (nb->kind == PB_KIND_NONE) continue;

            attached[n.row][n.col] = true;
            queue[tail++] = n;
        }
    }

    /* Count unattached */
    int orphans = 0;
    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            const pb_bubble* b = pb_board_get_const(board, pos);
            if (b->kind != PB_KIND_NONE && !attached[row][col]) {
                orphans++;
            }
        }
    }

    return orphans;
}

/*============================================================================
 * Validation
 *============================================================================*/

pb_validation_result pb_validate_level(const pb_board* board,
                                       const pb_ruleset* ruleset,
                                       uint8_t available_colors,
                                       pb_validation_info* info)
{
    memset(info, 0, sizeof(*info));
    info->result = PB_VALID;
    info->first_error.row = -1;
    info->first_error.col = -1;

    pb_board_stats stats;
    pb_board_analyze(board, &stats);

    /* Check for orphans */
    if (stats.orphan_count > 0) {
        info->result = PB_INVALID_ORPHANS;
        info->error_count = stats.orphan_count;
        snprintf(info->message, sizeof(info->message),
                 "%d bubble(s) not attached to ceiling", stats.orphan_count);
        return info->result;
    }

    /* Check color availability */
    uint8_t needed_colors = 0;
    for (int c = 0; c < PB_MAX_COLORS; c++) {
        if (stats.color_counts[c] > 0) {
            needed_colors |= (1 << c);
        }
    }

    uint8_t missing = needed_colors & ~available_colors;
    if (missing) {
        info->result = PB_INVALID_COLORS;
        info->missing_colors = missing;

        /* Count missing colors */
        for (int c = 0; c < PB_MAX_COLORS; c++) {
            if (missing & (1 << c)) info->error_count++;
        }

        snprintf(info->message, sizeof(info->message),
                 "Level uses %d color(s) not available in queue",
                 info->error_count);
        return info->result;
    }

    /* Check density limits */
    if (ruleset && stats.density > 0.9f) {
        info->result = PB_INVALID_TOO_DENSE;
        snprintf(info->message, sizeof(info->message),
                 "Board too dense (%.1f%% full)", stats.density * 100.0f);
        return info->result;
    }

    snprintf(info->message, sizeof(info->message),
             "Valid: %d bubbles, %d colors", stats.total_bubbles,
             __builtin_popcount(needed_colors));

    return PB_VALID;
}

/*============================================================================
 * Reachability Analysis
 *============================================================================*/

void pb_find_reachable(const pb_board* board, pb_scalar radius,
                       pb_scalar left_wall, pb_scalar right_wall,
                       pb_point cannon_pos, pb_scalar min_angle, pb_scalar max_angle,
                       bool reachable[PB_MAX_ROWS][PB_MAX_COLS])
{
    memset(reachable, 0, sizeof(bool) * PB_MAX_ROWS * PB_MAX_COLS);

    /* Sample angles across the range */
    int num_samples = 100;
    pb_scalar angle_step = (max_angle - min_angle) / (pb_scalar)num_samples;

    for (int i = 0; i <= num_samples; i++) {
        pb_scalar angle = min_angle + angle_step * (pb_scalar)i;

        /* Calculate velocity from angle */
        pb_vec2 velocity = {
            PB_FIXED_MUL(PB_SCALAR_COS(angle), PB_DEFAULT_SHOT_SPEED),
            -PB_FIXED_MUL(PB_SCALAR_SIN(angle), PB_DEFAULT_SHOT_SPEED)
        };

        /* Simulate shot */
        pb_point path[32];
        int path_count;
        pb_collision result = pb_shot_simulate(
            cannon_pos, velocity, board, radius,
            left_wall, right_wall, PB_FLOAT_TO_FIXED(0.0f), 2,
            path, &path_count, 32
        );

        if (result.type == PB_COLLISION_BUBBLE ||
            result.type == PB_COLLISION_CEILING) {
            /* Find snap cell */
            pb_offset snap = pb_find_snap_cell(board, result.hit_point, radius);
            if (snap.row >= 0 && snap.col >= 0) {
                reachable[snap.row][snap.col] = true;
            }
        }
    }
}

/*============================================================================
 * Solver Core
 *============================================================================*/

void pb_solver_init(pb_solver* solver, const pb_board* board,
                    const pb_ruleset* ruleset, uint64_t seed)
{
    memcpy(&solver->board, board, sizeof(pb_board));

    if (ruleset) {
        solver->ruleset = *ruleset;
    } else {
        /* Default ruleset */
        memset(&solver->ruleset, 0, sizeof(pb_ruleset));
        solver->ruleset.match_threshold = PB_DEFAULT_MATCH_THRESHOLD;
        solver->ruleset.cols_even = PB_DEFAULT_COLS_EVEN;
        solver->ruleset.cols_odd = PB_DEFAULT_COLS_ODD;
        solver->ruleset.rows = PB_DEFAULT_ROWS;
        solver->ruleset.max_bounces = PB_DEFAULT_MAX_BOUNCES;
    }

    pb_rng_seed(&solver->rng, seed);
    solver->queue_length = 0;
    solver->moves_made = 0;
    solver->max_depth = 5;
    solver->deterministic = false;
}

void pb_solver_set_queue(pb_solver* solver, const pb_bubble* queue, int length)
{
    int copy_len = length < 32 ? length : 32;
    memcpy(solver->queue, queue, (size_t)copy_len * sizeof(pb_bubble));
    solver->queue_length = copy_len;
    solver->deterministic = true;
}

bool pb_solver_is_cleared(const pb_solver* solver)
{
    for (int row = 0; row < solver->board.rows; row++) {
        int cols = pb_row_cols(row, solver->board.cols_even, solver->board.cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            const pb_bubble* b = pb_board_get_const(&solver->board, pos);
            if (b->kind == PB_KIND_COLORED) {
                return false;
            }
        }
    }
    return true;
}

/*============================================================================
 * Move Finding
 *============================================================================*/

/* Evaluate how good a target cell is for a given color */
static float evaluate_target(const pb_board* board, pb_offset target,
                             uint8_t color_id, int match_threshold)
{
    float score = 0.0f;

    /* Count adjacent same-color bubbles */
    pb_offset neighbors[6];
    pb_hex_neighbors_offset(target, neighbors);

    int same_color = 0;
    int any_neighbor = 0;

    for (int i = 0; i < 6; i++) {
        pb_offset n = neighbors[i];
        if (!pb_board_in_bounds(board, n)) continue;

        const pb_bubble* nb = pb_board_get_const(board, n);
        if (nb->kind == PB_KIND_NONE) continue;

        any_neighbor++;
        if (nb->kind == PB_KIND_COLORED && nb->color_id == color_id) {
            same_color++;
        }
    }

    /* No neighbors = floating shot, very bad */
    if (any_neighbor == 0) {
        return -100.0f;
    }

    /* Potential match = very good */
    if (same_color >= match_threshold - 1) {
        score += 50.0f + (float)same_color * 10.0f;
    } else if (same_color > 0) {
        score += (float)same_color * 5.0f;
    }

    /* Prefer higher rows (closer to ceiling) */
    score -= (float)target.row * 0.5f;

    return score;
}

/* Calculate expected pops and drops for a move without modifying board */
static void calculate_move_results(const pb_board* board, pb_offset target,
                                   uint8_t color_id, int match_threshold,
                                   int* out_pops, int* out_drops)
{
    *out_pops = 0;
    *out_drops = 0;

    /* Create temporary board copy to simulate the move */
    pb_board temp_board = *board;

    /* Place bubble at target */
    pb_bubble b = {
        .kind = PB_KIND_COLORED,
        .color_id = color_id,
        .flags = 0,
        .special = PB_SPECIAL_NONE,
        .payload = {0}
    };
    pb_board_set(&temp_board, target, b);

    /* Find matches */
    pb_visit_result matches;
    int match_count = pb_find_matches(&temp_board, target, &matches);

    if (match_count >= match_threshold) {
        *out_pops = match_count;

        /* Remove matches to find orphans */
        pb_board_remove_cells(&temp_board, &matches);

        /* Find orphans that would drop */
        pb_visit_result orphans;
        int orphan_count = pb_find_orphans(&temp_board, &orphans);
        *out_drops = orphan_count;
    }
}

int pb_solver_find_moves(pb_solver* solver, pb_bubble current,
                         pb_move_list* moves)
{
    moves->count = 0;

    /* Get board dimensions for simulation */
    pb_scalar radius = solver->ruleset.bubble_radius;
    pb_scalar left_wall = PB_FLOAT_TO_FIXED(0.0f);
    pb_scalar right_wall = PB_FLOAT_TO_FIXED((float)(solver->board.cols_even * 32));
    pb_scalar ceiling = PB_FLOAT_TO_FIXED(0.0f);
    pb_point cannon = {right_wall / 2, PB_FLOAT_TO_FIXED(400.0f)};

    /* Try various angles */
    int num_angles = 32;
    pb_scalar angle_min = PB_MIN_ANGLE;
    pb_scalar angle_max = PB_MAX_ANGLE;
    pb_scalar angle_step = (angle_max - angle_min) / (pb_scalar)num_angles;

    for (int a = 0; a <= num_angles && moves->count < 16; a++) {
        pb_scalar angle = angle_min + angle_step * (pb_scalar)a;

        pb_vec2 velocity = {
            PB_FIXED_MUL(PB_SCALAR_COS(angle), PB_DEFAULT_SHOT_SPEED),
            -PB_FIXED_MUL(PB_SCALAR_SIN(angle), PB_DEFAULT_SHOT_SPEED)
        };

        /* Simulate shot */
        pb_point path[8];
        int path_count;
        pb_collision result = pb_shot_simulate(
            cannon, velocity, &solver->board, radius,
            left_wall, right_wall, ceiling, solver->ruleset.max_bounces,
            path, &path_count, 8
        );

        if (result.type != PB_COLLISION_BUBBLE &&
            result.type != PB_COLLISION_CEILING) {
            continue;
        }

        /* Find landing cell */
        pb_offset snap = pb_find_snap_cell(&solver->board, result.hit_point, radius);
        if (snap.row < 0 || snap.col < 0) continue;

        /* Avoid duplicates */
        bool duplicate = false;
        for (int m = 0; m < moves->count; m++) {
            if (moves->moves[m].target.row == snap.row &&
                moves->moves[m].target.col == snap.col) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        /* Evaluate move */
        float score = evaluate_target(&solver->board, snap, current.color_id,
                                      solver->ruleset.match_threshold);

        /* Calculate expected results */
        int pops = 0, drops = 0;
        calculate_move_results(&solver->board, snap, current.color_id,
                               solver->ruleset.match_threshold, &pops, &drops);

        pb_move* move = &moves->moves[moves->count];
        move->angle = angle;
        move->color_id = current.color_id;
        move->target = snap;
        move->expected_pops = pops;
        move->expected_drops = drops;
        move->score = score;
        moves->count++;
    }

    /* Sort by score (simple bubble sort) */
    for (int i = 0; i < moves->count - 1; i++) {
        for (int j = i + 1; j < moves->count; j++) {
            if (moves->moves[j].score > moves->moves[i].score) {
                pb_move tmp = moves->moves[i];
                moves->moves[i] = moves->moves[j];
                moves->moves[j] = tmp;
            }
        }
    }

    return moves->count;
}

float pb_solver_evaluate_move(pb_solver* solver, const pb_move* move)
{
    return evaluate_target(&solver->board, move->target, move->color_id,
                           solver->ruleset.match_threshold);
}

void pb_solver_apply_move(pb_solver* solver, const pb_move* move)
{
    /* Place bubble at target */
    pb_bubble b = {
        .kind = PB_KIND_COLORED,
        .color_id = move->color_id,
        .flags = 0,
        .special = PB_SPECIAL_NONE,
        .payload = {0}
    };
    pb_board_set(&solver->board, move->target, b);

    /* Find and clear matches */
    pb_visit_result matches;
    int match_count = pb_find_matches(&solver->board, move->target, &matches);

    if (match_count >= solver->ruleset.match_threshold) {
        pb_board_remove_cells(&solver->board, &matches);

        /* Clear orphans after removing matches */
        pb_visit_result orphans;
        pb_find_orphans(&solver->board, &orphans);
        pb_board_remove_cells(&solver->board, &orphans);
    }

    solver->moves_made++;
}

/*============================================================================
 * Solvability Analysis
 *============================================================================*/

bool pb_analyze_solvability(const pb_board* board,
                            const pb_ruleset* ruleset,
                            const pb_bubble* queue, int queue_length,
                            int max_search,
                            pb_solvability* result)
{
    memset(result, 0, sizeof(*result));

    pb_solver solver;
    pb_solver_init(&solver, board, ruleset, 0);

    if (queue && queue_length > 0) {
        pb_solver_set_queue(&solver, queue, queue_length);
        result->shots_available = queue_length;
    } else {
        result->shots_available = 50;  /* Assume reasonable queue */
    }

    /* Count bubbles */
    pb_board_stats stats;
    pb_board_analyze(board, &stats);

    /* Rough estimate: need at least (total / 3) shots minimum */
    result->min_moves = (stats.total_bubbles + 2) / 3;

    /* Check if already cleared (empty board) */
    if (pb_solver_is_cleared(&solver)) {
        result->solvable = true;
        result->min_moves = 0;
        result->confidence = 1.0f;
        return true;
    }

    /* Simple greedy search */
    int moves_used = 0;
    int search_iterations = 0;
    bool cleared = false;

    while (!cleared && moves_used < result->shots_available &&
           search_iterations < max_search) {
        search_iterations++;

        /* Get next bubble */
        pb_bubble current;
        if (solver.deterministic && moves_used < solver.queue_length) {
            current = solver.queue[moves_used];
        } else {
            /* Random bubble for non-deterministic analysis */
            current.kind = PB_KIND_COLORED;
            current.color_id = (uint8_t)pb_rng_range(&solver.rng, 5);
            current.flags = 0;
            current.special = PB_SPECIAL_NONE;
        }

        /* Find best move */
        pb_move_list moves;
        int count = pb_solver_find_moves(&solver, current, &moves);

        if (count == 0) {
            /* No valid moves - level might be impossible */
            break;
        }

        /* Apply best move */
        pb_solver_apply_move(&solver, &moves.moves[0]);
        moves_used++;

        /* Record solution moves */
        if (moves_used <= 16) {
            result->solution.moves[result->solution.count++] = moves.moves[0];
        }

        cleared = pb_solver_is_cleared(&solver);
    }

    result->solvable = cleared;
    result->possibly_solvable = !cleared && moves_used < result->shots_available;
    result->min_moves = moves_used;
    result->confidence = cleared ? 1.0f :
                         (float)search_iterations / (float)max_search;

    return true;
}

/*============================================================================
 * Difficulty Estimation
 *============================================================================*/

pb_difficulty pb_estimate_difficulty(const pb_board* board,
                                     const pb_ruleset* ruleset,
                                     pb_difficulty_info* info)
{
    memset(info, 0, sizeof(*info));

    pb_board_stats stats;
    pb_board_analyze(board, &stats);

    /* Estimate moves needed */
    int estimated_moves = (stats.total_bubbles + 2) / 3;

    /* Factor in color variety */
    int color_count = 0;
    for (int c = 0; c < PB_MAX_COLORS; c++) {
        if (stats.color_counts[c] > 0) color_count++;
    }

    /* Factor in blockers */
    estimated_moves += stats.blocker_count * 2;

    /* Precision based on density */
    float precision = stats.density;

    /* Determine rating */
    pb_difficulty rating;
    if (estimated_moves < 5) {
        rating = PB_DIFFICULTY_TRIVIAL;
    } else if (estimated_moves < 15) {
        rating = PB_DIFFICULTY_EASY;
    } else if (estimated_moves < 30) {
        rating = PB_DIFFICULTY_MEDIUM;
    } else if (estimated_moves < 50) {
        rating = PB_DIFFICULTY_HARD;
    } else {
        rating = PB_DIFFICULTY_EXPERT;
    }

    /* Increase difficulty for more colors */
    if (color_count > 5 && rating < PB_DIFFICULTY_EXPERT) {
        rating++;
    }

    /* Fill in info */
    info->rating = rating;
    info->estimated_moves = estimated_moves;
    info->precision_required = precision;
    info->time_pressure = (ruleset && ruleset->shots_per_row_insert > 0) ? 0.5f : 0.0f;
    info->chokepoints = stats.blocker_count;

    const char* rating_names[] = {
        "Trivial", "Easy", "Medium", "Hard", "Expert", "Unknown"
    };
    snprintf(info->notes, sizeof(info->notes),
             "%s: ~%d moves, %d colors, %.0f%% density",
             rating_names[rating], estimated_moves, color_count,
             stats.density * 100.0f);

    return rating;
}

/*
 * pb_solver.h - Level solvability analysis and validation
 *
 * Provides tools for:
 * - Validating level constraints (reachability, color balance)
 * - Checking theoretical solvability
 * - Generating hints and optimal move sequences
 * - Level difficulty estimation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_SOLVER_H
#define PB_SOLVER_H

#include "pb_types.h"
#include "pb_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Validation Result
 *============================================================================*/

typedef enum pb_validation_result {
    PB_VALID = 0,
    PB_INVALID_ORPHANS,         /* Bubbles not attached to ceiling */
    PB_INVALID_COLORS,          /* Colors in level not available in queue */
    PB_INVALID_GEOMETRY,        /* Invalid hex grid layout */
    PB_INVALID_UNREACHABLE,     /* Some bubbles cannot be hit */
    PB_INVALID_TOO_DENSE,       /* Board exceeds density limits */
    PB_INVALID_IMPOSSIBLE,      /* No possible solution exists */
} pb_validation_result;

typedef struct pb_validation_info {
    pb_validation_result result;
    int error_count;            /* Number of issues found */
    pb_offset first_error;      /* First problematic cell */
    uint8_t missing_colors;     /* Colors needed but not in queue */
    char message[128];          /* Human-readable description */
} pb_validation_info;

/*============================================================================
 * Board Statistics
 *============================================================================*/

typedef struct pb_board_stats {
    int total_bubbles;
    int color_counts[PB_MAX_COLORS];
    int special_counts[PB_SPECIAL_COUNT];
    int max_group_size;         /* Largest same-color group */
    int orphan_count;           /* Bubbles not attached to ceiling */
    int blocker_count;          /* Indestructible blockers */
    float density;              /* Bubble count / total cells */
    int height;                 /* Lowest row with bubbles */
} pb_board_stats;

/*============================================================================
 * Move Representation
 *============================================================================*/

typedef struct pb_move {
    pb_scalar angle;            /* Shot angle (radians) */
    uint8_t color_id;           /* Bubble color */
    pb_offset target;           /* Expected landing cell */
    int expected_pops;          /* Expected bubbles popped */
    int expected_drops;         /* Expected bubbles dropped */
    float score;                /* Move quality score */
} pb_move;

typedef struct pb_move_list {
    pb_move moves[16];          /* Up to 16 candidate moves */
    int count;
} pb_move_list;

/*============================================================================
 * Solver State
 *============================================================================*/

typedef struct pb_solver {
    pb_board board;             /* Working copy of board */
    pb_ruleset ruleset;
    pb_rng rng;
    pb_bubble queue[32];        /* Known future bubbles */
    int queue_length;
    int moves_made;
    int max_depth;              /* Search depth limit */
    bool deterministic;         /* Use deterministic queue only */
} pb_solver;

/*============================================================================
 * Validation Functions
 *============================================================================*/

/**
 * Validate a level for playability.
 * Checks connectivity, color availability, and basic constraints.
 */
pb_validation_result pb_validate_level(const pb_board* board,
                                       const pb_ruleset* ruleset,
                                       uint8_t available_colors,
                                       pb_validation_info* info);

/**
 * Get board statistics.
 */
void pb_board_analyze(const pb_board* board, pb_board_stats* stats);

/**
 * Check if any bubbles are orphaned (not attached to ceiling).
 */
int pb_count_orphans(const pb_board* board);

/**
 * Find all cells reachable by shots from given position.
 * Returns bitmask of reachable cells by row.
 */
void pb_find_reachable(const pb_board* board, pb_scalar radius,
                       pb_scalar left_wall, pb_scalar right_wall,
                       pb_point cannon_pos, pb_scalar min_angle, pb_scalar max_angle,
                       bool reachable[PB_MAX_ROWS][PB_MAX_COLS]);

/*============================================================================
 * Solver Functions
 *============================================================================*/

/**
 * Initialize solver with board state.
 */
void pb_solver_init(pb_solver* solver, const pb_board* board,
                    const pb_ruleset* ruleset, uint64_t seed);

/**
 * Set known future bubble queue (for deterministic solving).
 */
void pb_solver_set_queue(pb_solver* solver, const pb_bubble* queue, int length);

/**
 * Find best moves for current board state.
 *
 * @param solver   Solver state
 * @param current  Current bubble to shoot
 * @param moves    Output: ranked move list
 * @return         Number of valid moves found
 */
int pb_solver_find_moves(pb_solver* solver, pb_bubble current,
                         pb_move_list* moves);

/**
 * Evaluate a specific move.
 *
 * @param solver  Solver state
 * @param move    Move to evaluate
 * @return        Score (higher = better)
 */
float pb_solver_evaluate_move(pb_solver* solver, const pb_move* move);

/**
 * Apply a move to solver state.
 */
void pb_solver_apply_move(pb_solver* solver, const pb_move* move);

/**
 * Check if board is cleared.
 */
bool pb_solver_is_cleared(const pb_solver* solver);

/*============================================================================
 * Solvability Analysis
 *============================================================================*/

typedef struct pb_solvability {
    bool solvable;              /* Definitely solvable with given queue */
    bool possibly_solvable;     /* Might be solvable with luck */
    int min_moves;              /* Minimum moves needed (estimate) */
    int shots_available;        /* Queue length */
    float confidence;           /* Confidence in analysis (0-1) */
    pb_move_list solution;      /* First few moves of solution */
} pb_solvability;

/**
 * Analyze level solvability.
 *
 * @param board          Initial board state
 * @param ruleset        Game rules
 * @param queue          Known bubble queue (NULL for random)
 * @param queue_length   Queue length
 * @param max_search     Maximum search iterations
 * @param result         Output: solvability analysis
 * @return               True if analysis completed
 */
bool pb_analyze_solvability(const pb_board* board,
                            const pb_ruleset* ruleset,
                            const pb_bubble* queue, int queue_length,
                            int max_search,
                            pb_solvability* result);

/*============================================================================
 * Difficulty Estimation
 *============================================================================*/

typedef enum pb_difficulty {
    PB_DIFFICULTY_TRIVIAL = 0,  /* < 5 moves, obvious solution */
    PB_DIFFICULTY_EASY,         /* 5-15 moves, straightforward */
    PB_DIFFICULTY_MEDIUM,       /* 15-30 moves, some planning needed */
    PB_DIFFICULTY_HARD,         /* 30-50 moves, careful play required */
    PB_DIFFICULTY_EXPERT,       /* 50+ moves or tight constraints */
    PB_DIFFICULTY_UNKNOWN,      /* Could not determine */
} pb_difficulty;

typedef struct pb_difficulty_info {
    pb_difficulty rating;
    int estimated_moves;
    float precision_required;   /* How precise shots need to be (0-1) */
    float time_pressure;        /* Effect of pressure mechanics (0-1) */
    int chokepoints;            /* Critical decision points */
    char notes[256];            /* Analysis notes */
} pb_difficulty_info;

/**
 * Estimate level difficulty.
 */
pb_difficulty pb_estimate_difficulty(const pb_board* board,
                                     const pb_ruleset* ruleset,
                                     pb_difficulty_info* info);

#ifdef __cplusplus
}
#endif

#endif /* PB_SOLVER_H */

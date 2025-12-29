/*
 * pb_game.h - Game state controller
 *
 * Main game loop orchestration: initialization, input handling,
 * shot resolution, match detection, and win/lose conditions.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_GAME_H
#define PB_GAME_H

#include "pb_types.h"
#include "pb_board.h"
#include "pb_shot.h"
#include "pb_effect.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Game Configuration
 *============================================================================*/

/**
 * Get default ruleset for a game mode.
 */
void pb_ruleset_default(pb_ruleset* ruleset, pb_mode_type mode);

/**
 * Validate ruleset parameters.
 */
pb_result pb_ruleset_validate(const pb_ruleset* ruleset);

/*============================================================================
 * Game Initialization
 *============================================================================*/

/**
 * Initialize a new game state.
 *
 * @param state   State to initialize
 * @param ruleset Game rules (NULL for defaults)
 * @param seed    RNG seed for deterministic generation
 * @return        PB_OK on success
 */
pb_result pb_game_init(pb_game_state* state, const pb_ruleset* ruleset,
                       uint64_t seed);

/**
 * Reset game state for a new game (preserves ruleset).
 */
void pb_game_reset(pb_game_state* state, uint64_t seed);

/**
 * Load initial board configuration from bubble array.
 *
 * @param state   Game state
 * @param bubbles Array of bubbles (row-major order, NULL = empty)
 * @param rows    Number of rows to load
 * @return        PB_OK on success
 */
pb_result pb_game_load_board(pb_game_state* state, const pb_bubble* bubbles,
                             int rows);

/*============================================================================
 * Input Handling
 *============================================================================*/

/**
 * Set cannon aim angle.
 *
 * @param state Game state
 * @param angle Angle in radians (0 = right, PI/2 = up, PI = left)
 */
void pb_game_set_angle(pb_game_state* state, pb_scalar angle);

/**
 * Adjust cannon angle by delta.
 */
void pb_game_rotate(pb_game_state* state, pb_scalar delta);

/**
 * Fire the current bubble.
 *
 * @param state Game state
 * @return      PB_OK if shot fired, error if not ready
 */
pb_result pb_game_fire(pb_game_state* state);

/**
 * Swap current and preview bubbles.
 */
pb_result pb_game_swap_bubbles(pb_game_state* state);

/**
 * Pause/unpause the game.
 */
void pb_game_pause(pb_game_state* state, bool paused);

/*============================================================================
 * Game Loop
 *============================================================================*/

/**
 * Advance game simulation by one frame.
 * Handles shot movement, collision, matching, and effects.
 *
 * @param state Game state
 * @return      Number of events generated this frame
 */
int pb_game_tick(pb_game_state* state);

/**
 * Process match detection after bubble placement.
 * Called automatically by pb_game_tick.
 *
 * @param state       Game state
 * @param placed_cell Cell where bubble was placed
 * @return            Number of bubbles popped
 */
int pb_game_process_matches(pb_game_state* state, pb_offset placed_cell);

/**
 * Process orphan detection and dropping.
 * Called automatically after matches.
 *
 * @param state Game state
 * @return      Number of bubbles dropped
 */
int pb_game_process_orphans(pb_game_state* state);

/**
 * Generate next bubble and move preview to current.
 */
void pb_game_next_bubble(pb_game_state* state);

/*============================================================================
 * Game Queries
 *============================================================================*/

/**
 * Check if game is over (won or lost).
 */
bool pb_game_is_over(const pb_game_state* state);

/**
 * Check win condition (board cleared).
 */
bool pb_game_is_won(const pb_game_state* state);

/**
 * Check lose condition (varies by mode).
 */
bool pb_game_is_lost(const pb_game_state* state);

/**
 * Get current game checksum for sync verification.
 */
uint32_t pb_game_checksum(const pb_game_state* state);

/*============================================================================
 * Event Log
 *============================================================================*/

/**
 * Get events from the event log.
 *
 * @param state      Game state
 * @param start_idx  Starting index
 * @param events     Output array
 * @param max_events Maximum events to return
 * @return           Number of events returned
 */
int pb_game_get_events(const pb_game_state* state, int start_idx,
                       pb_event* events, int max_events);

/**
 * Clear the event log.
 */
void pb_game_clear_events(pb_game_state* state);

/**
 * Add event to the log.
 */
void pb_game_add_event(pb_game_state* state, const pb_event* event);

/*============================================================================
 * Garbage Exchange (Versus Mode)
 *============================================================================*/

/**
 * Get pending garbage to send to opponent.
 * Returns the count and resets the counter.
 *
 * @param state Game state
 * @return      Number of garbage bubbles to send
 */
int pb_game_get_garbage_to_send(pb_game_state* state);

/**
 * Receive garbage from opponent.
 * Spawns random bubbles from top or inserts new row.
 *
 * @param state  Game state
 * @param count  Number of garbage bubbles
 * @return       PB_OK on success, error if game over
 */
pb_result pb_game_receive_garbage(pb_game_state* state, int count);

/**
 * Check if hurry-up warning is active.
 */
bool pb_game_is_hurry(const pb_game_state* state);

/**
 * Get frames remaining until auto-fire.
 * Returns 0 if shot is in flight or auto-fire triggered.
 */
uint32_t pb_game_get_hurry_countdown(const pb_game_state* state);

/*============================================================================
 * Playfield Geometry
 *============================================================================*/

/**
 * Get playfield boundaries for shot physics.
 */
typedef struct pb_playfield {
    pb_scalar left_wall;
    pb_scalar right_wall;
    pb_scalar ceiling;
    pb_scalar floor;
    pb_scalar bubble_radius;
    pb_point cannon_pos;
} pb_playfield;

/**
 * Calculate playfield geometry from board dimensions.
 */
void pb_playfield_calc(pb_playfield* field, const pb_board* board,
                       pb_scalar bubble_radius);

/**
 * Get trajectory preview points.
 *
 * @param state     Game state
 * @param angle     Aim angle
 * @param points    Output array of path points
 * @param max_points Maximum points to return
 * @return          Number of points in trajectory
 */
int pb_game_get_trajectory(const pb_game_state* state, pb_scalar angle,
                           pb_point* points, int max_points);

#ifdef __cplusplus
}
#endif

#endif /* PB_GAME_H */

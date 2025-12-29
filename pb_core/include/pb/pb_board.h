/*
 * pb_board.h - Board operations and traversal
 *
 * Implements the core board manipulation: placement, removal,
 * match detection, and orphan detection using unified BFS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_BOARD_H
#define PB_BOARD_H

#include "pb_types.h"
#include "pb_hex.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Board Initialization
 *============================================================================*/

/**
 * Initialize an empty board with default dimensions.
 */
void pb_board_init(pb_board* board);

/**
 * Initialize board with custom dimensions.
 */
void pb_board_init_custom(pb_board* board, int rows, int cols_even, int cols_odd);

/**
 * Clear all bubbles from the board.
 */
void pb_board_clear(pb_board* board);

/*============================================================================
 * Cell Access
 *============================================================================*/

/**
 * Get pointer to bubble at position (NULL if out of bounds).
 */
pb_bubble* pb_board_get(pb_board* board, pb_offset pos);

/**
 * Get const pointer to bubble at position (NULL if out of bounds).
 */
const pb_bubble* pb_board_get_const(const pb_board* board, pb_offset pos);

/**
 * Check if position is within board bounds.
 */
bool pb_board_in_bounds(const pb_board* board, pb_offset pos);

/**
 * Check if cell is empty (no bubble).
 */
bool pb_board_is_empty(const pb_board* board, pb_offset pos);

/**
 * Set bubble at position. Returns false if out of bounds.
 */
bool pb_board_set(pb_board* board, pb_offset pos, pb_bubble bubble);

/**
 * Remove bubble at position.
 */
void pb_board_remove(pb_board* board, pb_offset pos);

/*============================================================================
 * Traversal Result
 *============================================================================*/

/**
 * Maximum cells that can be visited in one traversal.
 * Sized for maximum possible board (may be set by pb_config.h).
 */
#ifndef PB_MAX_VISITED
#define PB_MAX_VISITED PB_MAX_CELLS
#endif

/**
 * Result of a traversal operation.
 */
typedef struct pb_visit_result {
    pb_offset cells[PB_MAX_VISITED];
    int count;
} pb_visit_result;

/*============================================================================
 * Unified Traversal Primitive
 *============================================================================*/

/**
 * Visitor predicate function.
 * Returns true if the cell should be included in traversal.
 *
 * @param board    The board being traversed
 * @param pos      Current cell position
 * @param origin   The starting cell of the traversal
 * @param userdata Optional user context
 */
typedef bool (*pb_visitor_fn)(const pb_board* board, pb_offset pos,
                               pb_offset origin, void* userdata);

/**
 * Visit all connected cells matching a predicate, starting from origin.
 * Uses BFS for efficient, stack-safe traversal.
 *
 * @param board    The board to traverse
 * @param origin   Starting cell
 * @param visitor  Predicate function (cell is visited if returns true)
 * @param userdata Optional context passed to visitor
 * @param result   Output: visited cells
 * @return         Number of cells visited
 */
int pb_visit_connected(const pb_board* board, pb_offset origin,
                       pb_visitor_fn visitor, void* userdata,
                       pb_visit_result* result);

/*============================================================================
 * Match Detection
 *============================================================================*/

/**
 * Find all bubbles that match the color at the origin position.
 * Uses pb_visit_connected internally.
 *
 * @param board   The board
 * @param origin  Starting position (must contain a colored bubble)
 * @param result  Output: matching cells
 * @return        Number of matching bubbles
 */
int pb_find_matches(const pb_board* board, pb_offset origin,
                    pb_visit_result* result);

/**
 * Check if a match exists at position meeting threshold.
 *
 * @param board     The board
 * @param pos       Position to check
 * @param threshold Minimum bubbles for a valid match (usually 3)
 * @return          true if match exists
 */
bool pb_has_match(const pb_board* board, pb_offset pos, int threshold);

/*============================================================================
 * Orphan Detection
 *============================================================================*/

/**
 * Find all bubbles connected to the ceiling (directly or indirectly).
 * Bubbles not in this set are orphans and should fall.
 *
 * @param board  The board
 * @param result Output: cells connected to ceiling
 * @return       Number of anchored cells
 */
int pb_find_anchored(const pb_board* board, pb_visit_result* result);

/**
 * Find all orphaned bubbles (not connected to ceiling).
 *
 * @param board  The board
 * @param result Output: orphaned cells
 * @return       Number of orphans
 */
int pb_find_orphans(const pb_board* board, pb_visit_result* result);

/*============================================================================
 * Batch Operations
 *============================================================================*/

/**
 * Remove all bubbles in the result set.
 *
 * @param board   The board to modify
 * @param cells   Cells to remove
 * @return        Number of cells removed
 */
int pb_board_remove_cells(pb_board* board, const pb_visit_result* cells);

/**
 * Count bubbles of each color on the board.
 *
 * @param board  The board to scan
 * @param counts Output array of PB_MAX_COLORS elements
 */
void pb_board_count_colors(const pb_board* board, int counts[PB_MAX_COLORS]);

/**
 * Get bitmask of colors present on the board.
 */
uint8_t pb_board_color_mask(const pb_board* board);

/**
 * Check if board is completely empty.
 */
bool pb_board_is_clear(const pb_board* board);

/**
 * Calculate checksum of board state for sync verification.
 */
uint32_t pb_board_checksum(const pb_board* board);

#ifdef __cplusplus
}
#endif

#endif /* PB_BOARD_H */

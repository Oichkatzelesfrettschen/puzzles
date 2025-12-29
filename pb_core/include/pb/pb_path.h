/*
 * pb_path.h - Pathfinding for hexagonal grids
 *
 * Implements A* and Jump Point Search (JPS) adapted for hex grids.
 * Based on research from:
 *   - Red Blob Games: https://www.redblobgames.com/grids/hexagons/
 *   - "Improved A* Navigation Path-Planning Algorithm Based on Hexagonal Grid"
 *     (ISPRS Int. J. Geo-Inf. 2024, 13, 166)
 *   - "An Improved Jump Point Search Pathfinding Algorithm for Hexagonal Grid Maps"
 *     (ResearchGate, May 2025)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_PATH_H
#define PB_PATH_H

#include "pb_types.h"
#include "pb_hex.h"
#include "pb_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/* Maximum path length */
#ifndef PB_MAX_PATH
#define PB_MAX_PATH 256
#endif

/* Priority queue size for A* open set */
#ifndef PB_PATH_HEAP_SIZE
#define PB_PATH_HEAP_SIZE 512
#endif

/*============================================================================
 * Path Result
 *============================================================================*/

/**
 * Result of a pathfinding operation.
 */
typedef struct pb_path_result {
    pb_offset path[PB_MAX_PATH];    /* Sequence of cells from start to goal */
    int length;                      /* Number of cells in path (0 = no path) */
    int nodes_expanded;              /* Stats: nodes evaluated */
    int nodes_visited;               /* Stats: nodes added to open set */
} pb_path_result;

/*============================================================================
 * Terrain Cost Function
 *============================================================================*/

/**
 * Terrain cost callback.
 * Returns the cost to enter a cell, or -1 if impassable.
 * Default cost is 1 for all passable cells.
 *
 * @param board    The game board
 * @param pos      Cell position to evaluate
 * @param userdata Optional user context
 * @return         Cost to enter (>0), or -1 if blocked
 */
typedef int (*pb_terrain_cost_fn)(const pb_board* board, pb_offset pos,
                                   void* userdata);

/*============================================================================
 * Heuristic Functions
 *============================================================================*/

/**
 * Heuristic types for A*.
 */
typedef enum pb_heuristic_type {
    PB_HEURISTIC_MANHATTAN = 0,  /* Standard hex distance */
    PB_HEURISTIC_EUCLIDEAN,      /* Euclidean distance in pixel space */
    PB_HEURISTIC_WEIGHTED,       /* Weighted A* (epsilon * manhattan) */
    PB_HEURISTIC_TERRAIN         /* Terrain-aware (from 2024 paper) */
} pb_heuristic_type;

/*============================================================================
 * Pathfinder Configuration
 *============================================================================*/

/**
 * Configuration for pathfinding operations.
 */
typedef struct pb_pathfinder_config {
    pb_heuristic_type heuristic;     /* Heuristic function to use */
    pb_scalar weight;                 /* Weight for weighted A* (>1 = faster but suboptimal) */
    pb_terrain_cost_fn cost_fn;       /* Custom terrain cost (NULL = default) */
    void* cost_userdata;              /* Context for cost function */
    bool allow_diagonal;              /* Allow all 6 hex directions (always true for hex) */
    int max_iterations;               /* Safety limit (0 = unlimited) */
} pb_pathfinder_config;

/**
 * Get default pathfinder configuration.
 */
pb_pathfinder_config pb_pathfinder_default_config(void);

/*============================================================================
 * A* Pathfinding
 *============================================================================*/

/**
 * Find shortest path using A* algorithm.
 *
 * @param board   The game board
 * @param start   Starting position
 * @param goal    Goal position
 * @param config  Pathfinder configuration (NULL = defaults)
 * @param result  Output: found path
 * @return        true if path found, false otherwise
 */
bool pb_astar_find_path(const pb_board* board, pb_offset start, pb_offset goal,
                        const pb_pathfinder_config* config, pb_path_result* result);

/*============================================================================
 * Jump Point Search (JPS) for Hex Grids
 *============================================================================*/

/**
 * Find path using Jump Point Search.
 * More efficient than A* for medium-to-long paths on uniform terrain.
 * Based on the May 2025 improved JPS algorithm for hex grids.
 *
 * @param board   The game board
 * @param start   Starting position
 * @param goal    Goal position
 * @param config  Pathfinder configuration (NULL = defaults)
 * @param result  Output: found path
 * @return        true if path found, false otherwise
 */
bool pb_jps_find_path(const pb_board* board, pb_offset start, pb_offset goal,
                      const pb_pathfinder_config* config, pb_path_result* result);

/*============================================================================
 * Distance and Reachability
 *============================================================================*/

/**
 * Calculate exact path distance between two cells.
 * Returns -1 if no path exists.
 */
int pb_path_distance(const pb_board* board, pb_offset start, pb_offset goal,
                     const pb_pathfinder_config* config);

/**
 * Check if goal is reachable from start.
 * More efficient than full pathfinding for simple reachability tests.
 */
bool pb_is_reachable(const pb_board* board, pb_offset start, pb_offset goal,
                     const pb_pathfinder_config* config);

/*============================================================================
 * Flood Fill Pathfinding
 *============================================================================*/

/**
 * Result of a flood fill (reachable area) operation.
 */
typedef struct pb_flood_result {
    pb_offset cells[PB_MAX_CELLS];   /* Reachable cells */
    int distances[PB_MAX_CELLS];     /* Distance from origin for each cell */
    int count;                        /* Number of reachable cells */
} pb_flood_result;

/**
 * Find all cells reachable within a given distance.
 * Uses BFS for exact distances.
 *
 * @param board       The game board
 * @param origin      Starting position
 * @param max_dist    Maximum distance (0 = unlimited)
 * @param config      Pathfinder configuration (NULL = defaults)
 * @param result      Output: reachable cells with distances
 * @return            Number of reachable cells
 */
int pb_flood_fill(const pb_board* board, pb_offset origin, int max_dist,
                  const pb_pathfinder_config* config, pb_flood_result* result);

/*============================================================================
 * Line of Sight
 *============================================================================*/

/**
 * Check if there's clear line of sight between two cells.
 * Uses hex line drawing to check for obstacles.
 *
 * @param board   The game board
 * @param from    Starting position
 * @param to      Target position
 * @return        true if unobstructed line of sight
 */
bool pb_has_line_of_sight(const pb_board* board, pb_offset from, pb_offset to);

/**
 * Get all cells along a line of sight.
 * Stops at first obstacle.
 *
 * @param board   The game board
 * @param from    Starting position
 * @param to      Target position
 * @param out     Output array of cells
 * @param max     Maximum cells to output
 * @return        Number of cells (includes endpoint if reached)
 */
int pb_get_line_of_sight(const pb_board* board, pb_offset from, pb_offset to,
                         pb_offset* out, int max);

/*============================================================================
 * Field of View
 *============================================================================*/

/**
 * Result of field of view calculation.
 */
typedef struct pb_fov_result {
    pb_offset visible[PB_MAX_CELLS]; /* Visible cells */
    int count;                        /* Number of visible cells */
} pb_fov_result;

/**
 * Calculate visible cells from a position (shadowcasting for hex).
 *
 * @param board   The game board
 * @param origin  Observer position
 * @param radius  Maximum view distance
 * @param result  Output: visible cells
 * @return        Number of visible cells
 */
int pb_calculate_fov(const pb_board* board, pb_offset origin, int radius,
                     pb_fov_result* result);

#ifdef __cplusplus
}
#endif

#endif /* PB_PATH_H */

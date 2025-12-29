/*
 * pb_hex.h - Hexagonal grid mathematics
 *
 * Based on canonical algorithms from Red Blob Games:
 * https://www.redblobgames.com/grids/hexagons/
 *
 * Implements offset, axial, and cube coordinate systems with
 * conversions, neighbor iteration, distance, and rounding.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_HEX_H
#define PB_HEX_H

#include "pb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *
 * Fixed-point mode requires pre-computed Q16.16 constants to avoid
 * float-to-int conversion overhead and precision loss.
 *============================================================================*/

#if PB_USE_FIXED_POINT
/* Q16.16 fixed-point constants (value * 65536) */
#define PB_SQRT3      113512    /* 1.7320508... * 65536 */
#define PB_SQRT3_2    56756     /* 0.8660254... * 65536 */
#define PB_0_75       49152     /* 0.75 * 65536 */
#define PB_1_OVER_3   21845     /* (1/3) * 65536 */
#define PB_2_OVER_3   43691     /* (2/3) * 65536 */
#define PB_SQRT3_OVER_3 37837   /* (sqrt(3)/3) * 65536 */
#else
/* Floating-point constants */
#define PB_SQRT3        1.7320508075688772935f
#define PB_SQRT3_2      0.8660254037844386468f
#define PB_0_75         0.75f
#define PB_1_OVER_3     0.33333333333333333f
#define PB_2_OVER_3     0.66666666666666667f
#define PB_SQRT3_OVER_3 0.57735026918962576f
#endif

/* Number of neighbors for a hexagonal cell */
#define PB_HEX_NEIGHBORS 6

/* Neighbor direction indices */
typedef enum pb_hex_dir {
    PB_DIR_E = 0,   /* East (right) */
    PB_DIR_NE,      /* Northeast */
    PB_DIR_NW,      /* Northwest */
    PB_DIR_W,       /* West (left) */
    PB_DIR_SW,      /* Southwest */
    PB_DIR_SE       /* Southeast */
} pb_hex_dir;

/*============================================================================
 * Coordinate Conversions
 *============================================================================*/

/**
 * Convert offset coordinates to axial.
 * Uses "odd-r" layout (odd rows shifted right).
 */
pb_axial pb_offset_to_axial(pb_offset off);

/**
 * Convert axial coordinates to offset.
 * Uses "odd-r" layout (odd rows shifted right).
 */
pb_offset pb_axial_to_offset(pb_axial ax);

/**
 * Convert axial to cube coordinates.
 * Calculates s = -q - r to satisfy q + r + s = 0 invariant.
 */
pb_cube pb_axial_to_cube(pb_axial ax);

/**
 * Convert cube to axial coordinates.
 * Simply drops the s component.
 */
pb_axial pb_cube_to_axial(pb_cube cb);

/**
 * Convert offset directly to cube.
 */
pb_cube pb_offset_to_cube(pb_offset off);

/**
 * Convert cube directly to offset.
 */
pb_offset pb_cube_to_offset(pb_cube cb);

/*============================================================================
 * Neighbor Operations
 *============================================================================*/

/**
 * Get all 6 neighbors of a hex in offset coordinates.
 * Correctly handles odd/even row offset.
 *
 * @param off       The center cell
 * @param neighbors Output array of 6 neighbors (must be pre-allocated)
 */
void pb_hex_neighbors_offset(pb_offset off, pb_offset neighbors[6]);

/**
 * Get a specific neighbor by direction in offset coordinates.
 */
pb_offset pb_hex_neighbor_offset(pb_offset off, pb_hex_dir dir);

/**
 * Get all 6 neighbors of a hex in axial coordinates.
 *
 * @param ax        The center cell
 * @param neighbors Output array of 6 neighbors (must be pre-allocated)
 */
void pb_hex_neighbors_axial(pb_axial ax, pb_axial neighbors[6]);

/**
 * Get a specific neighbor by direction in axial coordinates.
 */
pb_axial pb_hex_neighbor_axial(pb_axial ax, pb_hex_dir dir);

/**
 * Get all 6 neighbors of a hex in cube coordinates.
 *
 * @param cb        The center cell
 * @param neighbors Output array of 6 neighbors (must be pre-allocated)
 */
void pb_hex_neighbors_cube(pb_cube cb, pb_cube neighbors[6]);

/**
 * Get a specific neighbor by direction in cube coordinates.
 */
pb_cube pb_hex_neighbor_cube(pb_cube cb, pb_hex_dir dir);

/*============================================================================
 * Distance and Comparison
 *============================================================================*/

/**
 * Calculate Manhattan distance between two hexes in cube coordinates.
 * This is the number of steps needed to move between cells.
 */
int pb_hex_distance_cube(pb_cube a, pb_cube b);

/**
 * Calculate distance in axial coordinates.
 */
int pb_hex_distance_axial(pb_axial a, pb_axial b);

/**
 * Calculate distance in offset coordinates.
 */
int pb_hex_distance_offset(pb_offset a, pb_offset b);

/**
 * Check if two offset coordinates are equal.
 */
bool pb_offset_eq(pb_offset a, pb_offset b);

/**
 * Check if two axial coordinates are equal.
 */
bool pb_axial_eq(pb_axial a, pb_axial b);

/**
 * Check if two cube coordinates are equal.
 */
bool pb_cube_eq(pb_cube a, pb_cube b);

/*============================================================================
 * Pixel <-> Hex Conversions
 *============================================================================*/

/**
 * Convert offset coordinates to pixel position (center of hex).
 *
 * @param off    Offset coordinates
 * @param radius Hex radius (distance from center to corner)
 * @return       Pixel position of hex center
 */
pb_point pb_offset_to_pixel(pb_offset off, pb_scalar radius);

/**
 * Convert pixel position to fractional cube coordinates.
 * Use pb_hex_round() to get integer coordinates.
 *
 * @param pt     Pixel position
 * @param radius Hex radius
 * @return       Fractional cube coordinates
 */
pb_cube_frac pb_pixel_to_cube_frac(pb_point pt, pb_scalar radius);

/**
 * Round fractional cube coordinates to nearest hex.
 * Maintains q + r + s = 0 invariant.
 */
pb_cube pb_hex_round(pb_cube_frac cf);

/**
 * Convert pixel position to offset coordinates (snaps to nearest hex).
 *
 * @param pt     Pixel position
 * @param radius Hex radius
 * @return       Nearest hex in offset coordinates
 */
pb_offset pb_pixel_to_offset(pb_point pt, pb_scalar radius);

/*============================================================================
 * Grid Bounds Checking
 *============================================================================*/

/**
 * Check if offset coordinates are within board bounds.
 *
 * @param off       Coordinates to check
 * @param rows      Number of rows
 * @param cols_even Columns in even rows
 * @param cols_odd  Columns in odd rows
 * @return          true if within bounds
 */
bool pb_offset_in_bounds(pb_offset off, int rows, int cols_even, int cols_odd);

/**
 * Get the number of columns for a given row.
 */
int pb_row_cols(int row, int cols_even, int cols_odd);

/*============================================================================
 * Line Drawing (Bresenham's for hex)
 *============================================================================*/

/**
 * Get hex cells along a line between two points.
 * Uses linear interpolation in cube space.
 *
 * @param start  Starting hex
 * @param end    Ending hex
 * @param out    Output array (must be large enough)
 * @param max    Maximum cells to output
 * @return       Number of cells in the line
 */
int pb_hex_line(pb_cube start, pb_cube end, pb_cube* out, int max);

/*============================================================================
 * Linear Array Neighbor Iteration (HP48-inspired optimization)
 *
 * For tight inner loops, using pre-computed pointer offsets avoids
 * coordinate conversion overhead. The board is stored as a linear array
 * with row-major order: index = row * PB_MAX_COLS + col
 *
 * HP48 uses this for match/orphan detection with direct pointer arithmetic.
 *============================================================================*/

/**
 * Pre-computed neighbor offsets for linear array access.
 * Even and odd rows have different offsets due to hex staggering.
 *
 * Layout (odd-r, odd rows offset right):
 *   Even row: cols_even bubbles
 *   Odd row:  cols_odd bubbles (shifted right by half bubble width)
 *
 * For standard 8-column even / 7-column odd layout:
 *   Even row neighbors: {E:+1, NE:-7, NW:-8, W:-1, SW:+8, SE:+9}
 *   Odd row neighbors:  {E:+1, NE:-8, NW:-9, W:-1, SW:+7, SE:+8}
 */
extern const int8_t pb_neighbor_offsets_even[6];
extern const int8_t pb_neighbor_offsets_odd[6];

/**
 * Get neighbor offsets for a given row parity.
 * @param row_is_odd  True if row number is odd
 * @return            Pointer to array of 6 neighbor offsets
 */
PB_INLINE const int8_t* pb_get_neighbor_offsets(bool row_is_odd) {
    return row_is_odd ? pb_neighbor_offsets_odd : pb_neighbor_offsets_even;
}

/**
 * Convert offset coordinates to linear array index.
 * @param off  Offset coordinates
 * @return     Linear index into board array
 */
PB_INLINE int pb_offset_to_linear(pb_offset off) {
    return off.row * PB_MAX_COLS + off.col;
}

/**
 * Convert linear index to offset coordinates.
 * @param idx  Linear index
 * @return     Offset coordinates
 */
PB_INLINE pb_offset pb_linear_to_offset(int idx) {
    pb_offset off;
    off.row = idx / PB_MAX_COLS;
    off.col = idx % PB_MAX_COLS;
    return off;
}

/**
 * Iterate neighbors using linear array offsets.
 *
 * Example usage (avoid coordinate conversions in inner loop):
 * @code
 *     int idx = pb_offset_to_linear(cell);
 *     const int8_t* offsets = pb_get_neighbor_offsets(cell.row & 1);
 *     for (int i = 0; i < 6; i++) {
 *         int neighbor_idx = idx + offsets[i];
 *         if (neighbor_idx >= 0 && neighbor_idx < PB_MAX_CELLS) {
 *             // Process neighbor at neighbor_idx
 *         }
 *     }
 * @endcode
 */

/**
 * Macro for iterating neighbors with linear array indexing.
 * More efficient than pb_hex_neighbors_offset() for tight loops.
 *
 * @param idx        Linear index of center cell
 * @param row        Row number of center cell (for parity check)
 * @param cols       Number of columns (for bounds check)
 * @param neighbor   Loop variable (int) for neighbor index
 *
 * Usage:
 * @code
 *     int idx = cell.row * PB_MAX_COLS + cell.col;
 *     PB_FOR_EACH_NEIGHBOR_LINEAR(idx, cell.row, PB_MAX_COLS, neighbor_idx) {
 *         if (board_array[neighbor_idx].kind != PB_KIND_NONE) {
 *             // Process occupied neighbor
 *         }
 *     }
 * @endcode
 */
#define PB_FOR_EACH_NEIGHBOR_LINEAR(idx, row, cols, neighbor) \
    for (const int8_t* _pb_offs = pb_get_neighbor_offsets((row) & 1), \
                      *_pb_end = _pb_offs + 6; \
         _pb_offs < _pb_end && \
         ((neighbor) = (idx) + *_pb_offs, 1); \
         ++_pb_offs) \
        if ((neighbor) >= 0 && (neighbor) < (int)((cols) * PB_MAX_ROWS))

#ifdef __cplusplus
}
#endif

#endif /* PB_HEX_H */

/*
 * pb_hex.c - Hexagonal grid mathematics implementation
 *
 * Based on canonical algorithms from Red Blob Games:
 * https://www.redblobgames.com/grids/hexagons/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_hex.h"
#include <math.h>

/*============================================================================
 * Static Data: Neighbor Offsets
 *============================================================================*/

/* Axial/Cube direction vectors (E, NE, NW, W, SW, SE) */
static const int cube_directions[6][3] = {
    {+1,  0, -1},  /* E  */
    {+1, -1,  0},  /* NE */
    { 0, -1, +1},  /* NW */
    {-1,  0, +1},  /* W  */
    {-1, +1,  0},  /* SW */
    { 0, +1, -1},  /* SE */
};

/* Offset neighbor deltas for even rows (row % 2 == 0) */
static const int offset_neighbors_even[6][2] = {
    {+1,  0},  /* E  */
    { 0, -1},  /* NE */
    {-1, -1},  /* NW */
    {-1,  0},  /* W  */
    {-1, +1},  /* SW */
    { 0, +1},  /* SE */
};

/* Offset neighbor deltas for odd rows (row % 2 == 1) */
static const int offset_neighbors_odd[6][2] = {
    {+1,  0},  /* E  */
    {+1, -1},  /* NE */
    { 0, -1},  /* NW */
    {-1,  0},  /* W  */
    { 0, +1},  /* SW */
    {+1, +1},  /* SE */
};

/*============================================================================
 * Coordinate Conversions
 *============================================================================*/

pb_axial pb_offset_to_axial(pb_offset off)
{
    pb_axial ax;
    ax.q = off.col - (off.row - (off.row & 1)) / 2;
    ax.r = off.row;
    return ax;
}

pb_offset pb_axial_to_offset(pb_axial ax)
{
    pb_offset off;
    off.col = ax.q + (ax.r - (ax.r & 1)) / 2;
    off.row = ax.r;
    return off;
}

pb_cube pb_axial_to_cube(pb_axial ax)
{
    pb_cube cb;
    cb.q = ax.q;
    cb.r = ax.r;
    cb.s = -ax.q - ax.r;  /* Maintains q + r + s = 0 */
    return cb;
}

pb_axial pb_cube_to_axial(pb_cube cb)
{
    pb_axial ax;
    ax.q = cb.q;
    ax.r = cb.r;
    return ax;
}

pb_cube pb_offset_to_cube(pb_offset off)
{
    return pb_axial_to_cube(pb_offset_to_axial(off));
}

pb_offset pb_cube_to_offset(pb_cube cb)
{
    return pb_axial_to_offset(pb_cube_to_axial(cb));
}

/*============================================================================
 * Neighbor Operations
 *============================================================================*/

void pb_hex_neighbors_offset(pb_offset off, pb_offset neighbors[6])
{
    const int (*deltas)[2] = (off.row & 1) ?
        offset_neighbors_odd : offset_neighbors_even;

    for (int i = 0; i < 6; i++) {
        neighbors[i].col = off.col + deltas[i][0];
        neighbors[i].row = off.row + deltas[i][1];
    }
}

pb_offset pb_hex_neighbor_offset(pb_offset off, pb_hex_dir dir)
{
    const int (*deltas)[2] = (off.row & 1) ?
        offset_neighbors_odd : offset_neighbors_even;

    pb_offset result;
    result.col = off.col + deltas[dir][0];
    result.row = off.row + deltas[dir][1];
    return result;
}

void pb_hex_neighbors_axial(pb_axial ax, pb_axial neighbors[6])
{
    for (int i = 0; i < 6; i++) {
        neighbors[i].q = ax.q + cube_directions[i][0];
        neighbors[i].r = ax.r + cube_directions[i][1];
    }
}

pb_axial pb_hex_neighbor_axial(pb_axial ax, pb_hex_dir dir)
{
    pb_axial result;
    result.q = ax.q + cube_directions[dir][0];
    result.r = ax.r + cube_directions[dir][1];
    return result;
}

void pb_hex_neighbors_cube(pb_cube cb, pb_cube neighbors[6])
{
    for (int i = 0; i < 6; i++) {
        neighbors[i].q = cb.q + cube_directions[i][0];
        neighbors[i].r = cb.r + cube_directions[i][1];
        neighbors[i].s = cb.s + cube_directions[i][2];
    }
}

pb_cube pb_hex_neighbor_cube(pb_cube cb, pb_hex_dir dir)
{
    pb_cube result;
    result.q = cb.q + cube_directions[dir][0];
    result.r = cb.r + cube_directions[dir][1];
    result.s = cb.s + cube_directions[dir][2];
    return result;
}

/*============================================================================
 * Distance and Comparison
 *============================================================================*/

static inline int abs_int(int x)
{
    return x < 0 ? -x : x;
}

static inline int max_int(int a, int b)
{
    return a > b ? a : b;
}

static inline int max3_int(int a, int b, int c)
{
    return max_int(a, max_int(b, c));
}

int pb_hex_distance_cube(pb_cube a, pb_cube b)
{
    /* Manhattan distance in cube space, divided by 2 because we count each step twice */
    return (abs_int(a.q - b.q) + abs_int(a.r - b.r) + abs_int(a.s - b.s)) / 2;
}

int pb_hex_distance_axial(pb_axial a, pb_axial b)
{
    return pb_hex_distance_cube(pb_axial_to_cube(a), pb_axial_to_cube(b));
}

int pb_hex_distance_offset(pb_offset a, pb_offset b)
{
    return pb_hex_distance_cube(pb_offset_to_cube(a), pb_offset_to_cube(b));
}

bool pb_offset_eq(pb_offset a, pb_offset b)
{
    return a.row == b.row && a.col == b.col;
}

bool pb_axial_eq(pb_axial a, pb_axial b)
{
    return a.q == b.q && a.r == b.r;
}

bool pb_cube_eq(pb_cube a, pb_cube b)
{
    return a.q == b.q && a.r == b.r && a.s == b.s;
}

/*============================================================================
 * Pixel <-> Hex Conversions
 *============================================================================*/

pb_point pb_offset_to_pixel(pb_offset off, pb_scalar radius)
{
    pb_point pt;
    pb_scalar width = PB_FIXED_MUL(radius, PB_INT_TO_FIXED(2));
    pb_scalar height = PB_FIXED_MUL(PB_SQRT3, radius);
    pb_scalar width_75 = PB_FIXED_MUL(width, PB_0_75);

    /* Pointy-top hex layout */
    pt.x = radius + PB_FIXED_MUL(PB_INT_TO_FIXED(off.col), width_75);
    if (off.row & 1) {
        pt.x += PB_FIXED_MUL(radius, PB_0_75);  /* Odd rows shifted right */
    }
    pt.y = PB_FIXED_MUL(radius, PB_SQRT3_2)
           + PB_FIXED_MUL(PB_INT_TO_FIXED(off.row), height);

    return pt;
}

pb_cube_frac pb_pixel_to_cube_frac(pb_point pt, pb_scalar radius)
{
    pb_cube_frac cf;

    /* Convert to axial (pointy-top layout) */
    pb_scalar q = PB_FIXED_DIV(PB_FIXED_MUL(PB_SQRT3_OVER_3, pt.x)
                             - PB_FIXED_MUL(PB_1_OVER_3, pt.y), radius);
    pb_scalar r = PB_FIXED_DIV(PB_FIXED_MUL(PB_2_OVER_3, pt.y), radius);

    cf.q = q;
    cf.r = r;
    cf.s = -q - r;

    return cf;
}

pb_cube pb_hex_round(pb_cube_frac cf)
{
    pb_cube cb;

    /* Round each component */
    int q = (int)PB_SCALAR_ROUND(cf.q);
    int r = (int)PB_SCALAR_ROUND(cf.r);
    int s = (int)PB_SCALAR_ROUND(cf.s);

    /* Calculate rounding errors */
    pb_scalar q_diff = PB_SCALAR_ABS(cf.q - PB_INT_TO_FIXED(q));
    pb_scalar r_diff = PB_SCALAR_ABS(cf.r - PB_INT_TO_FIXED(r));
    pb_scalar s_diff = PB_SCALAR_ABS(cf.s - PB_INT_TO_FIXED(s));

    /* Reset the component with largest rounding error */
    if (q_diff > r_diff && q_diff > s_diff) {
        q = -r - s;
    } else if (r_diff > s_diff) {
        r = -q - s;
    } else {
        s = -q - r;
    }

    cb.q = q;
    cb.r = r;
    cb.s = s;

    return cb;
}

pb_offset pb_pixel_to_offset(pb_point pt, pb_scalar radius)
{
    pb_cube_frac cf = pb_pixel_to_cube_frac(pt, radius);
    pb_cube cb = pb_hex_round(cf);
    return pb_cube_to_offset(cb);
}

/*============================================================================
 * Grid Bounds Checking
 *============================================================================*/

int pb_row_cols(int row, int cols_even, int cols_odd)
{
    return (row & 1) ? cols_odd : cols_even;
}

bool pb_offset_in_bounds(pb_offset off, int rows, int cols_even, int cols_odd)
{
    if (off.row < 0 || off.row >= rows) {
        return false;
    }
    int cols = pb_row_cols(off.row, cols_even, cols_odd);
    return off.col >= 0 && off.col < cols;
}

/*============================================================================
 * Line Drawing
 *============================================================================*/

static pb_scalar lerp(pb_scalar a, pb_scalar b, pb_scalar t)
{
    return a + (b - a) * t;
}

static pb_cube_frac cube_lerp(pb_cube a, pb_cube b, pb_scalar t)
{
    pb_cube_frac cf;
    cf.q = lerp((pb_scalar)a.q, (pb_scalar)b.q, t);
    cf.r = lerp((pb_scalar)a.r, (pb_scalar)b.r, t);
    cf.s = lerp((pb_scalar)a.s, (pb_scalar)b.s, t);
    return cf;
}

int pb_hex_line(pb_cube start, pb_cube end, pb_cube* out, int max)
{
    int dist = pb_hex_distance_cube(start, end);
    if (dist == 0) {
        if (max > 0) {
            out[0] = start;
            return 1;
        }
        return 0;
    }

    int count = 0;
    for (int i = 0; i <= dist && count < max; i++) {
        pb_scalar t = PB_FIXED_DIV(PB_INT_TO_FIXED(i), PB_INT_TO_FIXED(dist));
        pb_cube_frac cf = cube_lerp(start, end, t);
        /* Add small nudge to avoid edge cases */
        cf.q += PB_EPSILON;
        cf.r += PB_EPSILON;
        cf.s -= PB_EPSILON * 2;
        out[count++] = pb_hex_round(cf);
    }

    return count;
}

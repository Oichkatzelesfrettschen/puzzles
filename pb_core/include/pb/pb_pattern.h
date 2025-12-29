/**
 * @file pb_pattern.h
 * @brief Pattern overlay system for CVD accessibility
 *
 * Provides shape and pattern definitions that can be overlaid on bubbles
 * to provide redundant visual encoding beyond color alone.
 *
 * Pattern Types:
 * - Shapes: Circle, triangle, square, diamond, star, cross, ring, heart
 * - Lines: Horizontal, vertical, diagonal left/right
 * - Hatches: Crosshatch, dense hatching
 * - Fills: Solid, dotted, stippled
 *
 * Each pattern is defined with:
 * - Primitive geometry (can be rendered as SVG, Canvas, or rasterized)
 * - A unique ID for serialization
 * - Recommended foreground/background contrast
 */

#ifndef PB_PATTERN_H
#define PB_PATTERN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Pattern Types
 * ============================================================================ */

/** Pattern category */
typedef enum pb_pattern_category {
    PB_PATTERN_CAT_NONE = 0,
    PB_PATTERN_CAT_SHAPE,      /* Centered shape (triangle, star, etc.) */
    PB_PATTERN_CAT_LINE,       /* Line-based pattern */
    PB_PATTERN_CAT_HATCH,      /* Cross-hatching */
    PB_PATTERN_CAT_FILL,       /* Fill pattern (dots, stipple) */

    PB_PATTERN_CAT_COUNT
} pb_pattern_category;

/** Built-in pattern IDs */
typedef enum pb_pattern_id {
    PB_PATTERN_NONE = 0,

    /* Shapes (centered, filled) */
    PB_PATTERN_CIRCLE,         /* Solid circle */
    PB_PATTERN_TRIANGLE,       /* Upward triangle */
    PB_PATTERN_SQUARE,         /* Square/diamond rotated 45 deg */
    PB_PATTERN_DIAMOND,        /* Diamond (tall rhombus) */
    PB_PATTERN_STAR,           /* 5-pointed star */
    PB_PATTERN_CROSS,          /* Plus sign */
    PB_PATTERN_RING,           /* Hollow circle */
    PB_PATTERN_HEART,          /* Heart shape */

    /* Lines */
    PB_PATTERN_LINES_H,        /* Horizontal lines */
    PB_PATTERN_LINES_V,        /* Vertical lines */
    PB_PATTERN_LINES_DIAG_L,   /* Diagonal left (/) */
    PB_PATTERN_LINES_DIAG_R,   /* Diagonal right (\) */

    /* Hatching */
    PB_PATTERN_HATCH_CROSS,    /* + crosshatch */
    PB_PATTERN_HATCH_DIAG,     /* X diagonal crosshatch */

    /* Fills */
    PB_PATTERN_DOTS,           /* Polka dots */
    PB_PATTERN_STIPPLE,        /* Random stipple */

    PB_PATTERN_COUNT
} pb_pattern_id;

/** Get pattern name for display */
const char* pb_pattern_name(pb_pattern_id id);

/** Get pattern category */
pb_pattern_category pb_pattern_get_category(pb_pattern_id id);

/* ============================================================================
 * Pattern Geometry
 * ============================================================================ */

/** 2D point (normalized 0-1 coordinates) */
typedef struct pb_point2d {
    float x, y;
} pb_point2d;

/** Line segment */
typedef struct pb_line2d {
    pb_point2d start, end;
} pb_line2d;

/** Circle primitive */
typedef struct pb_circle2d {
    pb_point2d center;
    float radius;
} pb_circle2d;

/** Polygon primitive (max 16 vertices) */
typedef struct pb_polygon2d {
    pb_point2d vertices[16];
    int count;
    bool filled;
} pb_polygon2d;

/** Pattern definition with geometry */
typedef struct pb_pattern_def {
    pb_pattern_id id;
    pb_pattern_category category;
    const char* name;

    /* Geometry (normalized to unit square, centered at 0.5, 0.5) */
    pb_polygon2d* polygons;    /* Array of polygons */
    int polygon_count;

    pb_line2d* lines;          /* Array of lines */
    int line_count;

    pb_circle2d* circles;      /* Array of circles */
    int circle_count;

    /* Rendering hints */
    float stroke_width;        /* Relative to bubble radius (0.05 = 5%) */
    float scale;               /* Pattern scale (0.6 = 60% of bubble) */
    float rotation;            /* Rotation in degrees */
} pb_pattern_def;

/**
 * Get pattern definition by ID.
 * Returns pointer to static data; do not modify.
 */
const pb_pattern_def* pb_pattern_get(pb_pattern_id id);

/* ============================================================================
 * SVG Generation
 * ============================================================================ */

/**
 * Generate SVG path string for a pattern.
 * Useful for web rendering or vector export.
 *
 * @param id Pattern ID
 * @param size Bubble diameter in pixels
 * @param out Output buffer
 * @param out_size Buffer size
 * @return Number of bytes written (excluding null), or required size if out is NULL
 */
int pb_pattern_to_svg_path(pb_pattern_id id, float size, char* out, int out_size);

/**
 * Generate complete SVG element for a pattern.
 * Includes viewBox, stroke settings, and path data.
 *
 * @param id Pattern ID
 * @param size Bubble diameter
 * @param fill_color Fill color as hex (e.g., "#FFFFFF")
 * @param stroke_color Stroke color as hex
 * @param out Output buffer
 * @param out_size Buffer size
 * @return Number of bytes written
 */
int pb_pattern_to_svg(pb_pattern_id id, float size,
                      const char* fill_color, const char* stroke_color,
                      char* out, int out_size);

/* ============================================================================
 * Pattern Assignment
 * ============================================================================ */

/** Mapping of color index to pattern */
typedef struct pb_pattern_map {
    pb_pattern_id patterns[8];  /* One pattern per color index (0-7) */
} pb_pattern_map;

/**
 * Get default CVD-safe pattern assignments.
 * Uses maximally distinct patterns for 8 colors.
 */
void pb_pattern_get_default_map(pb_pattern_map* out);

/**
 * Check if a pattern map provides sufficient visual distinction.
 * All patterns should be unique.
 */
bool pb_pattern_map_is_valid(const pb_pattern_map* map);

#ifdef __cplusplus
}
#endif

#endif /* PB_PATTERN_H */

/**
 * @file pb_pattern.c
 * @brief Pattern overlay system implementation
 *
 * All geometry is defined in normalized coordinates (0-1),
 * centered at (0.5, 0.5) for easy scaling to any bubble size.
 */

#include "pb/pb_pattern.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Pattern Names
 * ============================================================================ */

static const char* pattern_names[] = {
    "none",
    "circle",
    "triangle",
    "square",
    "diamond",
    "star",
    "cross",
    "ring",
    "heart",
    "lines_h",
    "lines_v",
    "lines_diag_l",
    "lines_diag_r",
    "hatch_cross",
    "hatch_diag",
    "dots",
    "stipple"
};

const char* pb_pattern_name(pb_pattern_id id) {
    /* Note: id is enum, guaranteed >= 0 on all platforms */
    if (id >= PB_PATTERN_COUNT) {
        return "unknown";
    }
    return pattern_names[id];
}

/* ============================================================================
 * Pattern Categories
 * ============================================================================ */

static const pb_pattern_category pattern_categories[] = {
    PB_PATTERN_CAT_NONE,   /* NONE */
    PB_PATTERN_CAT_SHAPE,  /* CIRCLE */
    PB_PATTERN_CAT_SHAPE,  /* TRIANGLE */
    PB_PATTERN_CAT_SHAPE,  /* SQUARE */
    PB_PATTERN_CAT_SHAPE,  /* DIAMOND */
    PB_PATTERN_CAT_SHAPE,  /* STAR */
    PB_PATTERN_CAT_SHAPE,  /* CROSS */
    PB_PATTERN_CAT_SHAPE,  /* RING */
    PB_PATTERN_CAT_SHAPE,  /* HEART */
    PB_PATTERN_CAT_LINE,   /* LINES_H */
    PB_PATTERN_CAT_LINE,   /* LINES_V */
    PB_PATTERN_CAT_LINE,   /* LINES_DIAG_L */
    PB_PATTERN_CAT_LINE,   /* LINES_DIAG_R */
    PB_PATTERN_CAT_HATCH,  /* HATCH_CROSS */
    PB_PATTERN_CAT_HATCH,  /* HATCH_DIAG */
    PB_PATTERN_CAT_FILL,   /* DOTS */
    PB_PATTERN_CAT_FILL    /* STIPPLE */
};

pb_pattern_category pb_pattern_get_category(pb_pattern_id id) {
    /* Note: id is enum, guaranteed >= 0 on all platforms */
    if (id >= PB_PATTERN_COUNT) {
        return PB_PATTERN_CAT_NONE;
    }
    return pattern_categories[id];
}

/* ============================================================================
 * Static Geometry Definitions
 * ============================================================================ */

/* Triangle (upward pointing) */
static pb_polygon2d triangle_poly = {
    .vertices = {
        { 0.5f, 0.2f },   /* Top */
        { 0.2f, 0.8f },   /* Bottom left */
        { 0.8f, 0.8f }    /* Bottom right */
    },
    .count = 3,
    .filled = true
};

/* Square (rotated 45 degrees = diamond orientation) */
static pb_polygon2d square_poly = {
    .vertices = {
        { 0.5f, 0.2f },   /* Top */
        { 0.8f, 0.5f },   /* Right */
        { 0.5f, 0.8f },   /* Bottom */
        { 0.2f, 0.5f }    /* Left */
    },
    .count = 4,
    .filled = true
};

/* Diamond (tall rhombus) */
static pb_polygon2d diamond_poly = {
    .vertices = {
        { 0.5f, 0.15f },  /* Top */
        { 0.7f, 0.5f },   /* Right */
        { 0.5f, 0.85f },  /* Bottom */
        { 0.3f, 0.5f }    /* Left */
    },
    .count = 4,
    .filled = true
};

/* 5-pointed star */
static pb_polygon2d star_poly = {
    .vertices = {
        /* Outer points */
        { 0.5f, 0.15f },   /* Top */
        { 0.41f, 0.40f },
        { 0.15f, 0.40f },  /* Left outer */
        { 0.35f, 0.55f },
        { 0.25f, 0.85f },  /* Bottom left */
        { 0.5f, 0.65f },
        { 0.75f, 0.85f },  /* Bottom right */
        { 0.65f, 0.55f },
        { 0.85f, 0.40f },  /* Right outer */
        { 0.59f, 0.40f }
    },
    .count = 10,
    .filled = true
};

/* Cross (plus sign) */
static pb_polygon2d cross_poly = {
    .vertices = {
        { 0.35f, 0.2f },
        { 0.65f, 0.2f },
        { 0.65f, 0.35f },
        { 0.8f, 0.35f },
        { 0.8f, 0.65f },
        { 0.65f, 0.65f },
        { 0.65f, 0.8f },
        { 0.35f, 0.8f },
        { 0.35f, 0.65f },
        { 0.2f, 0.65f },
        { 0.2f, 0.35f },
        { 0.35f, 0.35f }
    },
    .count = 12,
    .filled = true
};

/* Heart shape (approximated) */
static pb_polygon2d heart_poly = {
    .vertices = {
        { 0.5f, 0.85f },   /* Bottom point */
        { 0.15f, 0.5f },   /* Left curve */
        { 0.15f, 0.35f },
        { 0.25f, 0.2f },
        { 0.5f, 0.35f },   /* Top center dip */
        { 0.75f, 0.2f },
        { 0.85f, 0.35f },
        { 0.85f, 0.5f }    /* Right curve */
    },
    .count = 8,
    .filled = true
};

/* Circles for CIRCLE pattern */
static pb_circle2d circle_shape = {
    .center = { 0.5f, 0.5f },
    .radius = 0.25f
};

/* Ring (hollow circle) - rendered with stroke only */
static pb_circle2d ring_outer = {
    .center = { 0.5f, 0.5f },
    .radius = 0.3f
};

/* Horizontal lines */
static pb_line2d lines_h[] = {
    { { 0.2f, 0.35f }, { 0.8f, 0.35f } },
    { { 0.2f, 0.5f },  { 0.8f, 0.5f } },
    { { 0.2f, 0.65f }, { 0.8f, 0.65f } }
};

/* Vertical lines */
static pb_line2d lines_v[] = {
    { { 0.35f, 0.2f }, { 0.35f, 0.8f } },
    { { 0.5f, 0.2f },  { 0.5f, 0.8f } },
    { { 0.65f, 0.2f }, { 0.65f, 0.8f } }
};

/* Diagonal left (/) */
static pb_line2d lines_diag_l[] = {
    { { 0.25f, 0.75f }, { 0.75f, 0.25f } },
    { { 0.15f, 0.5f },  { 0.5f, 0.15f } },
    { { 0.5f, 0.85f },  { 0.85f, 0.5f } }
};

/* Diagonal right (\) */
static pb_line2d lines_diag_r[] = {
    { { 0.25f, 0.25f }, { 0.75f, 0.75f } },
    { { 0.15f, 0.5f },  { 0.5f, 0.85f } },
    { { 0.5f, 0.15f },  { 0.85f, 0.5f } }
};

/* Cross hatch (+) */
static pb_line2d hatch_cross[] = {
    { { 0.2f, 0.35f }, { 0.8f, 0.35f } },
    { { 0.2f, 0.5f },  { 0.8f, 0.5f } },
    { { 0.2f, 0.65f }, { 0.8f, 0.65f } },
    { { 0.35f, 0.2f }, { 0.35f, 0.8f } },
    { { 0.5f, 0.2f },  { 0.5f, 0.8f } },
    { { 0.65f, 0.2f }, { 0.65f, 0.8f } }
};

/* Diagonal crosshatch (X) */
static pb_line2d hatch_diag[] = {
    { { 0.25f, 0.25f }, { 0.75f, 0.75f } },
    { { 0.25f, 0.75f }, { 0.75f, 0.25f } },
    { { 0.15f, 0.5f },  { 0.5f, 0.15f } },
    { { 0.5f, 0.85f },  { 0.85f, 0.5f } },
    { { 0.15f, 0.5f },  { 0.5f, 0.85f } },
    { { 0.5f, 0.15f },  { 0.85f, 0.5f } }
};

/* Dots pattern */
static pb_circle2d dots_circles[] = {
    { { 0.3f, 0.3f }, 0.06f },
    { { 0.7f, 0.3f }, 0.06f },
    { { 0.5f, 0.5f }, 0.06f },
    { { 0.3f, 0.7f }, 0.06f },
    { { 0.7f, 0.7f }, 0.06f }
};

/* Stipple pattern (random-looking dots) */
static pb_circle2d stipple_circles[] = {
    { { 0.25f, 0.35f }, 0.04f },
    { { 0.45f, 0.25f }, 0.04f },
    { { 0.65f, 0.40f }, 0.04f },
    { { 0.35f, 0.55f }, 0.04f },
    { { 0.55f, 0.60f }, 0.04f },
    { { 0.75f, 0.55f }, 0.04f },
    { { 0.40f, 0.75f }, 0.04f },
    { { 0.60f, 0.80f }, 0.04f }
};

/* ============================================================================
 * Pattern Definitions
 * ============================================================================ */

static pb_pattern_def pattern_defs[PB_PATTERN_COUNT] = {
    /* NONE */
    { PB_PATTERN_NONE, PB_PATTERN_CAT_NONE, "none",
      NULL, 0, NULL, 0, NULL, 0, 0.0f, 0.0f, 0.0f },

    /* CIRCLE */
    { PB_PATTERN_CIRCLE, PB_PATTERN_CAT_SHAPE, "circle",
      NULL, 0, NULL, 0, &circle_shape, 1, 0.05f, 0.6f, 0.0f },

    /* TRIANGLE */
    { PB_PATTERN_TRIANGLE, PB_PATTERN_CAT_SHAPE, "triangle",
      &triangle_poly, 1, NULL, 0, NULL, 0, 0.05f, 0.6f, 0.0f },

    /* SQUARE */
    { PB_PATTERN_SQUARE, PB_PATTERN_CAT_SHAPE, "square",
      &square_poly, 1, NULL, 0, NULL, 0, 0.05f, 0.6f, 0.0f },

    /* DIAMOND */
    { PB_PATTERN_DIAMOND, PB_PATTERN_CAT_SHAPE, "diamond",
      &diamond_poly, 1, NULL, 0, NULL, 0, 0.05f, 0.6f, 0.0f },

    /* STAR */
    { PB_PATTERN_STAR, PB_PATTERN_CAT_SHAPE, "star",
      &star_poly, 1, NULL, 0, NULL, 0, 0.05f, 0.6f, 0.0f },

    /* CROSS */
    { PB_PATTERN_CROSS, PB_PATTERN_CAT_SHAPE, "cross",
      &cross_poly, 1, NULL, 0, NULL, 0, 0.05f, 0.6f, 0.0f },

    /* RING */
    { PB_PATTERN_RING, PB_PATTERN_CAT_SHAPE, "ring",
      NULL, 0, NULL, 0, &ring_outer, 1, 0.08f, 0.6f, 0.0f },

    /* HEART */
    { PB_PATTERN_HEART, PB_PATTERN_CAT_SHAPE, "heart",
      &heart_poly, 1, NULL, 0, NULL, 0, 0.05f, 0.6f, 0.0f },

    /* LINES_H */
    { PB_PATTERN_LINES_H, PB_PATTERN_CAT_LINE, "lines_h",
      NULL, 0, lines_h, 3, NULL, 0, 0.06f, 1.0f, 0.0f },

    /* LINES_V */
    { PB_PATTERN_LINES_V, PB_PATTERN_CAT_LINE, "lines_v",
      NULL, 0, lines_v, 3, NULL, 0, 0.06f, 1.0f, 0.0f },

    /* LINES_DIAG_L */
    { PB_PATTERN_LINES_DIAG_L, PB_PATTERN_CAT_LINE, "lines_diag_l",
      NULL, 0, lines_diag_l, 3, NULL, 0, 0.06f, 1.0f, 0.0f },

    /* LINES_DIAG_R */
    { PB_PATTERN_LINES_DIAG_R, PB_PATTERN_CAT_LINE, "lines_diag_r",
      NULL, 0, lines_diag_r, 3, NULL, 0, 0.06f, 1.0f, 0.0f },

    /* HATCH_CROSS */
    { PB_PATTERN_HATCH_CROSS, PB_PATTERN_CAT_HATCH, "hatch_cross",
      NULL, 0, hatch_cross, 6, NULL, 0, 0.05f, 1.0f, 0.0f },

    /* HATCH_DIAG */
    { PB_PATTERN_HATCH_DIAG, PB_PATTERN_CAT_HATCH, "hatch_diag",
      NULL, 0, hatch_diag, 6, NULL, 0, 0.05f, 1.0f, 0.0f },

    /* DOTS */
    { PB_PATTERN_DOTS, PB_PATTERN_CAT_FILL, "dots",
      NULL, 0, NULL, 0, dots_circles, 5, 0.0f, 1.0f, 0.0f },

    /* STIPPLE */
    { PB_PATTERN_STIPPLE, PB_PATTERN_CAT_FILL, "stipple",
      NULL, 0, NULL, 0, stipple_circles, 8, 0.0f, 1.0f, 0.0f }
};

const pb_pattern_def* pb_pattern_get(pb_pattern_id id) {
    /* Note: id is enum, guaranteed >= 0 on all platforms */
    if (id >= PB_PATTERN_COUNT) {
        return &pattern_defs[0];  /* Return NONE */
    }
    return &pattern_defs[id];
}

/* ============================================================================
 * SVG Generation
 * ============================================================================ */

int pb_pattern_to_svg_path(pb_pattern_id id, float size, char* out, int out_size) {
    const pb_pattern_def* def = pb_pattern_get(id);
    if (!def || def->id == PB_PATTERN_NONE) {
        if (out && out_size > 0) out[0] = '\0';
        return 0;
    }

    char buffer[2048] = {0};
    int pos = 0;

    /* Scale factor */
    float s = size * def->scale;
    float offset = (size - s) * 0.5f;

    /* Generate polygon paths */
    for (int p = 0; p < def->polygon_count; p++) {
        const pb_polygon2d* poly = &def->polygons[p];
        if (poly->count < 3) continue;

        pos += snprintf(buffer + pos, sizeof(buffer) - (size_t)pos, "M%.1f,%.1f ",
                       poly->vertices[0].x * s + offset,
                       poly->vertices[0].y * s + offset);

        for (int i = 1; i < poly->count; i++) {
            pos += snprintf(buffer + pos, sizeof(buffer) - (size_t)pos, "L%.1f,%.1f ",
                           poly->vertices[i].x * s + offset,
                           poly->vertices[i].y * s + offset);
        }
        pos += snprintf(buffer + pos, sizeof(buffer) - (size_t)pos, "Z ");
    }

    /* Generate line paths */
    for (int l = 0; l < def->line_count; l++) {
        const pb_line2d* line = &def->lines[l];
        pos += snprintf(buffer + pos, sizeof(buffer) - (size_t)pos,
                       "M%.1f,%.1f L%.1f,%.1f ",
                       line->start.x * size, line->start.y * size,
                       line->end.x * size, line->end.y * size);
    }

    /* Generate circle paths (approximate with bezier) */
    for (int c = 0; c < def->circle_count; c++) {
        const pb_circle2d* circle = &def->circles[c];
        float cx = circle->center.x * size;
        float cy = circle->center.y * size;
        float r = circle->radius * size * def->scale;
        float k = 0.5522847498f;  /* Bezier approximation constant */

        pos += snprintf(buffer + pos, sizeof(buffer) - (size_t)pos,
            "M%.1f,%.1f "
            "C%.1f,%.1f %.1f,%.1f %.1f,%.1f "
            "C%.1f,%.1f %.1f,%.1f %.1f,%.1f "
            "C%.1f,%.1f %.1f,%.1f %.1f,%.1f "
            "C%.1f,%.1f %.1f,%.1f %.1f,%.1f Z ",
            cx, cy - r,
            cx + r*k, cy - r, cx + r, cy - r*k, cx + r, cy,
            cx + r, cy + r*k, cx + r*k, cy + r, cx, cy + r,
            cx - r*k, cy + r, cx - r, cy + r*k, cx - r, cy,
            cx - r, cy - r*k, cx - r*k, cy - r, cx, cy - r);
    }

    int len = (int)strlen(buffer);
    if (out && out_size > 0) {
        int copy_len = len < out_size - 1 ? len : out_size - 1;
        memcpy(out, buffer, (size_t)copy_len);
        out[copy_len] = '\0';
    }

    return len;
}

int pb_pattern_to_svg(pb_pattern_id id, float size,
                      const char* fill_color, const char* stroke_color,
                      char* out, int out_size) {
    char path[2048];
    pb_pattern_to_svg_path(id, size, path, sizeof(path));

    if (!path[0]) {
        if (out && out_size > 0) out[0] = '\0';
        return 0;
    }

    const pb_pattern_def* def = pb_pattern_get(id);
    float stroke_w = def->stroke_width * size;

    char buffer[4096];
    int len = snprintf(buffer, sizeof(buffer),
        "<svg viewBox=\"0 0 %.0f %.0f\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<path d=\"%s\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"/>"
        "</svg>",
        size, size, path,
        fill_color ? fill_color : "white",
        stroke_color ? stroke_color : "black",
        stroke_w);

    if (out && out_size > 0) {
        int copy_len = len < out_size - 1 ? len : out_size - 1;
        memcpy(out, buffer, (size_t)copy_len);
        out[copy_len] = '\0';
    }

    return len;
}

/* ============================================================================
 * Pattern Assignment
 * ============================================================================ */

void pb_pattern_get_default_map(pb_pattern_map* out) {
    if (!out) return;

    /* Assign maximally distinct patterns for 8 colors */
    out->patterns[0] = PB_PATTERN_CIRCLE;      /* Color 0 */
    out->patterns[1] = PB_PATTERN_TRIANGLE;    /* Color 1 */
    out->patterns[2] = PB_PATTERN_SQUARE;      /* Color 2 */
    out->patterns[3] = PB_PATTERN_STAR;        /* Color 3 */
    out->patterns[4] = PB_PATTERN_CROSS;       /* Color 4 */
    out->patterns[5] = PB_PATTERN_DIAMOND;     /* Color 5 */
    out->patterns[6] = PB_PATTERN_HEART;       /* Color 6 */
    out->patterns[7] = PB_PATTERN_RING;        /* Color 7 */
}

bool pb_pattern_map_is_valid(const pb_pattern_map* map) {
    if (!map) return false;

    /* Check all patterns are unique */
    for (int i = 0; i < 7; i++) {
        for (int j = i + 1; j < 8; j++) {
            if (map->patterns[i] == map->patterns[j] &&
                map->patterns[i] != PB_PATTERN_NONE) {
                return false;
            }
        }
    }

    return true;
}

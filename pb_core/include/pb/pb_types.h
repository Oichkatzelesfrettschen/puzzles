/*
 * pb_types.h - Core type definitions for pb_core
 *
 * Clean-room Puzzle Bobble core library
 * Platform-independent game logic
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_TYPES_H
#define PB_TYPES_H

/* Build configuration must come first for size tier macros */
#include "pb_config.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Build Configuration
 *============================================================================*/

/* Use fixed-point arithmetic for deterministic multiplayer */
#ifndef PB_USE_FIXED_POINT
#define PB_USE_FIXED_POINT 0
#endif

/* Fixed-point format: 16.16 (16 integer bits, 16 fractional bits) */
#if PB_USE_FIXED_POINT
typedef int32_t pb_fixed;
#define PB_FIXED_SHIFT 16
#define PB_FIXED_ONE (1 << PB_FIXED_SHIFT)
#define PB_FIXED_HALF (1 << (PB_FIXED_SHIFT - 1))
#define PB_INT_TO_FIXED(x) ((pb_fixed)((x) << PB_FIXED_SHIFT))
#define PB_FIXED_TO_INT(x) ((int)((x) >> PB_FIXED_SHIFT))
#define PB_FLOAT_TO_FIXED(x) ((pb_fixed)((x) * PB_FIXED_ONE))
#define PB_FIXED_TO_FLOAT(x) ((float)(x) / PB_FIXED_ONE)
#define PB_FIXED_MUL(a,b) ((pb_fixed)(((int64_t)(a) * (b)) >> PB_FIXED_SHIFT))
#define PB_FIXED_DIV(a,b) ((pb_fixed)(((int64_t)(a) << PB_FIXED_SHIFT) / (b)))
#define PB_SCALAR_ABS(x) ((x) < 0 ? -(x) : (x))
#define PB_SCALAR_ROUND(x) PB_FIXED_TO_INT((x) + PB_FIXED_HALF)
/* Trig/sqrt: convert to float, apply, convert back (TODO: pure integer impl) */
#define PB_SCALAR_SIN(x) PB_FLOAT_TO_FIXED(sinf(PB_FIXED_TO_FLOAT(x)))
#define PB_SCALAR_COS(x) PB_FLOAT_TO_FIXED(cosf(PB_FIXED_TO_FLOAT(x)))
#define PB_SCALAR_SQRT(x) PB_FLOAT_TO_FIXED(sqrtf(PB_FIXED_TO_FLOAT(x)))
#define PB_EPSILON 1               /* Smallest positive fixed-point value */
typedef pb_fixed pb_scalar;
#else
typedef float pb_scalar;
#define PB_INT_TO_FIXED(x) ((pb_scalar)(x))
#define PB_FIXED_TO_INT(x) ((int)(x))
#define PB_FLOAT_TO_FIXED(x) ((pb_scalar)(x))
#define PB_FIXED_TO_FLOAT(x) ((float)(x))
#define PB_FIXED_MUL(a,b) ((a) * (b))
#define PB_FIXED_DIV(a,b) ((a) / (b))
#define PB_SCALAR_ABS(x) fabsf(x)
#define PB_SCALAR_ROUND(x) roundf(x)
#define PB_SCALAR_SIN(x) sinf(x)
#define PB_SCALAR_COS(x) cosf(x)
#define PB_SCALAR_SQRT(x) sqrtf(x)
#define PB_EPSILON 1e-6f           /* Small epsilon for float comparisons */
#endif

/*============================================================================
 * Grid Constants
 *============================================================================*/

/* Default grid dimensions (can be overridden per-ruleset) */
#define PB_DEFAULT_COLS_EVEN 8
#define PB_DEFAULT_COLS_ODD  7
#define PB_DEFAULT_ROWS      12

/* Max dimensions (may be defined by pb_config.h for size tiers) */
#ifndef PB_MAX_COLS
#define PB_MAX_COLS          16
#endif
#ifndef PB_MAX_ROWS
#define PB_MAX_ROWS          32
#endif
#ifndef PB_MAX_CELLS
#define PB_MAX_CELLS         (PB_MAX_COLS * PB_MAX_ROWS)
#endif

/* Match threshold (minimum bubbles to pop) */
#define PB_DEFAULT_MATCH_THRESHOLD 3

/* Maximum bubble colors (may be defined by pb_config.h) */
#ifndef PB_MAX_COLORS
#define PB_MAX_COLORS 8
#endif

/* Maximum special bubble types */
#define PB_MAX_SPECIAL_TYPES 16

/*============================================================================
 * Coordinate Types
 *============================================================================*/

/* Offset coordinates (storage format, row-column with staggered rows) */
typedef struct pb_offset {
    int row;
    int col;
} pb_offset;

/* Axial coordinates (algorithmic format, q-r with implicit s) */
typedef struct pb_axial {
    int q;
    int r;
} pb_axial;

/* Cube coordinates (algorithmic format, q-r-s with q+r+s=0 invariant) */
typedef struct pb_cube {
    int q;
    int r;
    int s;
} pb_cube;

/* Fractional cube for rounding operations */
typedef struct pb_cube_frac {
    pb_scalar q;
    pb_scalar r;
    pb_scalar s;
} pb_cube_frac;

/* 2D point (pixel/world coordinates) */
typedef struct pb_point {
    pb_scalar x;
    pb_scalar y;
} pb_point;

/* 2D vector (direction/velocity) */
typedef struct pb_vec2 {
    pb_scalar x;
    pb_scalar y;
} pb_vec2;

/*============================================================================
 * Bubble Types
 *============================================================================*/

/* Base bubble kind */
typedef enum pb_bubble_kind {
    PB_KIND_NONE = 0,       /* Empty cell */
    PB_KIND_COLORED,        /* Standard colored bubble */
    PB_KIND_SPECIAL,        /* Special effect bubble */
    PB_KIND_BLOCKER,        /* Indestructible blocker */
    PB_KIND_WILDCARD,       /* Matches any color */
    PB_KIND_COUNT
} pb_bubble_kind;

/* Bubble flags (bitfield) */
typedef enum pb_bubble_flags {
    PB_FLAG_NONE           = 0,
    PB_FLAG_INDESTRUCTIBLE = (1 << 0),  /* Cannot be popped */
    PB_FLAG_STICKY         = (1 << 1),  /* Attracts nearby bubbles */
    PB_FLAG_GHOST          = (1 << 2),  /* Shot passes through */
    PB_FLAG_FROZEN         = (1 << 3),  /* Cannot fall until thawed */
    PB_FLAG_ANCHOR         = (1 << 4),  /* Counts as attached to ceiling */
    PB_FLAG_ACTIVATED      = (1 << 5)   /* Effect already triggered */
} pb_bubble_flags;

/* Special bubble types (effects) */
typedef enum pb_special_type {
    PB_SPECIAL_NONE = 0,
    PB_SPECIAL_BOMB,        /* Destroys adjacent cells (radius 1) */
    PB_SPECIAL_LIGHTNING,   /* Destroys entire row */
    PB_SPECIAL_STAR,        /* Destroys all of hit color */
    PB_SPECIAL_MAGNETIC,    /* Attracts projectile */
    PB_SPECIAL_RAINBOW,     /* Matches any color */
    PB_SPECIAL_SHIFTER,     /* Cycles color each turn */
    PB_SPECIAL_PORTAL,      /* Remaps shot exit point */
    PB_SPECIAL_ICE,         /* Prevents falling until thawed */
    PB_SPECIAL_SPLIT,       /* Divides into two colors on hit */
    PB_SPECIAL_VIRUS,       /* Spreads color along adjacency */
    PB_SPECIAL_KEY,         /* Required to unlock blockers */
    PB_SPECIAL_LOCK,        /* Requires key to be cleared */
    PB_SPECIAL_COUNT
} pb_special_type;

/* Bubble structure */
typedef struct pb_bubble {
    pb_bubble_kind kind;
    uint8_t color_id;           /* Color index (0 = PB_MAX_COLORS-1) */
    uint8_t flags;              /* pb_bubble_flags bitfield */
    pb_special_type special;    /* Special type (if kind == PB_KIND_SPECIAL) */
    union {
        uint8_t timer;          /* For timed effects */
        uint8_t magnet_strength;
        uint8_t bomb_radius;
        uint8_t portal_target;  /* Target cell ID */
    } payload;
} pb_bubble;

/*============================================================================
 * Board State
 *============================================================================*/

/* Board structure */
typedef struct pb_board {
    pb_bubble cells[PB_MAX_ROWS][PB_MAX_COLS];
    int cols_even;              /* Columns in even rows */
    int cols_odd;               /* Columns in odd rows */
    int rows;                   /* Total rows */
    int ceiling_row;            /* Current ceiling position (for pressure) */
} pb_board;

/*============================================================================
 * Shot State
 *============================================================================*/

/* Shot phase */
typedef enum pb_shot_phase {
    PB_SHOT_IDLE = 0,
    PB_SHOT_AIMING,
    PB_SHOT_MOVING,
    PB_SHOT_COLLIDED,
    PB_SHOT_SNAPPING
} pb_shot_phase;

/* Shot structure */
typedef struct pb_shot {
    pb_shot_phase phase;
    pb_bubble bubble;           /* The bubble being shot */
    pb_point pos;               /* Current position */
    pb_vec2 velocity;           /* Current velocity */
    int bounces;                /* Number of wall bounces */
    int max_bounces;            /* Maximum allowed bounces */
} pb_shot;

/*============================================================================
 * Ruleset
 *============================================================================*/

/* Game mode type */
typedef enum pb_mode_type {
    PB_MODE_PUZZLE = 0,         /* Fixed board, perfect information */
    PB_MODE_ARCADE,             /* Campaign with ceiling pressure */
    PB_MODE_SURVIVAL,           /* Add rows after N shots */
    PB_MODE_TIME_ATTACK,        /* Time-limited scoring */
    PB_MODE_VERSUS,             /* Competitive with garbage */
    PB_MODE_COOP,               /* Shared board, multiple inputs */
    PB_MODE_ZEN,                /* No pressure, practice mode */
    PB_MODE_COUNT
} pb_mode_type;

/* Lose condition type */
typedef enum pb_lose_condition {
    PB_LOSE_OVERFLOW = 0,       /* Bubble crosses bottom line */
    PB_LOSE_TIMEOUT,            /* Time runs out */
    PB_LOSE_SHOTS_EXHAUSTED     /* No more shots */
} pb_lose_condition;

/* Ruleset structure */
typedef struct pb_ruleset {
    pb_mode_type mode;
    int match_threshold;        /* Minimum bubbles to pop (usually 3) */
    int cols_even;
    int cols_odd;
    int rows;
    int max_bounces;
    int shots_per_row_insert;   /* Shots before new row (survival mode) */
    int initial_rows;           /* Starting bubble rows */
    pb_lose_condition lose_on;
    bool allow_color_switch;    /* Can swap current/next bubble */
    bool restrict_colors_to_board; /* Only generate colors on board */
    uint8_t allowed_colors;     /* Bitmask of allowed color indices */
    uint8_t allowed_specials;   /* Bitmask of allowed special types */
} pb_ruleset;

/*============================================================================
 * RNG State (for deterministic replay)
 *============================================================================*/

/* xoshiro128** state (fast, high-quality PRNG) */
typedef struct pb_rng {
    uint32_t state[4];
} pb_rng;

/*============================================================================
 * Event Types (for replay/networking)
 *============================================================================*/

typedef enum pb_event_type {
    PB_EVENT_NONE = 0,
    PB_EVENT_ROTATE_LEFT,
    PB_EVENT_ROTATE_RIGHT,
    PB_EVENT_FIRE,
    PB_EVENT_SWITCH_BUBBLE,
    PB_EVENT_PAUSE,
    PB_EVENT_UNPAUSE,
    /* Internal events (not from input) */
    PB_EVENT_BUBBLE_PLACED,
    PB_EVENT_BUBBLES_POPPED,
    PB_EVENT_BUBBLES_DROPPED,
    PB_EVENT_ROW_INSERTED,
    PB_EVENT_GARBAGE_RECEIVED,
    PB_EVENT_GAME_OVER,
    PB_EVENT_LEVEL_CLEAR,
    PB_EVENT_COUNT
} pb_event_type;

/*
 * OPTIMIZED pb_event: Uses compact cell indices instead of full pb_offset.
 *
 * Size comparison (full tier, 512 cells):
 *   OLD: 4108 bytes per event (cells[512] * sizeof(pb_offset))
 *   NEW:   60 bytes per event (cells[48] * sizeof(pb_cell_index))
 *
 * This is a 68x size reduction with no gameplay impact.
 * Use PB_CELL_TO_INDEX/PB_INDEX_TO_ROW/PB_INDEX_TO_COL for conversion.
 */
typedef struct pb_event {
    pb_event_type type;
    uint32_t frame;             /* Frame number when event occurred */
    union {
        struct { pb_scalar angle; } fire;
        struct { pb_cell_index cell; pb_bubble bubble; } placed;
        struct { uint8_t count; pb_cell_index cells[PB_EVENT_CELL_MAX]; } popped;
        struct { uint8_t count; pb_cell_index cells[PB_EVENT_CELL_MAX]; } dropped;
        struct { uint8_t player_id; uint8_t count; } garbage;
    } data;
} pb_event;

/*============================================================================
 * Game State
 *============================================================================*/

typedef enum pb_game_phase {
    PB_PHASE_INIT = 0,
    PB_PHASE_READY,
    PB_PHASE_PLAYING,
    PB_PHASE_ANIMATING,         /* Match/drop animation in progress */
    PB_PHASE_PAUSED,
    PB_PHASE_WON,
    PB_PHASE_LOST
} pb_game_phase;

typedef struct pb_game_state {
    pb_game_phase phase;
    pb_board board;
    pb_ruleset ruleset;
    pb_shot shot;
    pb_rng rng;

    pb_bubble current_bubble;   /* Next bubble to fire */
    pb_bubble preview_bubble;   /* Preview of next-next bubble */
    pb_scalar cannon_angle;     /* Current aim angle (radians) */

    uint32_t frame;             /* Current frame number */
    uint32_t score;
    int shots_fired;
    int shots_until_row;        /* Countdown to next row insertion */
    int combo_multiplier;

    /* Event log for replay */
    pb_event events[256];
    int event_count;

    /* Checksum for sync verification */
    uint32_t checksum;
} pb_game_state;

/*============================================================================
 * Color/Visual Types (for pb_data layer)
 *============================================================================*/

/* sRGB color (8-bit per channel) */
typedef struct pb_color_srgb8 {
    uint8_t r, g, b, a;
} pb_color_srgb8;

/* Linear RGB color (floating point) */
typedef struct pb_color_linear {
    float r, g, b, a;
} pb_color_linear;

/* Oklab color */
typedef struct pb_color_oklab {
    float L, a, b;
} pb_color_oklab;

/* Oklch color (cylindrical Oklab) */
typedef struct pb_color_oklch {
    float L, C, h;
} pb_color_oklch;

/* Visual identity for a bubble type */
typedef struct pb_bubble_visual {
    pb_color_srgb8 srgb;        /* Primary color */
    pb_color_srgb8 outline;     /* Outline/border color */
    int pattern_id;             /* Pattern atlas index (for CVD) */
    float cvd_safe_rank;        /* Min distance under CVD simulations */
} pb_bubble_visual;

/*============================================================================
 * API Result Codes
 *============================================================================*/

typedef enum pb_result {
    PB_OK = 0,
    PB_ERR_INVALID_ARG,
    PB_ERR_OUT_OF_BOUNDS,
    PB_ERR_INVALID_STATE,
    PB_ERR_NO_MEMORY,
    PB_ERR_NOT_IMPLEMENTED
} pb_result;

#ifdef __cplusplus
}
#endif

#endif /* PB_TYPES_H */

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

/* Freestanding-compatible math functions */
#include "pb_freestanding.h"

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

/*============================================================================
 * Fixed-Point Arithmetic
 *
 * Supported formats (see pb_config.h for selection):
 *   Q16.16 - 32-bit signed: range ±32768, precision 1/65536
 *   Q12.12 - 16-bit signed: range ±8, precision 1/4096 (HP48-style)
 *   Q8.8   - 16-bit signed: range ±128, precision 1/256
 *   Q8.1   - 8-bit signed:  range ±64, precision 0.5
 *   Q0.8   - 8-bit unsigned: range 0-0.996, precision 1/256
 *============================================================================*/
#if PB_USE_FIXED_POINT

#if PB_FIXED_FORMAT == PB_FP_FORMAT_Q0_8
/*---------------------------------------------------------------------------
 * Q0.8: 8-bit unsigned, all fractional (for normalized 0-1 values)
 * Range: 0.0 to 0.99609375 (255/256)
 * Precision: 1/256 = 0.00390625
 * Use case: percentages, interpolation factors, alpha values
 *---------------------------------------------------------------------------*/
typedef uint8_t pb_fixed;
#define PB_FIXED_SHIFT 8
#define PB_FIXED_ONE   255                          /* Closest to 1.0 */
#define PB_FIXED_HALF  128                          /* 0.5 */
#define PB_INT_TO_FIXED(x) ((pb_fixed)(((x) > 0) ? PB_FIXED_ONE : 0))
#define PB_FIXED_TO_INT(x) ((int)((x) >= 128 ? 1 : 0))
#define PB_FLOAT_TO_FIXED(x) ((pb_fixed)((x) * 256.0f))
#define PB_FIXED_TO_FLOAT(x) ((float)(x) / 256.0f)
#define PB_FIXED_MUL(a,b) ((pb_fixed)(((uint16_t)(a) * (b)) >> 8))
#define PB_FIXED_DIV(a,b) ((pb_fixed)(((uint16_t)(a) << 8) / ((b) ? (b) : 1)))
#define PB_FIXED_IS_UNSIGNED 1

#elif PB_FIXED_FORMAT == PB_FP_FORMAT_Q8_1
/*---------------------------------------------------------------------------
 * Q8.1: 8-bit signed, 7 integer + 1 fractional bit
 * Range: -64.0 to +63.5
 * Precision: 0.5
 * Use case: coarse positioning, AI weights, extreme memory constraints
 *---------------------------------------------------------------------------*/
typedef int8_t pb_fixed;
#define PB_FIXED_SHIFT 1
#define PB_FIXED_ONE   (1 << PB_FIXED_SHIFT)        /* 2 */
#define PB_FIXED_HALF  1                            /* 0.5 */
#define PB_INT_TO_FIXED(x) ((pb_fixed)((x) << PB_FIXED_SHIFT))
#define PB_FIXED_TO_INT(x) ((int)((x) >> PB_FIXED_SHIFT))
#define PB_FLOAT_TO_FIXED(x) ((pb_fixed)((x) * PB_FIXED_ONE))
#define PB_FIXED_TO_FLOAT(x) ((float)(x) / PB_FIXED_ONE)
#define PB_FIXED_MUL(a,b) ((pb_fixed)(((int16_t)(a) * (b)) >> PB_FIXED_SHIFT))
#define PB_FIXED_DIV(a,b) ((pb_fixed)(((b) != 0) ? (((int16_t)(a) << PB_FIXED_SHIFT) / (b)) : 0))

#elif PB_FIXED_FORMAT == PB_FP_FORMAT_Q8_8
/*---------------------------------------------------------------------------
 * Q8.8: 16-bit signed, 8 integer + 8 fractional bits
 * Range: -128.0 to +127.99609375
 * Precision: 1/256 = 0.00390625
 * Use case: small playfields, velocity, angles on 8-bit systems
 *---------------------------------------------------------------------------*/
typedef int16_t pb_fixed;
#define PB_FIXED_SHIFT 8
#define PB_FIXED_ONE   (1 << PB_FIXED_SHIFT)        /* 256 */
#define PB_FIXED_HALF  (1 << (PB_FIXED_SHIFT - 1))  /* 128 */
#define PB_INT_TO_FIXED(x) ((pb_fixed)((x) << PB_FIXED_SHIFT))
#define PB_FIXED_TO_INT(x) ((int)((x) >> PB_FIXED_SHIFT))
#define PB_FLOAT_TO_FIXED(x) ((pb_fixed)((x) * PB_FIXED_ONE))
#define PB_FIXED_TO_FLOAT(x) ((float)(x) / PB_FIXED_ONE)
/* Q8.8 multiply fits in 32-bit intermediate */
#define PB_FIXED_MUL(a,b) ((pb_fixed)(((int32_t)(a) * (b)) >> PB_FIXED_SHIFT))
#define PB_FIXED_DIV(a,b) ((pb_fixed)(((b) != 0) ? (((int32_t)(a) << PB_FIXED_SHIFT) / (b)) : 0))

#elif PB_FIXED_FORMAT == PB_FP_FORMAT_Q12_12
/*---------------------------------------------------------------------------
 * Q12.12: 16-bit signed, 4 integer + 12 fractional bits (HP48-style)
 * Range: -8.0 to +7.999755859375
 * Precision: 1/4096 = 0.000244140625
 * Use case: HP48-style playfield (56x88), 16-bit embedded systems
 *---------------------------------------------------------------------------*/
typedef int16_t pb_fixed;
#define PB_FIXED_SHIFT 12
#define PB_FIXED_ONE   (1 << PB_FIXED_SHIFT)        /* 4096 */
#define PB_FIXED_HALF  (1 << (PB_FIXED_SHIFT - 1))  /* 2048 */
#define PB_INT_TO_FIXED(x) ((pb_fixed)((x) << PB_FIXED_SHIFT))
#define PB_FIXED_TO_INT(x) ((int)((x) >> PB_FIXED_SHIFT))
#define PB_FLOAT_TO_FIXED(x) ((pb_fixed)((x) * PB_FIXED_ONE))
#define PB_FIXED_TO_FLOAT(x) ((float)(x) / PB_FIXED_ONE)
/* Q12.12 multiply fits in 32-bit intermediate */
#define PB_FIXED_MUL(a,b) ((pb_fixed)(((int32_t)(a) * (b)) >> PB_FIXED_SHIFT))
#define PB_FIXED_DIV(a,b) ((pb_fixed)(((b) != 0) ? (((int32_t)(a) << PB_FIXED_SHIFT) / (b)) : 0))

#else /* PB_FIXED_FORMAT == PB_FP_FORMAT_Q16_16 (default) */
/*---------------------------------------------------------------------------
 * Q16.16: 32-bit signed, 16 integer + 16 fractional bits
 * Range: -32768.0 to +32767.999984741211
 * Precision: 1/65536 = 0.0000152587890625
 * Use case: desktop/modern, full precision, large playfields
 *---------------------------------------------------------------------------*/
typedef int32_t pb_fixed;
#define PB_FIXED_SHIFT 16
#define PB_FIXED_ONE   (1 << PB_FIXED_SHIFT)        /* 65536 */
#define PB_FIXED_HALF  (1 << (PB_FIXED_SHIFT - 1))  /* 32768 */
#define PB_INT_TO_FIXED(x) ((pb_fixed)((x) << PB_FIXED_SHIFT))
#define PB_FIXED_TO_INT(x) ((int)((x) >> PB_FIXED_SHIFT))
#define PB_FLOAT_TO_FIXED(x) ((pb_fixed)((x) * PB_FIXED_ONE))
#define PB_FIXED_TO_FLOAT(x) ((float)(x) / PB_FIXED_ONE)
/* Q16.16 multiply requires 64-bit intermediate */
#define PB_FIXED_MUL(a,b) ((pb_fixed)(((int64_t)(a) * (b)) >> PB_FIXED_SHIFT))
#define PB_FIXED_DIV(a,b) ((pb_fixed)(((b) != 0) ? (((int64_t)(a) << PB_FIXED_SHIFT) / (b)) : 0))

#endif /* PB_FIXED_FORMAT */

/* Common fixed-point operations */
#ifndef PB_FIXED_IS_UNSIGNED
#define PB_SCALAR_ABS(x) ((x) < 0 ? -(x) : (x))
#else
#define PB_SCALAR_ABS(x) (x)  /* Unsigned types are always positive */
#endif
#define PB_SCALAR_ROUND(x) PB_FIXED_TO_INT((x) + PB_FIXED_HALF)

/* Trig functions: pure integer when freestanding, otherwise convert through float */
#if PB_PLATFORM_FREESTANDING
#define PB_SCALAR_SIN(x) pb_fp_sin(x)
#define PB_SCALAR_COS(x) pb_fp_cos(x)
#define PB_SCALAR_SQRT(x) pb_fp_sqrt(x)
#else
#define PB_SCALAR_SIN(x) PB_FLOAT_TO_FIXED(pb_sinf(PB_FIXED_TO_FLOAT(x)))
#define PB_SCALAR_COS(x) PB_FLOAT_TO_FIXED(pb_cosf(PB_FIXED_TO_FLOAT(x)))
#define PB_SCALAR_SQRT(x) PB_FLOAT_TO_FIXED(pb_sqrtf(PB_FIXED_TO_FLOAT(x)))
#endif

#define PB_EPSILON 1               /* Smallest positive fixed-point value */
typedef pb_fixed pb_scalar;

#else /* !PB_USE_FIXED_POINT */
/*---------------------------------------------------------------------------
 * Floating-point mode (default for desktop builds)
 *---------------------------------------------------------------------------*/
typedef float pb_scalar;
#define PB_FIXED_SHIFT 0
#define PB_INT_TO_FIXED(x) ((pb_scalar)(x))
#define PB_FIXED_TO_INT(x) ((int)(x))
#define PB_FLOAT_TO_FIXED(x) ((pb_scalar)(x))
#define PB_FIXED_TO_FLOAT(x) ((float)(x))
#define PB_FIXED_MUL(a,b) ((a) * (b))
#define PB_FIXED_DIV(a,b) ((b) != 0.0f ? ((a) / (b)) : 0.0f)
#define PB_SCALAR_ABS(x) pb_fabsf(x)
#define PB_SCALAR_ROUND(x) pb_roundf(x)
#define PB_SCALAR_SIN(x) pb_sinf(x)
#define PB_SCALAR_COS(x) pb_cosf(x)
#define PB_SCALAR_SQRT(x) pb_sqrtf(x)
#define PB_EPSILON 1e-6f           /* Small epsilon for float comparisons */
#endif /* PB_USE_FIXED_POINT */

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
 * Direction Index (HP48-inspired)
 *
 * Uses integer index 0-78 instead of radians for cannon direction.
 * Index N maps to angle (12 + N*2) degrees = (0.209 + N*0.035) radians.
 * Wall reflection is trivial: new_index = 78 - old_index
 *============================================================================*/

/* Direction index type (1 byte, sufficient for 0-78 range) */
typedef uint8_t pb_direction_index;

/* Direction constants */
#define PB_DIR_MIN       0      /* 12 degrees */
#define PB_DIR_MAX       78     /* 168 degrees */
#define PB_DIR_VERTICAL  39     /* 90 degrees (straight up) */
#define PB_DIR_COUNT     79     /* Number of valid directions */

/* Angle step in degrees and radians */
#define PB_DIR_STEP_DEG  2
#define PB_DIR_BASE_DEG  12

/* Wall reflection: simple subtraction */
#define PB_DIR_REFLECT(idx) (PB_DIR_MAX - (idx))

/* Convert between direction index and radians */
#if PB_USE_DIRECTION_INDEX
/* Index to radians: (12 + index*2) * PI/180 */
#define PB_DIR_TO_RADIANS(idx) \
    PB_FLOAT_TO_FIXED(((PB_DIR_BASE_DEG + (idx) * PB_DIR_STEP_DEG) * 3.14159265f) / 180.0f)

/* Radians to index: floor((degrees - 12) / 2), clamped to 0-78 */
PB_INLINE pb_direction_index pb_radians_to_dir(pb_scalar radians) {
    float deg = PB_FIXED_TO_FLOAT(radians) * 180.0f / 3.14159265f;
    int idx = (int)((deg - PB_DIR_BASE_DEG) / PB_DIR_STEP_DEG);
    if (idx < PB_DIR_MIN) idx = PB_DIR_MIN;
    if (idx > PB_DIR_MAX) idx = PB_DIR_MAX;
    return (pb_direction_index)idx;
}
#endif /* PB_USE_DIRECTION_INDEX */

/*============================================================================
 * Screen Effects (HP48-inspired)
 *
 * Visual warning effects before row insertion.
 * HP48 uses screen margin oscillation; we provide abstract effect types.
 *============================================================================*/

#if PB_USE_SCREEN_EFFECTS
typedef enum pb_screen_effect {
    PB_EFFECT_NONE = 0,
    PB_EFFECT_SHAKE_SLOW,   /* 2 shots remaining before row insert */
    PB_EFFECT_SHAKE_FAST,   /* 1 shot remaining before row insert */
    PB_EFFECT_FLASH,        /* Warning flash */
    PB_EFFECT_PULSE         /* Pulsing border */
} pb_screen_effect;

/* Screen effect state for renderers */
typedef struct pb_effect_state {
    pb_screen_effect active;
    pb_vec2 offset;         /* Screen shake offset */
    uint8_t intensity;      /* Effect intensity 0-255 */
    uint32_t start_frame;   /* Frame when effect started */
} pb_effect_state;
#endif /* PB_USE_SCREEN_EFFECTS */

/*============================================================================
 * Hurry-Up State (HP48-inspired countdown)
 *============================================================================*/

typedef struct pb_hurry_state {
    bool active;                /* Hurry-up mode active */
    int seconds_remaining;      /* For countdown display (0-10) */
    pb_point indicator_pos;     /* Where to show countdown sprite */
    uint8_t countdown_sprite;   /* Sprite index for countdown (0-5) */
} pb_hurry_state;

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
    PB_FLAG_ACTIVATED      = (1 << 5),  /* Effect already triggered */
    /* HP48-inspired temporary flags for in-place flood-fill */
    PB_FLAG_MATCH_MARKED   = (1 << 6),  /* Temp: marked for current match */
    PB_FLAG_ORPHAN_CHECK   = (1 << 7)   /* Temp: attached check in progress */
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

/*
 * Compact Bubble (HP48-inspired 2-byte encoding)
 *
 * Used when PB_USE_COMPACT_BUBBLES is enabled.
 * Reduces memory from ~16 bytes to 2 bytes per bubble.
 *
 * Format:
 *   Byte 0: [kind:4][color:4]
 *   Byte 1: [flags:4][special:4]
 */
#if PB_USE_COMPACT_BUBBLES
typedef struct pb_bubble_compact {
    uint8_t kind_color;     /* High nibble: kind, Low nibble: color */
    uint8_t flags_special;  /* High nibble: flags, Low nibble: special */
} pb_bubble_compact;

/* Compact bubble accessors */
#define PB_COMPACT_GET_KIND(b)    (((b).kind_color >> 4) & 0x0F)
#define PB_COMPACT_GET_COLOR(b)   ((b).kind_color & 0x0F)
#define PB_COMPACT_GET_FLAGS(b)   ((b).flags_special >> 4)
#define PB_COMPACT_GET_SPECIAL(b) ((b).flags_special & 0x0F)

#define PB_COMPACT_SET_KIND(b, k)    ((b).kind_color = ((b).kind_color & 0x0F) | (((k) & 0x0F) << 4))
#define PB_COMPACT_SET_COLOR(b, c)   ((b).kind_color = ((b).kind_color & 0xF0) | ((c) & 0x0F))
#define PB_COMPACT_SET_FLAGS(b, f)   ((b).flags_special = ((b).flags_special & 0x0F) | (((f) & 0x0F) << 4))
#define PB_COMPACT_SET_SPECIAL(b, s) ((b).flags_special = ((b).flags_special & 0xF0) | ((s) & 0x0F))

/* Initialize compact bubble */
#define PB_COMPACT_INIT(kind, color, flags, special) \
    { (((kind) & 0x0F) << 4) | ((color) & 0x0F), \
      (((flags) & 0x0F) << 4) | ((special) & 0x0F) }

/* Convert between compact and full bubble */
PB_INLINE pb_bubble pb_bubble_from_compact(pb_bubble_compact c) {
    pb_bubble b;
    b.kind = (pb_bubble_kind)PB_COMPACT_GET_KIND(c);
    b.color_id = PB_COMPACT_GET_COLOR(c);
    b.flags = PB_COMPACT_GET_FLAGS(c);
    b.special = (pb_special_type)PB_COMPACT_GET_SPECIAL(c);
    b.payload.timer = 0;
    return b;
}

PB_INLINE pb_bubble_compact pb_bubble_to_compact(pb_bubble b) {
    pb_bubble_compact c;
    c.kind_color = ((b.kind & 0x0F) << 4) | (b.color_id & 0x0F);
    c.flags_special = ((b.flags & 0x0F) << 4) | (b.special & 0x0F);
    return c;
}
#endif /* PB_USE_COMPACT_BUBBLES */

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
    uint32_t last_bounce_frame; /* Frame of last bounce (for debounce) */
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
    pb_scalar bubble_radius;    /* Bubble display/collision radius */
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
 * Scoring Constants (from implementation analysis synthesis)
 *============================================================================*/

/* Base points per matched bubble */
#define PB_SCORE_BASE_MATCH      10

/* Base points per orphan bubble (higher value) */
#define PB_SCORE_BASE_ORPHAN     20

/* Quantifier divisor for score tiers (km-bubble-shooter style) */
#define PB_SCORE_QUANTIFIER_DIV  3

/* Maximum chain multiplier cap */
#define PB_MAX_CHAIN_MULTIPLIER  10

/*============================================================================
 * Timing Constants (from implementation analysis synthesis)
 *============================================================================*/

/* Auto-fire warning time in frames (at 60fps = 5 seconds) */
#define PB_HURRY_WARNING_FRAMES  300

/* Auto-fire trigger time in frames (at 60fps = 8 seconds) */
#define PB_HURRY_AUTOFIRE_FRAMES 480

/* Wall bounce debounce time in frames (50ms at 60fps = 3 frames) */
#define PB_BOUNCE_DEBOUNCE_FRAMES 3

/*============================================================================
 * Collision Constants (from implementation analysis synthesis)
 *============================================================================*/

/* Collision detection threshold as fraction of bubble diameter (0.85 recommended) */
#define PB_COLLISION_THRESHOLD_FACTOR PB_FLOAT_TO_FIXED(0.85f)

/*============================================================================
 * Game State
 *============================================================================*/

typedef enum pb_game_phase {
    PB_PHASE_INIT = 0,
    PB_PHASE_READY,
    PB_PHASE_PLAYING,
    PB_PHASE_ANIMATING,         /* Match/drop animation in progress */
    PB_PHASE_PAUSED,
    PB_PHASE_HURRY,             /* Hurry-up warning active */
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

    /* Auto-fire / hurry-up system */
    uint32_t last_shot_frame;   /* Frame when last shot was fired */
    bool hurry_active;          /* Hurry-up warning is active */

    /* Score quantifier (for km-bubble-shooter style scoring) */
    int score_quantifier;       /* Increments with each bubble popped */

    /* Garbage exchange (for versus mode) */
    int pending_garbage_send;   /* Garbage to send to opponent */
    int pending_garbage_recv;   /* Garbage received from opponent */

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
 * Pixel-Mask Collision (HP48-inspired)
 *
 * Alternative collision mode using sprite pixel overlap detection.
 * More accurate for pixel-art renderers than circle-based collision.
 *
 * HP48 uses 16x16 pixel masks; we support configurable sizes.
 * Collision: (mask_a & mask_b) != 0 at overlap region.
 *============================================================================*/

#if PB_USE_PIXEL_COLLISION

/* Pixel mask dimensions (must be power of 2) */
#ifndef PB_PIXEL_MASK_SIZE
    #if defined(PB_SIZE_MICRO)
        #define PB_PIXEL_MASK_SIZE 8    /* 8x8 for 8-bit systems */
    #elif defined(PB_SIZE_MINI)
        #define PB_PIXEL_MASK_SIZE 12   /* 12x12 for 16-bit systems */
    #else
        #define PB_PIXEL_MASK_SIZE 16   /* 16x16 for 32-bit+ */
    #endif
#endif

/* Row storage type (sized for mask width) */
#if PB_PIXEL_MASK_SIZE <= 8
    typedef uint8_t pb_mask_row;
#elif PB_PIXEL_MASK_SIZE <= 16
    typedef uint16_t pb_mask_row;
#else
    typedef uint32_t pb_mask_row;
#endif

/* Pixel mask for bubble sprite */
typedef struct pb_pixel_mask {
    pb_mask_row rows[PB_PIXEL_MASK_SIZE];
} pb_pixel_mask;

/*
 * Pre-computed circular bubble mask.
 * Generated at compile time or runtime based on bubble radius.
 */
extern const pb_pixel_mask pb_bubble_mask;

/**
 * Check if two pixel masks overlap at given offset.
 *
 * @param mask_a First mask (typically moving bubble)
 * @param mask_b Second mask (typically stationary bubble)
 * @param dx     X offset of mask_b relative to mask_a (pixels)
 * @param dy     Y offset of mask_b relative to mask_a (pixels)
 * @return       true if any pixels overlap
 */
PB_INLINE bool pb_pixel_masks_overlap(const pb_pixel_mask* mask_a,
                                       const pb_pixel_mask* mask_b,
                                       int dx, int dy) {
    int y;
    /* Check if masks are completely separate */
    if (dx >= PB_PIXEL_MASK_SIZE || dx <= -PB_PIXEL_MASK_SIZE ||
        dy >= PB_PIXEL_MASK_SIZE || dy <= -PB_PIXEL_MASK_SIZE) {
        return false;
    }

    /* Check each overlapping row */
    for (y = 0; y < PB_PIXEL_MASK_SIZE; y++) {
        int row_a = y;
        int row_b = y + dy;
        pb_mask_row shifted_b;

        if (row_b < 0 || row_b >= PB_PIXEL_MASK_SIZE) continue;

        /* Shift mask_b row by dx */
        if (dx >= 0) {
            shifted_b = mask_b->rows[row_b] >> dx;
        } else {
            shifted_b = mask_b->rows[row_b] << (-dx);
        }

        /* Check for overlap */
        if (mask_a->rows[row_a] & shifted_b) {
            return true;
        }
    }
    return false;
}

/**
 * Check collision between bubble at position and pixel mask.
 *
 * @param pos_a    Center of first bubble (pixel coordinates)
 * @param pos_b    Center of second bubble (pixel coordinates)
 * @param mask     Pixel mask to use (same for both, typically pb_bubble_mask)
 * @return         true if bubbles collide
 */
PB_INLINE bool pb_pixel_collision(pb_point pos_a, pb_point pos_b,
                                   const pb_pixel_mask* mask) {
    int dx = PB_FIXED_TO_INT(pos_b.x - pos_a.x);
    int dy = PB_FIXED_TO_INT(pos_b.y - pos_a.y);
    return pb_pixel_masks_overlap(mask, mask, dx, dy);
}

#endif /* PB_USE_PIXEL_COLLISION */

/*============================================================================
 * In-Place Flood-Fill Helpers (HP48-inspired)
 *
 * Uses bubble flags for marking during match/orphan detection.
 * Saves PB_MAX_CELLS bytes by avoiding separate visited array.
 *============================================================================*/

#if PB_USE_INPLACE_MARKING

/* Mark bubble for current match detection */
#define PB_MARK_FOR_MATCH(b)     ((b)->flags |= PB_FLAG_MATCH_MARKED)
#define PB_UNMARK_MATCH(b)       ((b)->flags &= ~PB_FLAG_MATCH_MARKED)
#define PB_IS_MATCH_MARKED(b)    (((b)->flags & PB_FLAG_MATCH_MARKED) != 0)

/* Mark bubble for orphan detection */
#define PB_MARK_FOR_ORPHAN(b)    ((b)->flags |= PB_FLAG_ORPHAN_CHECK)
#define PB_UNMARK_ORPHAN(b)      ((b)->flags &= ~PB_FLAG_ORPHAN_CHECK)
#define PB_IS_ORPHAN_MARKED(b)   (((b)->flags & PB_FLAG_ORPHAN_CHECK) != 0)

/* Clear all temporary marks from a bubble */
#define PB_CLEAR_TEMP_MARKS(b)   ((b)->flags &= ~(PB_FLAG_MATCH_MARKED | PB_FLAG_ORPHAN_CHECK))

#endif /* PB_USE_INPLACE_MARKING */

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

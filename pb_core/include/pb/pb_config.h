/*
 * pb_config.h - Build configuration for pb_core
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Size tiers for different target platforms:
 *   PB_SIZE_FULL   - Desktop/modern (32x16 board, 256 events, ~1MB state)
 *   PB_SIZE_MEDIUM - Embedded 32-bit (16x12 board, 64 events, ~64KB state)
 *   PB_SIZE_MINI   - 16-bit systems (12x8 board, 16 events, ~16KB state)
 *   PB_SIZE_MICRO  - 8-bit systems (8x8 board, 8 events, ~4KB state)
 *
 * C Standard tiers:
 *   PB_C17  - C17 (default, full features)
 *   PB_C11  - C11 fallback
 *   PB_C99  - C99 fallback (no _Static_assert)
 *   PB_C89  - C89/ANSI C (no inline, no bool, no designated initializers)
 */

#ifndef PB_CONFIG_H
#define PB_CONFIG_H

/* Standard integer types (needed for pb_cell_index) */
#include <stdint.h>

/*============================================================================
 * Size Tier Selection (define ONE before including pb_core.h)
 *============================================================================*/

#if defined(PB_SIZE_MICRO)
    /* 8-bit systems: AVR, Z80, 6502, Game Boy */
    #define PB_MAX_ROWS         8
    #define PB_MAX_COLS         8
    #define PB_MAX_CELLS        (PB_MAX_ROWS * PB_MAX_COLS)  /* 64 */
    #define PB_MAX_COLORS       4
    #define PB_MAX_EVENTS       8
    #define PB_MAX_QUEUE        4
    #define PB_MAX_CHECKPOINTS  2
    #define PB_MAX_VISITED      32
    #define PB_EVENT_CELL_MAX   16
    #define PB_SIZE_TIER        "micro"

#elif defined(PB_SIZE_MINI)
    /* 16-bit systems: 8086, MSP430 */
    #define PB_MAX_ROWS         12
    #define PB_MAX_COLS         8
    #define PB_MAX_CELLS        (PB_MAX_ROWS * PB_MAX_COLS)  /* 96 */
    #define PB_MAX_COLORS       6
    #define PB_MAX_EVENTS       16
    #define PB_MAX_QUEUE        8
    #define PB_MAX_CHECKPOINTS  4
    #define PB_MAX_VISITED      64
    #define PB_EVENT_CELL_MAX   24
    #define PB_SIZE_TIER        "mini"

#elif defined(PB_SIZE_MEDIUM)
    /* 32-bit embedded: ARM Cortex-M, ESP32 */
    #define PB_MAX_ROWS         16
    #define PB_MAX_COLS         12
    #define PB_MAX_CELLS        (PB_MAX_ROWS * PB_MAX_COLS)  /* 192 */
    #define PB_MAX_COLORS       8
    #define PB_MAX_EVENTS       64
    #define PB_MAX_QUEUE        16
    #define PB_MAX_CHECKPOINTS  8
    #define PB_MAX_VISITED      128
    #define PB_EVENT_CELL_MAX   48
    #define PB_SIZE_TIER        "medium"

#else
    /* Default: Full size for desktop (PB_SIZE_FULL or undefined) */
    #ifndef PB_SIZE_FULL
        #define PB_SIZE_FULL
    #endif
    #define PB_MAX_ROWS         32
    #define PB_MAX_COLS         16
    #define PB_MAX_CELLS        (PB_MAX_ROWS * PB_MAX_COLS)  /* 512 */
    #define PB_MAX_COLORS       8
    #define PB_MAX_EVENTS       256
    #define PB_MAX_QUEUE        32
    #define PB_MAX_CHECKPOINTS  32
    #define PB_MAX_VISITED      256
    #define PB_EVENT_CELL_MAX   48  /* Max cells per event (rare cascades) */
    #define PB_SIZE_TIER        "full"
#endif

/*============================================================================
 * C Standard Detection and Compatibility
 *============================================================================*/

/* Auto-detect C standard if not specified */
#if !defined(PB_C89) && !defined(PB_C99) && !defined(PB_C11) && !defined(PB_C17)
    #if defined(__STDC_VERSION__)
        #if __STDC_VERSION__ >= 201710L
            #define PB_C17
        #elif __STDC_VERSION__ >= 201112L
            #define PB_C11
        #elif __STDC_VERSION__ >= 199901L
            #define PB_C99
        #else
            #define PB_C89
        #endif
    #else
        /* Pre-C99 or K&R compiler */
        #define PB_C89
    #endif
#endif

/* C standard version number for diagnostics */
#if defined(PB_C17)
    #define PB_C_STD_VERSION 2017
#elif defined(PB_C11)
    #define PB_C_STD_VERSION 2011
#elif defined(PB_C99)
    #define PB_C_STD_VERSION 1999
#else
    #define PB_C_STD_VERSION 1989
#endif

/*============================================================================
 * Fixed-Point Mode (for deterministic multiplayer)
 *============================================================================*/

#ifndef PB_USE_FIXED_POINT
    #define PB_USE_FIXED_POINT 0
#endif

/*============================================================================
 * Fixed-Point Format Selection
 *
 * Available formats (define ONE before including pb_core.h):
 *   PB_FIXED_Q16_16 - Default: 32-bit, 16.16 format (range ±32768, precision 1/65536)
 *   PB_FIXED_Q12_12 - HP48-style: 16-bit signed, 4.12 format (range ±8, precision 1/4096)
 *   PB_FIXED_Q8_8   - Compact: 16-bit signed, 8.8 format (range ±128, precision 1/256)
 *   PB_FIXED_Q8_1   - Ultra-compact: 8-bit signed, 7.1 format (range ±64, precision 0.5)
 *   PB_FIXED_Q0_8   - Normalized: 8-bit unsigned, 0.8 format (range 0-0.996, precision 1/256)
 *
 * Use case recommendations:
 *   Q16.16 - Desktop/modern: full precision, large coordinates
 *   Q12.12 - 16-bit embedded: HP48-style, 56x88 playfield
 *   Q8.8   - 8-bit embedded: small playfields, velocity/angles
 *   Q8.1   - Extreme constraints: coarse positioning, AI weights
 *   Q0.8   - Normalized values: percentages, interpolation factors
 *============================================================================*/

/* Fixed-point format identifiers */
#define PB_FP_FORMAT_Q16_16  0
#define PB_FP_FORMAT_Q12_12  1
#define PB_FP_FORMAT_Q8_8    2
#define PB_FP_FORMAT_Q8_1    3
#define PB_FP_FORMAT_Q0_8    4

/* Select fixed-point format */
#ifndef PB_FIXED_FORMAT
    #if defined(PB_FIXED_Q0_8)
        #define PB_FIXED_FORMAT PB_FP_FORMAT_Q0_8
    #elif defined(PB_FIXED_Q8_1)
        #define PB_FIXED_FORMAT PB_FP_FORMAT_Q8_1
    #elif defined(PB_FIXED_Q8_8)
        #define PB_FIXED_FORMAT PB_FP_FORMAT_Q8_8
    #elif defined(PB_FIXED_Q12_12) || defined(PB_USE_Q12_FIXED_POINT) && PB_USE_Q12_FIXED_POINT
        #define PB_FIXED_FORMAT PB_FP_FORMAT_Q12_12
    #else
        #define PB_FIXED_FORMAT PB_FP_FORMAT_Q16_16
    #endif
#endif

/* Auto-select for size tiers if not explicitly set */
#if PB_USE_FIXED_POINT && !defined(PB_FIXED_Q16_16) && !defined(PB_FIXED_Q12_12) && \
    !defined(PB_FIXED_Q8_8) && !defined(PB_FIXED_Q8_1) && !defined(PB_FIXED_Q0_8)
    #if defined(PB_SIZE_MICRO)
        #undef PB_FIXED_FORMAT
        #define PB_FIXED_FORMAT PB_FP_FORMAT_Q12_12
    #endif
#endif

/* Legacy compatibility */
#ifndef PB_USE_Q12_FIXED_POINT
    #if PB_FIXED_FORMAT == PB_FP_FORMAT_Q12_12
        #define PB_USE_Q12_FIXED_POINT 1
    #else
        #define PB_USE_Q12_FIXED_POINT 0
    #endif
#endif

/*============================================================================
 * HP48-Inspired Memory Optimizations
 * (Based on hp48-puzzle-bobble static analysis)
 *============================================================================*/

/*
 * Compact Bubble Encoding
 * Uses 2 bytes per bubble instead of ~16 bytes.
 * Format: [kind:4][color:4] [flags:4][special:4]
 * HP48 uses 1 byte; we use 2 for pb_core's special bubble support.
 */
#ifndef PB_USE_COMPACT_BUBBLES
    #if defined(PB_SIZE_MICRO) || defined(PB_SIZE_MINI)
        #define PB_USE_COMPACT_BUBBLES 1
    #else
        #define PB_USE_COMPACT_BUBBLES 0
    #endif
#endif

/*
 * Direction Index Mode
 * Uses integer index 0-78 instead of radians for cannon direction.
 * Index N maps to angle (12 + N*2) degrees.
 * Wall reflection is trivial: new_index = 78 - old_index
 * Perfect for replay/network (1 byte vs 4 bytes).
 */
#ifndef PB_USE_DIRECTION_INDEX
    #define PB_USE_DIRECTION_INDEX 0
#endif

/*
 * Game-Specific Angle Tables
 * Uses 79-entry lookup table for firing angles only (12°-168°).
 * Saves 70% memory vs full 256-entry table.
 */
#ifndef PB_USE_GAME_ANGLE_TABLE
    #if defined(PB_SIZE_MICRO) || defined(PB_SIZE_MINI)
        #define PB_USE_GAME_ANGLE_TABLE 1
    #else
        #define PB_USE_GAME_ANGLE_TABLE 0
    #endif
#endif

/*
 * In-Place Flood-Fill Marking
 * Uses bubble flags directly instead of separate visited array.
 * Saves PB_MAX_CELLS bytes during match/orphan detection.
 */
#ifndef PB_USE_INPLACE_MARKING
    #if defined(PB_SIZE_MICRO)
        #define PB_USE_INPLACE_MARKING 1
    #else
        #define PB_USE_INPLACE_MARKING 0
    #endif
#endif

/*
 * Pixel-Mask Collision
 * Alternative collision mode using sprite overlap detection.
 * More accurate for pixel-art renderers.
 */
#ifndef PB_USE_PIXEL_COLLISION
    #define PB_USE_PIXEL_COLLISION 0
#endif

/*
 * Screen Effects (shake, flash)
 * Visual warning effects before row insertion.
 */
#ifndef PB_USE_SCREEN_EFFECTS
    #define PB_USE_SCREEN_EFFECTS 1
#endif

/*============================================================================
 * Event Representation Optimization
 *============================================================================*/

/*
 * ALGORITHM OPTIMIZATION: Compact cell representation
 *
 * Current design flaw: pb_event stores cells[PB_MAX_CELLS] = 4KB per event
 * Optimized design: Use single-byte indices for boards <= 256 cells
 *
 * Cell index encoding: (row * PB_MAX_COLS + col) fits in uint8_t
 * for boards up to 16x16=256 cells. Larger boards use uint16_t.
 */
#if (PB_MAX_CELLS <= 256)
    typedef uint8_t pb_cell_index;
    #define PB_CELL_INDEX_BITS 8
#else
    typedef uint16_t pb_cell_index;
    #define PB_CELL_INDEX_BITS 16
#endif

/* Invalid cell index sentinel (all bits set) */
#define PB_CELL_INDEX_INVALID ((pb_cell_index)-1)

/*
 * Cell index encoding/decoding macros
 * Encoding: index = row * PB_MAX_COLS + col
 * This linearizes the 2D grid into a 1D index.
 */
#define PB_CELL_TO_INDEX(row, col)  ((pb_cell_index)((row) * PB_MAX_COLS + (col)))
#define PB_INDEX_TO_ROW(idx)        ((int)((idx) / PB_MAX_COLS))
#define PB_INDEX_TO_COL(idx)        ((int)((idx) % PB_MAX_COLS))

/* Maximum cells per event (covers 99.9% of gameplay) */
#ifndef PB_EVENT_CELL_MAX
    #if defined(PB_SIZE_MICRO)
        #define PB_EVENT_CELL_MAX  8
    #elif defined(PB_SIZE_MINI)
        #define PB_EVENT_CELL_MAX  16
    #elif defined(PB_SIZE_MEDIUM)
        #define PB_EVENT_CELL_MAX  32
    #else
        #define PB_EVENT_CELL_MAX  48  /* Full: rare cascade max */
    #endif
#endif

/*============================================================================
 * Feature Flags
 *============================================================================*/

/* Replay system requires larger event buffers */
#if PB_MAX_EVENTS >= 64
    #define PB_FEATURE_REPLAY 1
#else
    #define PB_FEATURE_REPLAY 0
#endif

/* Solver requires more memory */
#if defined(PB_SIZE_FULL) || defined(PB_SIZE_MEDIUM)
    #define PB_FEATURE_SOLVER 1
#else
    #define PB_FEATURE_SOLVER 0
#endif

/* JSON/data layer requires heap */
#if defined(PB_SIZE_FULL)
    #define PB_FEATURE_DATA 1
#else
    #define PB_FEATURE_DATA 0
#endif

/* CVD (color vision) simulation */
#if defined(PB_SIZE_FULL) || defined(PB_SIZE_MEDIUM)
    #define PB_FEATURE_CVD 1
#else
    #define PB_FEATURE_CVD 0
#endif

/*============================================================================
 * Platform Detection
 *============================================================================*/

/* Detect 8-bit platforms */
#if defined(__AVR__) || defined(__z80) || defined(__SDCC_z80) || \
    defined(__SDCC_stm8) || defined(__SDCC_mcs51) || \
    defined(__CC65__) || defined(__GBDK__)
    #define PB_PLATFORM_8BIT 1
    #define PB_POINTER_SIZE 2
#else
    #define PB_PLATFORM_8BIT 0
#endif

/* Detect 16-bit platforms */
#if defined(__IA16__) || defined(__ia16__) || defined(__MSDOS__) || \
    defined(__MSP430__)
    #define PB_PLATFORM_16BIT 1
    #define PB_POINTER_SIZE 2
#else
    #define PB_PLATFORM_16BIT 0
#endif

/* Detect freestanding (no OS) - must explicitly be set or detected
 * via common bare-metal toolchain patterns. Hosted Linux cross-compilers
 * define __linux__ so we exclude them. */
#if defined(PB_FREESTANDING)
    /* Already set by user */
    #define PB_PLATFORM_FREESTANDING PB_FREESTANDING
#elif PB_PLATFORM_8BIT
    /* 8-bit platforms are always freestanding */
    #define PB_PLATFORM_FREESTANDING 1
#elif defined(__GNUC__) && !defined(__linux__) && !defined(__unix__) && \
      !defined(__APPLE__) && !defined(_WIN32) && \
      (defined(__arm__) || defined(__aarch64__) || \
       defined(__riscv) || defined(__sh__) || defined(__m68k__))
    /* Bare-metal embedded (no OS detected) */
    #define PB_PLATFORM_FREESTANDING 1
#else
    #define PB_PLATFORM_FREESTANDING 0
#endif

/* Default pointer size for 32/64-bit */
#ifndef PB_POINTER_SIZE
    #if defined(__LP64__) || defined(_LP64) || defined(__x86_64__)
        #define PB_POINTER_SIZE 8
    #else
        #define PB_POINTER_SIZE 4
    #endif
#endif

/*============================================================================
 * Compiler-Specific Attributes
 *============================================================================*/

#if defined(__GNUC__) || defined(__clang__)
    #define PB_UNUSED       __attribute__((unused))
    #define PB_PACKED       __attribute__((packed))
    #define PB_ALIGNED(n)   __attribute__((aligned(n)))
    #define PB_LIKELY(x)    __builtin_expect(!!(x), 1)
    #define PB_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else
    #define PB_UNUSED
    #define PB_PACKED
    #define PB_ALIGNED(n)
    #define PB_LIKELY(x)    (x)
    #define PB_UNLIKELY(x)  (x)
#endif

/* C89-compatible inline: use static + inline variant for all standards.
 * Static is required to avoid multiple definition errors when header is
 * included in multiple translation units. */
#if defined(PB_C89)
    #if defined(__GNUC__) || defined(__clang__)
        #define PB_INLINE static __inline__
    #else
        #define PB_INLINE static
    #endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    #define PB_INLINE static inline
#else
    #define PB_INLINE static
#endif

/*============================================================================
 * Debug/Assert Configuration
 *============================================================================*/

#ifndef PB_DEBUG
    #ifdef DEBUG
        #define PB_DEBUG 1
    #else
        #define PB_DEBUG 0
    #endif
#endif

#if PB_DEBUG
    #include <assert.h>
    #define PB_ASSERT(x) assert(x)
#else
    #define PB_ASSERT(x) ((void)0)
#endif

#endif /* PB_CONFIG_H */

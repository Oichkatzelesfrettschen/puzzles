/**
 * @file pb_cvd.h
 * @brief Color Vision Deficiency (CVD) simulation and correction
 *
 * Implements Brettel, Viénot, and Mollon's (1997) dichromacy simulation
 * algorithm, which is the gold standard for CVD simulation.
 *
 * CVD Types Supported:
 * - Protanopia: Red cone deficiency (~1% of males)
 * - Deuteranopia: Green cone deficiency (~1% of males)
 * - Tritanopia: Blue cone deficiency (~0.01% of population)
 * - Monochromacy: Complete color blindness (rare)
 *
 * References:
 * - Brettel et al. 1997: "Computerized simulation of color appearance for dichromats"
 * - https://daltonlens.org/opensource-cvd-simulation/
 */

#ifndef PB_CVD_H
#define PB_CVD_H

#include "pb_color.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CVD Types
 * ============================================================================ */

/** Color Vision Deficiency types */
typedef enum pb_cvd_type {
    PB_CVD_NONE = 0,      /* Normal vision */
    PB_CVD_PROTANOPIA,    /* Red cone deficiency (L-cone) */
    PB_CVD_DEUTERANOPIA,  /* Green cone deficiency (M-cone) */
    PB_CVD_TRITANOPIA,    /* Blue cone deficiency (S-cone) */
    PB_CVD_ACHROMATOPSIA, /* Complete color blindness (rod monochromacy) */

    PB_CVD_COUNT
} pb_cvd_type;

/** Get human-readable name for CVD type */
const char* pb_cvd_type_name(pb_cvd_type type);

/* ============================================================================
 * CVD Simulation
 * ============================================================================ */

/**
 * Simulate how a color appears to someone with a specific CVD.
 * Uses the Brettel 1997 algorithm for dichromacy simulation.
 *
 * @param color Original sRGB color
 * @param type CVD type to simulate
 * @param severity Severity from 0.0 (normal) to 1.0 (full dichromacy)
 * @return Simulated color as it would appear to the CVD viewer
 */
pb_rgb pb_cvd_simulate(pb_rgb color, pb_cvd_type type, float severity);

/**
 * Simulate CVD on a hex color string.
 * @param hex_in Input hex color
 * @param type CVD type
 * @param severity 0.0 to 1.0
 * @param hex_out Output buffer (at least 8 bytes)
 * @return true on success
 */
bool pb_cvd_simulate_hex(const char* hex_in, pb_cvd_type type,
                         float severity, char* hex_out);

/* ============================================================================
 * Palette Analysis
 * ============================================================================ */

/** Result of CVD-safety analysis for a color pair */
typedef struct pb_cvd_pair_result {
    float normal_delta_e;     /* Delta E with normal vision */
    float simulated_delta_e;  /* Delta E under CVD simulation */
    bool confusable;          /* Are colors confusable under CVD? */
} pb_cvd_pair_result;

/** Result of full palette CVD analysis */
typedef struct pb_cvd_palette_result {
    pb_cvd_type type;              /* CVD type tested */
    float min_delta_e;             /* Minimum Delta E in palette */
    int confusable_pair_count;     /* Number of confusable pairs */
    int confusable_pairs[64][2];   /* Indices of confusable pairs (max 64) */
    bool safe;                     /* Is palette safe for this CVD type? */
} pb_cvd_palette_result;

/**
 * Minimum Oklab Delta E for colors to be distinguishable.
 * Values below this threshold may be confused.
 *
 * Thresholds based on research:
 * - 0.02: Just noticeable difference (JND)
 * - 0.04: Clearly distinguishable
 * - 0.06: Safely distinguishable (recommended for accessibility)
 */
#define PB_CVD_DELTA_E_THRESHOLD 0.06f

/**
 * Analyze if two colors are confusable under a specific CVD.
 *
 * @param c1 First color
 * @param c2 Second color
 * @param type CVD type to test
 * @param severity CVD severity (0.0-1.0, typically 1.0 for full dichromacy)
 * @return Analysis result
 */
pb_cvd_pair_result pb_cvd_analyze_pair(pb_rgb c1, pb_rgb c2,
                                        pb_cvd_type type, float severity);

/**
 * Analyze an entire palette for CVD safety.
 *
 * @param colors Array of sRGB colors
 * @param count Number of colors in palette
 * @param type CVD type to test
 * @param severity CVD severity
 * @param result Output result structure
 */
void pb_cvd_analyze_palette(const pb_rgb* colors, int count,
                            pb_cvd_type type, float severity,
                            pb_cvd_palette_result* result);

/**
 * Check if a palette is safe for all major CVD types.
 *
 * @param colors Array of sRGB colors
 * @param count Number of colors
 * @return true if palette is distinguishable under all CVD types
 */
bool pb_cvd_palette_is_universal(const pb_rgb* colors, int count);

/* ============================================================================
 * CVD-Safe Palette Generation
 * ============================================================================ */

/**
 * Attempt to adjust a color to improve CVD distinguishability.
 * Preserves lightness, adjusts hue/chroma in OKLCH space.
 *
 * @param color Original color
 * @param reference Color to distinguish from
 * @param type CVD type to optimize for
 * @return Adjusted color (or original if already distinguishable)
 */
pb_rgb pb_cvd_optimize_color(pb_rgb color, pb_rgb reference, pb_cvd_type type);

/**
 * Generate a maximally-spaced CVD-safe palette.
 *
 * @param count Number of colors to generate (2-8)
 * @param lightness Target lightness (0.0-1.0), typically 0.6-0.8
 * @param out Output array of at least `count` colors
 * @return true on success
 */
bool pb_cvd_generate_safe_palette(int count, float lightness, pb_rgb* out);

#ifdef __cplusplus
}
#endif

#endif /* PB_CVD_H */

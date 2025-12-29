/**
 * @file pb_color.h
 * @brief Perceptual color space conversions (sRGB, Oklab, OKLCH)
 *
 * Implements Bjorn Ottosson's Oklab perceptual color space for:
 * - Perceptually uniform color blending
 * - CVD-safe palette generation
 * - Color difference calculations (Delta E)
 *
 * References:
 * - https://bottosson.github.io/posts/oklab/
 * - https://www.w3.org/TR/WCAG21/#dfn-relative-luminance
 */

#ifndef PB_COLOR_H
#define PB_COLOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Color Types
 * ============================================================================ */

/** sRGB color with 8-bit components (0-255) */
typedef struct pb_rgb8 {
    uint8_t r, g, b;
} pb_rgb8;

/** sRGB color with floating-point components (0.0-1.0) */
typedef struct pb_rgb {
    float r, g, b;
} pb_rgb;

/** Linear RGB color (gamma-decoded, 0.0-1.0) */
typedef struct pb_linear_rgb {
    float r, g, b;
} pb_linear_rgb;

/** Oklab perceptual color space (L: 0-1, a/b: roughly -0.4 to 0.4) */
typedef struct pb_oklab {
    float L;  /* Lightness (0 = black, 1 = white) */
    float a;  /* Green-red axis */
    float b;  /* Blue-yellow axis */
} pb_oklab;

/** OKLCH cylindrical color space (L: 0-1, C: 0-0.4+, h: 0-360) */
typedef struct pb_oklch {
    float L;  /* Lightness */
    float C;  /* Chroma (saturation) */
    float h;  /* Hue in degrees */
} pb_oklch;

/* ============================================================================
 * Parsing & Formatting
 * ============================================================================ */

/**
 * Parse hex color string to RGB8.
 * Accepts: "#RGB", "#RRGGBB", "RGB", "RRGGBB"
 * @return true on success, false on parse error
 */
bool pb_hex_to_rgb8(const char* hex, pb_rgb8* out);

/**
 * Format RGB8 as hex string.
 * @param out Buffer of at least 8 bytes (writes "#RRGGBB\0")
 */
void pb_rgb8_to_hex(pb_rgb8 color, char* out);

/**
 * Pack RGB8 to 32-bit RGBA (alpha = 255).
 * Format: 0xRRGGBBAA
 */
uint32_t pb_rgb8_to_rgba32(pb_rgb8 color);

/**
 * Unpack 32-bit RGBA to RGB8 (ignores alpha).
 */
pb_rgb8 pb_rgba32_to_rgb8(uint32_t rgba);

/* ============================================================================
 * Color Space Conversions
 * ============================================================================ */

/** Convert RGB8 (0-255) to floating-point RGB (0.0-1.0) */
pb_rgb pb_rgb8_to_rgb(pb_rgb8 c);

/** Convert floating-point RGB (0.0-1.0) to RGB8 (0-255) */
pb_rgb8 pb_rgb_to_rgb8(pb_rgb c);

/** Apply sRGB gamma decoding (sRGB -> Linear RGB) */
pb_linear_rgb pb_srgb_to_linear(pb_rgb srgb);

/** Apply sRGB gamma encoding (Linear RGB -> sRGB) */
pb_rgb pb_linear_to_srgb(pb_linear_rgb linear);

/** Convert linear RGB to Oklab */
pb_oklab pb_linear_to_oklab(pb_linear_rgb linear);

/** Convert Oklab to linear RGB */
pb_linear_rgb pb_oklab_to_linear(pb_oklab lab);

/** Convert Oklab to OKLCH (cylindrical coordinates) */
pb_oklch pb_oklab_to_oklch(pb_oklab lab);

/** Convert OKLCH to Oklab (Cartesian coordinates) */
pb_oklab pb_oklch_to_oklab(pb_oklch lch);

/* ============================================================================
 * Convenience Conversions
 * ============================================================================ */

/** sRGB -> Oklab (convenience) */
pb_oklab pb_srgb_to_oklab(pb_rgb srgb);

/** Oklab -> sRGB (convenience) */
pb_rgb pb_oklab_to_srgb(pb_oklab lab);

/** sRGB -> OKLCH (convenience) */
pb_oklch pb_srgb_to_oklch(pb_rgb srgb);

/** OKLCH -> sRGB (convenience) */
pb_rgb pb_oklch_to_srgb(pb_oklch lch);

/** Hex string -> OKLCH (convenience) */
bool pb_hex_to_oklch(const char* hex, pb_oklch* out);

/** OKLCH -> Hex string (convenience) */
void pb_oklch_to_hex(pb_oklch lch, char* out);

/* ============================================================================
 * WCAG Luminance & Contrast
 * ============================================================================ */

/**
 * Calculate WCAG 2.1 relative luminance.
 * Uses WCAG-specific gamma threshold (0.03928) for standards compliance.
 * @return Luminance in range [0.0, 1.0]
 */
float pb_wcag_luminance(pb_rgb srgb);

/**
 * Calculate WCAG 2.1 contrast ratio between two colors.
 * @return Contrast ratio in range [1.0, 21.0]
 */
float pb_wcag_contrast(pb_rgb fg, pb_rgb bg);

/**
 * Check if contrast meets WCAG AA standard (4.5:1 for normal text).
 */
bool pb_wcag_aa_normal(pb_rgb fg, pb_rgb bg);

/**
 * Check if contrast meets WCAG AA standard (3.0:1 for large text).
 */
bool pb_wcag_aa_large(pb_rgb fg, pb_rgb bg);

/**
 * Check if contrast meets WCAG AAA standard (7.0:1 for normal text).
 */
bool pb_wcag_aaa_normal(pb_rgb fg, pb_rgb bg);

/* ============================================================================
 * Perceptual Distance (Delta E)
 * ============================================================================ */

/**
 * Calculate Oklab Delta E (Euclidean distance in Oklab space).
 * This is a perceptually uniform color difference metric.
 * @return Distance (0.0 = identical, ~0.02 = just noticeable, ~0.1 = clearly different)
 */
float pb_oklab_delta_e(pb_oklab c1, pb_oklab c2);

/**
 * Calculate Delta E between two sRGB colors.
 */
float pb_srgb_delta_e(pb_rgb c1, pb_rgb c2);

/**
 * Calculate Delta E between two hex color strings.
 * @return Distance, or -1.0 on parse error
 */
float pb_hex_delta_e(const char* hex1, const char* hex2);

/* ============================================================================
 * Gamut Clipping
 * ============================================================================ */

/**
 * Check if Oklab color is within sRGB gamut.
 */
bool pb_oklab_in_gamut(pb_oklab lab);

/**
 * Clip Oklab color to sRGB gamut (preserves hue, reduces chroma).
 */
pb_oklab pb_oklab_clip_gamut(pb_oklab lab);

/**
 * Clip OKLCH color to sRGB gamut (preserves hue, reduces chroma).
 */
pb_oklch pb_oklch_clip_gamut(pb_oklch lch);

#ifdef __cplusplus
}
#endif

#endif /* PB_COLOR_H */

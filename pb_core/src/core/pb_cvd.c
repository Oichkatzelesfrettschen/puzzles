/**
 * @file pb_cvd.c
 * @brief CVD simulation implementation using Brettel 1997 algorithm
 *
 * The Brettel algorithm simulates dichromacy by:
 * 1. Converting sRGB to LMS (cone response) space
 * 2. Projecting onto a "confusion plane" where the missing cone type is zero
 * 3. Converting back to sRGB
 *
 * The confusion plane is defined by:
 * - A neutral anchor point (equal-energy white)
 * - Two spectral anchor points at 475nm and 575nm (or 485/660 for tritanopia)
 */

#include "pb/pb_cvd.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * LMS Color Space Matrices (Hunt-Pointer-Estevez normalized to D65)
 * ============================================================================ */

/* sRGB (linearized) to LMS */
static const float SRGB_TO_LMS[3][3] = {
    { 0.31399022f, 0.63951294f, 0.04649755f },
    { 0.15537241f, 0.75789446f, 0.08670142f },
    { 0.01775239f, 0.10944209f, 0.87256922f }
};

/* LMS to sRGB (linearized) */
static const float LMS_TO_SRGB[3][3] = {
    {  5.47221206f, -4.64196010f,  0.16963708f },
    { -1.12524190f,  2.29317094f, -0.16789520f },
    {  0.02980165f, -0.19318073f,  1.16364789f }
};

/* ============================================================================
 * Brettel 1997 Dichromacy Simulation Matrices
 *
 * Each dichromacy type has TWO projection matrices (for different hue regions)
 * separated by a boundary defined by the confusion line through white.
 * ============================================================================ */

/* Protanopia (L-cone deficiency) projection matrices */
static const float PROTAN_A[3][3] = {
    { 0.152286f,  1.052583f, -0.204868f },
    { 0.114503f,  0.786281f,  0.099216f },
    { 0.000000f,  0.000000f,  1.000000f }
};

static const float PROTAN_B[3][3] = {
    { 0.152286f,  1.052583f, -0.204868f },
    { 0.114503f,  0.786281f,  0.099216f },
    { 0.000000f,  0.000000f,  1.000000f }
};

/* Deuteranopia (M-cone deficiency) projection matrices */
static const float DEUTAN_A[3][3] = {
    { 1.000000f,  0.000000f,  0.000000f },
    { 0.494207f,  0.000000f,  0.505793f },
    { 0.000000f,  0.000000f,  1.000000f }
};

static const float DEUTAN_B[3][3] = {
    { 1.000000f,  0.000000f,  0.000000f },
    { 0.494207f,  0.000000f,  0.505793f },
    { 0.000000f,  0.000000f,  1.000000f }
};

/* Tritanopia (S-cone deficiency) projection matrices */
static const float TRITAN_A[3][3] = {
    { 1.000000f,  0.000000f,  0.000000f },
    { 0.000000f,  1.000000f,  0.000000f },
    { -0.395913f,  0.801109f,  0.000000f }
};

static const float TRITAN_B[3][3] = {
    { 1.000000f,  0.000000f,  0.000000f },
    { 0.000000f,  1.000000f,  0.000000f },
    { -0.395913f,  0.801109f,  0.000000f }
};

/* Separation boundaries (dot product with LMS to determine which matrix) */
static const float PROTAN_SEP[3] = { 0.0f, 1.0f, -1.0f };
static const float DEUTAN_SEP[3] = { 1.0f, 0.0f, -1.0f };
static const float TRITAN_SEP[3] = { 1.0f, -1.0f, 0.0f };

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static void mat3_mult_vec3(const float m[3][3], const float v[3], float out[3]) {
    out[0] = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2];
    out[1] = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2];
    out[2] = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2];
}

static float vec3_dot(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void vec3_lerp(const float a[3], const float b[3], float t, float out[3]) {
    out[0] = a[0] + t * (b[0] - a[0]);
    out[1] = a[1] + t * (b[1] - a[1]);
    out[2] = a[2] + t * (b[2] - a[2]);
}

/* Gamma functions (same as pb_color.c) */
static float srgb_to_linear_channel(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float linear_to_srgb_channel(float c) {
    if (c <= 0.0031308f) return c * 12.92f;
    return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

/* ============================================================================
 * CVD Type Names
 * ============================================================================ */

static const char* cvd_names[] = {
    "Normal",
    "Protanopia",
    "Deuteranopia",
    "Tritanopia",
    "Achromatopsia"
};

const char* pb_cvd_type_name(pb_cvd_type type) {
    /* Note: type is enum, guaranteed >= 0 on all platforms */
    if (type >= PB_CVD_COUNT) {
        return "Unknown";
    }
    return cvd_names[type];
}

/* ============================================================================
 * Core Simulation
 * ============================================================================ */

/**
 * Apply Brettel dichromacy simulation to LMS color.
 */
static void simulate_dichromacy_lms(const float lms[3], pb_cvd_type type,
                                     float out[3]) {
    const float (*mat_a)[3] = NULL;
    const float (*mat_b)[3] = NULL;
    const float* sep = NULL;

    switch (type) {
    case PB_CVD_PROTANOPIA:
        mat_a = PROTAN_A;
        mat_b = PROTAN_B;
        sep = PROTAN_SEP;
        break;
    case PB_CVD_DEUTERANOPIA:
        mat_a = DEUTAN_A;
        mat_b = DEUTAN_B;
        sep = DEUTAN_SEP;
        break;
    case PB_CVD_TRITANOPIA:
        mat_a = TRITAN_A;
        mat_b = TRITAN_B;
        sep = TRITAN_SEP;
        break;
    default:
        /* No simulation for other types - copy through */
        out[0] = lms[0];
        out[1] = lms[1];
        out[2] = lms[2];
        return;
    }

    /* Choose matrix based on which side of separation plane */
    const float (*mat)[3] = (vec3_dot(lms, sep) >= 0.0f) ? mat_a : mat_b;

    mat3_mult_vec3(mat, lms, out);
}

/**
 * Simulate achromatopsia (complete color blindness).
 * Converts to luminance-only using standard coefficients.
 */
static void simulate_achromatopsia_linear(const float rgb[3], float out[3]) {
    /* Use relative luminance coefficients */
    float Y = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
    out[0] = Y;
    out[1] = Y;
    out[2] = Y;
}

pb_rgb pb_cvd_simulate(pb_rgb color, pb_cvd_type type, float severity) {
    if (type == PB_CVD_NONE || severity <= 0.0f) {
        return color;
    }

    severity = clampf(severity, 0.0f, 1.0f);

    /* Convert to linear RGB */
    float linear[3] = {
        srgb_to_linear_channel(color.r),
        srgb_to_linear_channel(color.g),
        srgb_to_linear_channel(color.b)
    };

    float simulated[3];

    if (type == PB_CVD_ACHROMATOPSIA) {
        simulate_achromatopsia_linear(linear, simulated);
    } else {
        /* Convert to LMS */
        float lms[3];
        mat3_mult_vec3(SRGB_TO_LMS, linear, lms);

        /* Apply dichromacy simulation */
        float lms_sim[3];
        simulate_dichromacy_lms(lms, type, lms_sim);

        /* Convert back to linear RGB */
        mat3_mult_vec3(LMS_TO_SRGB, lms_sim, simulated);
    }

    /* Interpolate based on severity */
    float result[3];
    vec3_lerp(linear, simulated, severity, result);

    /* Convert back to sRGB and clamp */
    pb_rgb out;
    out.r = clampf(linear_to_srgb_channel(result[0]), 0.0f, 1.0f);
    out.g = clampf(linear_to_srgb_channel(result[1]), 0.0f, 1.0f);
    out.b = clampf(linear_to_srgb_channel(result[2]), 0.0f, 1.0f);

    return out;
}

bool pb_cvd_simulate_hex(const char* hex_in, pb_cvd_type type,
                          float severity, char* hex_out) {
    pb_rgb8 rgb8;
    if (!pb_hex_to_rgb8(hex_in, &rgb8)) {
        return false;
    }

    pb_rgb color = pb_rgb8_to_rgb(rgb8);
    pb_rgb simulated = pb_cvd_simulate(color, type, severity);
    pb_rgb8 out8 = pb_rgb_to_rgb8(simulated);

    pb_rgb8_to_hex(out8, hex_out);
    return true;
}

/* ============================================================================
 * Palette Analysis
 * ============================================================================ */

pb_cvd_pair_result pb_cvd_analyze_pair(pb_rgb c1, pb_rgb c2,
                                        pb_cvd_type type, float severity) {
    pb_cvd_pair_result result;

    /* Normal vision Delta E */
    result.normal_delta_e = pb_srgb_delta_e(c1, c2);

    /* Simulated Delta E */
    pb_rgb sim1 = pb_cvd_simulate(c1, type, severity);
    pb_rgb sim2 = pb_cvd_simulate(c2, type, severity);
    result.simulated_delta_e = pb_srgb_delta_e(sim1, sim2);

    /* Confusable if simulated Delta E is below threshold */
    result.confusable = (result.simulated_delta_e < PB_CVD_DELTA_E_THRESHOLD);

    return result;
}

void pb_cvd_analyze_palette(const pb_rgb* colors, int count,
                            pb_cvd_type type, float severity,
                            pb_cvd_palette_result* result) {
    memset(result, 0, sizeof(*result));
    result->type = type;
    result->min_delta_e = 999.0f;
    result->safe = true;

    if (count < 2) return;

    /* Check all pairs */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            pb_cvd_pair_result pair = pb_cvd_analyze_pair(
                colors[i], colors[j], type, severity);

            if (pair.simulated_delta_e < result->min_delta_e) {
                result->min_delta_e = pair.simulated_delta_e;
            }

            if (pair.confusable && result->confusable_pair_count < 64) {
                result->confusable_pairs[result->confusable_pair_count][0] = i;
                result->confusable_pairs[result->confusable_pair_count][1] = j;
                result->confusable_pair_count++;
                result->safe = false;
            }
        }
    }
}

bool pb_cvd_palette_is_universal(const pb_rgb* colors, int count) {
    pb_cvd_palette_result result;

    /* Test all major CVD types at full severity */
    pb_cvd_type types[] = {
        PB_CVD_PROTANOPIA,
        PB_CVD_DEUTERANOPIA,
        PB_CVD_TRITANOPIA,
        PB_CVD_ACHROMATOPSIA
    };

    for (int t = 0; t < 4; t++) {
        pb_cvd_analyze_palette(colors, count, types[t], 1.0f, &result);
        if (!result.safe) {
            return false;
        }
    }

    return true;
}

/* ============================================================================
 * Palette Optimization
 * ============================================================================ */

pb_rgb pb_cvd_optimize_color(pb_rgb color, pb_rgb reference, pb_cvd_type type) {
    pb_cvd_pair_result current = pb_cvd_analyze_pair(color, reference, type, 1.0f);

    if (!current.confusable) {
        return color;  /* Already distinguishable */
    }

    /* Convert to OKLCH for hue manipulation */
    pb_oklch lch = pb_srgb_to_oklch(color);

    /* Try shifting hue in both directions to find best separation */
    float best_delta_e = current.simulated_delta_e;
    float best_hue = lch.h;

    for (int step = 1; step <= 18; step++) {  /* Up to 90 degrees each way */
        float shift = (float)step * 5.0f;

        /* Try positive shift */
        pb_oklch test = lch;
        test.h = fmodf(lch.h + shift + 360.0f, 360.0f);
        pb_rgb test_rgb = pb_oklch_to_srgb(test);

        if (pb_oklab_in_gamut(pb_oklch_to_oklab(test))) {
            pb_cvd_pair_result pair = pb_cvd_analyze_pair(
                test_rgb, reference, type, 1.0f);
            if (pair.simulated_delta_e > best_delta_e) {
                best_delta_e = pair.simulated_delta_e;
                best_hue = test.h;
            }
        }

        /* Try negative shift */
        test.h = fmodf(lch.h - shift + 360.0f, 360.0f);
        test_rgb = pb_oklch_to_srgb(test);

        if (pb_oklab_in_gamut(pb_oklch_to_oklab(test))) {
            pb_cvd_pair_result pair = pb_cvd_analyze_pair(
                test_rgb, reference, type, 1.0f);
            if (pair.simulated_delta_e > best_delta_e) {
                best_delta_e = pair.simulated_delta_e;
                best_hue = test.h;
            }
        }

        /* Stop if we found a safe color */
        if (best_delta_e >= PB_CVD_DELTA_E_THRESHOLD) {
            break;
        }
    }

    /* Return optimized color */
    lch.h = best_hue;
    pb_oklch clipped = pb_oklch_clip_gamut(lch);
    return pb_oklch_to_srgb(clipped);
}

bool pb_cvd_generate_safe_palette(int count, float lightness, pb_rgb* out) {
    if (count < 2 || count > 8 || !out) {
        return false;
    }

    /* Use golden angle (137.5 degrees) for hue spacing */
    const float GOLDEN_ANGLE = 137.5077640500378546f;

    /* Start with evenly spaced hues at constant lightness/chroma */
    float hue = 0.0f;
    float chroma = 0.12f;  /* Conservative chroma for gamut safety */

    for (int i = 0; i < count; i++) {
        pb_oklch lch = { lightness, chroma, hue };

        /* Clip to gamut */
        lch = pb_oklch_clip_gamut(lch);

        out[i] = pb_oklch_to_srgb(lch);
        hue = fmodf(hue + GOLDEN_ANGLE, 360.0f);
    }

    /* Iteratively optimize for CVD safety */
    for (int iter = 0; iter < 10; iter++) {
        bool improved = false;

        for (int i = 1; i < count; i++) {
            pb_rgb optimized = out[i];

            /* Optimize against all previous colors */
            for (int j = 0; j < i; j++) {
                /* Check all CVD types */
                for (int t = 1; t < PB_CVD_COUNT; t++) {
                    pb_rgb better = pb_cvd_optimize_color(
                        optimized, out[j], (pb_cvd_type)t);
                    if (pb_srgb_delta_e(better, optimized) > 0.01f) {
                        optimized = better;
                        improved = true;
                    }
                }
            }

            out[i] = optimized;
        }

        if (!improved) break;
    }

    return pb_cvd_palette_is_universal(out, count);
}

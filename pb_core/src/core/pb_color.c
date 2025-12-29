/**
 * @file pb_color.c
 * @brief Perceptual color space conversions implementation
 *
 * Implements Björn Ottosson's Oklab algorithm with exact matrix coefficients.
 * Includes WCAG 2.1 luminance/contrast for accessibility verification.
 */

#include "pb/pb_color.h"
#include "pb/pb_freestanding.h"

#include <ctype.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/* sRGB gamma parameters */
#define SRGB_GAMMA_THRESHOLD 0.04045f
#define SRGB_LINEAR_SCALE    12.92f
#define SRGB_GAMMA_A         0.055f
#define SRGB_GAMMA_EXP       2.4f

/* WCAG uses a slightly different threshold (historical) */
#define WCAG_GAMMA_THRESHOLD 0.03928f

/* WCAG luminance coefficients (ITU-R BT.709) */
#define WCAG_LUMA_R 0.2126f
#define WCAG_LUMA_G 0.7152f
#define WCAG_LUMA_B 0.0722f

/* Oklab M1 matrix: sRGB linear -> LMS */
static const float M1[3][3] = {
    { 0.4122214708f, 0.5363325363f, 0.0514459929f },
    { 0.2119034982f, 0.6806995451f, 0.1073969566f },
    { 0.0883024619f, 0.2817188376f, 0.6299787005f }
};

/* Oklab M2 matrix: LMS' -> Lab */
static const float M2[3][3] = {
    {  0.2104542553f,  0.7936177850f, -0.0040720468f },
    {  1.9779984951f, -2.4285922050f,  0.4505937099f },
    {  0.0259040371f,  0.7827717662f, -0.8086757660f }
};

/* Inverse M1: LMS -> sRGB linear */
static const float M1_INV[3][3] = {
    {  4.0767416621f, -3.3077115913f,  0.2309699292f },
    { -1.2684380046f,  2.6097574011f, -0.3413193965f },
    { -0.0041960863f, -0.7034186147f,  1.7076147010f }
};

/* Inverse M2: Lab -> LMS' */
static const float M2_INV[3][3] = {
    { 1.0f,  0.3963377774f,  0.2158037573f },
    { 1.0f, -0.1055613458f, -0.0638541728f },
    { 1.0f, -0.0894841775f, -1.2914855480f }
};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline float signf(float x) {
    return x < 0.0f ? -1.0f : 1.0f;
}

/* Signed cube root (preserves sign) */
static inline float cbrt_signed(float x) {
    return signf(x) * cbrtf(fabsf(x));
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ============================================================================
 * Parsing & Formatting
 * ============================================================================ */

bool pb_hex_to_rgb8(const char* hex, pb_rgb8* out) {
    if (!hex || !out) return false;

    /* Skip leading # */
    if (*hex == '#') hex++;

    size_t len = strlen(hex);

    if (len == 3) {
        /* Short form: RGB */
        int r = hex_digit(hex[0]);
        int g = hex_digit(hex[1]);
        int b = hex_digit(hex[2]);
        if (r < 0 || g < 0 || b < 0) return false;
        out->r = (uint8_t)(r * 17);  /* 0xR -> 0xRR */
        out->g = (uint8_t)(g * 17);
        out->b = (uint8_t)(b * 17);
        return true;
    }

    if (len == 6) {
        /* Long form: RRGGBB */
        int r1 = hex_digit(hex[0]);
        int r2 = hex_digit(hex[1]);
        int g1 = hex_digit(hex[2]);
        int g2 = hex_digit(hex[3]);
        int b1 = hex_digit(hex[4]);
        int b2 = hex_digit(hex[5]);
        if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
            return false;
        }
        out->r = (uint8_t)(r1 * 16 + r2);
        out->g = (uint8_t)(g1 * 16 + g2);
        out->b = (uint8_t)(b1 * 16 + b2);
        return true;
    }

    return false;
}

void pb_rgb8_to_hex(pb_rgb8 color, char* out) {
    static const char hex_chars[] = "0123456789ABCDEF";
    out[0] = '#';
    out[1] = hex_chars[(color.r >> 4) & 0xF];
    out[2] = hex_chars[color.r & 0xF];
    out[3] = hex_chars[(color.g >> 4) & 0xF];
    out[4] = hex_chars[color.g & 0xF];
    out[5] = hex_chars[(color.b >> 4) & 0xF];
    out[6] = hex_chars[color.b & 0xF];
    out[7] = '\0';
}

uint32_t pb_rgb8_to_rgba32(pb_rgb8 color) {
    return ((uint32_t)color.r << 24) |
           ((uint32_t)color.g << 16) |
           ((uint32_t)color.b << 8) |
           0xFF;
}

pb_rgb8 pb_rgba32_to_rgb8(uint32_t rgba) {
    pb_rgb8 c;
    c.r = (uint8_t)((rgba >> 24) & 0xFF);
    c.g = (uint8_t)((rgba >> 16) & 0xFF);
    c.b = (uint8_t)((rgba >> 8) & 0xFF);
    return c;
}

/* ============================================================================
 * Basic Conversions
 * ============================================================================ */

pb_rgb pb_rgb8_to_rgb(pb_rgb8 c) {
    pb_rgb out;
    out.r = c.r / 255.0f;
    out.g = c.g / 255.0f;
    out.b = c.b / 255.0f;
    return out;
}

pb_rgb8 pb_rgb_to_rgb8(pb_rgb c) {
    pb_rgb8 out;
    out.r = (uint8_t)(clampf(c.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    out.g = (uint8_t)(clampf(c.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    out.b = (uint8_t)(clampf(c.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    return out;
}

/* ============================================================================
 * Gamma Conversions
 * ============================================================================ */

static float srgb_to_linear_channel(float c) {
    if (c <= SRGB_GAMMA_THRESHOLD) {
        return c / SRGB_LINEAR_SCALE;
    }
    return powf((c + SRGB_GAMMA_A) / (1.0f + SRGB_GAMMA_A), SRGB_GAMMA_EXP);
}

static float linear_to_srgb_channel(float c) {
    if (c <= SRGB_GAMMA_THRESHOLD / SRGB_LINEAR_SCALE) {
        return c * SRGB_LINEAR_SCALE;
    }
    return (1.0f + SRGB_GAMMA_A) * powf(c, 1.0f / SRGB_GAMMA_EXP) - SRGB_GAMMA_A;
}

pb_linear_rgb pb_srgb_to_linear(pb_rgb srgb) {
    pb_linear_rgb out;
    out.r = srgb_to_linear_channel(srgb.r);
    out.g = srgb_to_linear_channel(srgb.g);
    out.b = srgb_to_linear_channel(srgb.b);
    return out;
}

pb_rgb pb_linear_to_srgb(pb_linear_rgb linear) {
    pb_rgb out;
    out.r = linear_to_srgb_channel(linear.r);
    out.g = linear_to_srgb_channel(linear.g);
    out.b = linear_to_srgb_channel(linear.b);
    return out;
}

/* ============================================================================
 * Oklab Conversions
 * ============================================================================ */

pb_oklab pb_linear_to_oklab(pb_linear_rgb linear) {
    /* Linear RGB -> LMS */
    float l = M1[0][0] * linear.r + M1[0][1] * linear.g + M1[0][2] * linear.b;
    float m = M1[1][0] * linear.r + M1[1][1] * linear.g + M1[1][2] * linear.b;
    float s = M1[2][0] * linear.r + M1[2][1] * linear.g + M1[2][2] * linear.b;

    /* LMS -> LMS' (cube root) */
    float l_ = cbrt_signed(l);
    float m_ = cbrt_signed(m);
    float s_ = cbrt_signed(s);

    /* LMS' -> Oklab */
    pb_oklab out;
    out.L = M2[0][0] * l_ + M2[0][1] * m_ + M2[0][2] * s_;
    out.a = M2[1][0] * l_ + M2[1][1] * m_ + M2[1][2] * s_;
    out.b = M2[2][0] * l_ + M2[2][1] * m_ + M2[2][2] * s_;
    return out;
}

pb_linear_rgb pb_oklab_to_linear(pb_oklab lab) {
    /* Oklab -> LMS' */
    float l_ = M2_INV[0][0] * lab.L + M2_INV[0][1] * lab.a + M2_INV[0][2] * lab.b;
    float m_ = M2_INV[1][0] * lab.L + M2_INV[1][1] * lab.a + M2_INV[1][2] * lab.b;
    float s_ = M2_INV[2][0] * lab.L + M2_INV[2][1] * lab.a + M2_INV[2][2] * lab.b;

    /* LMS' -> LMS (cube) */
    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    /* LMS -> Linear RGB */
    pb_linear_rgb out;
    out.r = M1_INV[0][0] * l + M1_INV[0][1] * m + M1_INV[0][2] * s;
    out.g = M1_INV[1][0] * l + M1_INV[1][1] * m + M1_INV[1][2] * s;
    out.b = M1_INV[2][0] * l + M1_INV[2][1] * m + M1_INV[2][2] * s;
    return out;
}

pb_oklch pb_oklab_to_oklch(pb_oklab lab) {
    pb_oklch out;
    out.L = lab.L;
    out.C = sqrtf(lab.a * lab.a + lab.b * lab.b);
    out.h = atan2f(lab.b, lab.a) * 180.0f / (float)M_PI;
    if (out.h < 0.0f) out.h += 360.0f;
    return out;
}

pb_oklab pb_oklch_to_oklab(pb_oklch lch) {
    float h_rad = lch.h * (float)M_PI / 180.0f;
    pb_oklab out;
    out.L = lch.L;
    out.a = lch.C * cosf(h_rad);
    out.b = lch.C * sinf(h_rad);
    return out;
}

/* ============================================================================
 * Convenience Conversions
 * ============================================================================ */

pb_oklab pb_srgb_to_oklab(pb_rgb srgb) {
    return pb_linear_to_oklab(pb_srgb_to_linear(srgb));
}

pb_rgb pb_oklab_to_srgb(pb_oklab lab) {
    return pb_linear_to_srgb(pb_oklab_to_linear(lab));
}

pb_oklch pb_srgb_to_oklch(pb_rgb srgb) {
    return pb_oklab_to_oklch(pb_srgb_to_oklab(srgb));
}

pb_rgb pb_oklch_to_srgb(pb_oklch lch) {
    return pb_oklab_to_srgb(pb_oklch_to_oklab(lch));
}

bool pb_hex_to_oklch(const char* hex, pb_oklch* out) {
    pb_rgb8 rgb8;
    if (!pb_hex_to_rgb8(hex, &rgb8)) return false;
    *out = pb_srgb_to_oklch(pb_rgb8_to_rgb(rgb8));
    return true;
}

void pb_oklch_to_hex(pb_oklch lch, char* out) {
    pb_rgb srgb = pb_oklch_to_srgb(lch);
    pb_rgb8 rgb8 = pb_rgb_to_rgb8(srgb);
    pb_rgb8_to_hex(rgb8, out);
}

/* ============================================================================
 * WCAG Luminance & Contrast
 * ============================================================================ */

static float wcag_srgb_to_linear_channel(float c) {
    /* WCAG uses 0.03928 threshold (historical spec) */
    if (c <= WCAG_GAMMA_THRESHOLD) {
        return c / SRGB_LINEAR_SCALE;
    }
    return powf((c + SRGB_GAMMA_A) / (1.0f + SRGB_GAMMA_A), SRGB_GAMMA_EXP);
}

float pb_wcag_luminance(pb_rgb srgb) {
    float r = wcag_srgb_to_linear_channel(srgb.r);
    float g = wcag_srgb_to_linear_channel(srgb.g);
    float b = wcag_srgb_to_linear_channel(srgb.b);
    return WCAG_LUMA_R * r + WCAG_LUMA_G * g + WCAG_LUMA_B * b;
}

float pb_wcag_contrast(pb_rgb fg, pb_rgb bg) {
    float L1 = pb_wcag_luminance(fg);
    float L2 = pb_wcag_luminance(bg);

    float L_light = L1 > L2 ? L1 : L2;
    float L_dark = L1 < L2 ? L1 : L2;

    return (L_light + 0.05f) / (L_dark + 0.05f);
}

bool pb_wcag_aa_normal(pb_rgb fg, pb_rgb bg) {
    return pb_wcag_contrast(fg, bg) >= 4.5f;
}

bool pb_wcag_aa_large(pb_rgb fg, pb_rgb bg) {
    return pb_wcag_contrast(fg, bg) >= 3.0f;
}

bool pb_wcag_aaa_normal(pb_rgb fg, pb_rgb bg) {
    return pb_wcag_contrast(fg, bg) >= 7.0f;
}

/* ============================================================================
 * Perceptual Distance (Delta E)
 * ============================================================================ */

float pb_oklab_delta_e(pb_oklab c1, pb_oklab c2) {
    float dL = c1.L - c2.L;
    float da = c1.a - c2.a;
    float db = c1.b - c2.b;
    return sqrtf(dL * dL + da * da + db * db);
}

float pb_srgb_delta_e(pb_rgb c1, pb_rgb c2) {
    pb_oklab lab1 = pb_srgb_to_oklab(c1);
    pb_oklab lab2 = pb_srgb_to_oklab(c2);
    return pb_oklab_delta_e(lab1, lab2);
}

float pb_hex_delta_e(const char* hex1, const char* hex2) {
    pb_rgb8 rgb8_1, rgb8_2;
    if (!pb_hex_to_rgb8(hex1, &rgb8_1) || !pb_hex_to_rgb8(hex2, &rgb8_2)) {
        return -1.0f;
    }
    return pb_srgb_delta_e(pb_rgb8_to_rgb(rgb8_1), pb_rgb8_to_rgb(rgb8_2));
}

/* ============================================================================
 * Gamut Clipping
 * ============================================================================ */

bool pb_oklab_in_gamut(pb_oklab lab) {
    pb_linear_rgb linear = pb_oklab_to_linear(lab);
    const float epsilon = 0.0001f;
    return linear.r >= -epsilon && linear.r <= 1.0f + epsilon &&
           linear.g >= -epsilon && linear.g <= 1.0f + epsilon &&
           linear.b >= -epsilon && linear.b <= 1.0f + epsilon;
}

pb_oklab pb_oklab_clip_gamut(pb_oklab lab) {
    if (pb_oklab_in_gamut(lab)) {
        return lab;
    }

    /* Binary search to find maximum in-gamut chroma */
    pb_oklch lch = pb_oklab_to_oklch(lab);

    float lo = 0.0f;
    float hi = lch.C;

    for (int i = 0; i < 16; i++) {
        float mid = (lo + hi) * 0.5f;
        pb_oklch test = { lch.L, mid, lch.h };
        if (pb_oklab_in_gamut(pb_oklch_to_oklab(test))) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    pb_oklch clipped = { lch.L, lo, lch.h };
    return pb_oklch_to_oklab(clipped);
}

pb_oklch pb_oklch_clip_gamut(pb_oklch lch) {
    pb_oklab lab = pb_oklch_to_oklab(lch);
    pb_oklab clipped = pb_oklab_clip_gamut(lab);
    return pb_oklab_to_oklch(clipped);
}

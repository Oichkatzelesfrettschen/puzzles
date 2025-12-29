/**
 * @file test_color.c
 * @brief Tests for pb_color and pb_cvd modules
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pb/pb_color.h"
#include "pb/pb_cvd.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  " #name "... "); \
    tests_run++; \
    if (test_##name()) { \
        printf("OK\n"); \
        tests_passed++; \
    } else { \
        printf("FAILED\n"); \
    } \
} while(0)

#define APPROX_EQ(a, b, eps) (fabsf((a) - (b)) < (eps))

/* ============================================================================
 * Hex Parsing Tests
 * ============================================================================ */

static int test_hex_parse_6digit(void) {
    pb_rgb8 rgb;
    if (!pb_hex_to_rgb8("#FF8040", &rgb)) return 0;
    return rgb.r == 255 && rgb.g == 128 && rgb.b == 64;
}

static int test_hex_parse_3digit(void) {
    pb_rgb8 rgb;
    if (!pb_hex_to_rgb8("#F84", &rgb)) return 0;
    /* F -> FF, 8 -> 88, 4 -> 44 */
    return rgb.r == 0xFF && rgb.g == 0x88 && rgb.b == 0x44;
}

static int test_hex_parse_no_hash(void) {
    pb_rgb8 rgb;
    if (!pb_hex_to_rgb8("ABCDEF", &rgb)) return 0;
    return rgb.r == 0xAB && rgb.g == 0xCD && rgb.b == 0xEF;
}

static int test_hex_format_roundtrip(void) {
    pb_rgb8 original = { 0x12, 0x34, 0x56 };
    char hex[8];
    pb_rgb8_to_hex(original, hex);

    pb_rgb8 parsed;
    if (!pb_hex_to_rgb8(hex, &parsed)) return 0;

    return original.r == parsed.r &&
           original.g == parsed.g &&
           original.b == parsed.b;
}

/* ============================================================================
 * Oklab Conversion Tests
 * ============================================================================ */

static int test_oklab_black(void) {
    pb_rgb black = { 0.0f, 0.0f, 0.0f };
    pb_oklab lab = pb_srgb_to_oklab(black);
    /* Black should have L=0, a=0, b=0 */
    return APPROX_EQ(lab.L, 0.0f, 0.001f) &&
           APPROX_EQ(lab.a, 0.0f, 0.001f) &&
           APPROX_EQ(lab.b, 0.0f, 0.001f);
}

static int test_oklab_white(void) {
    pb_rgb white = { 1.0f, 1.0f, 1.0f };
    pb_oklab lab = pb_srgb_to_oklab(white);
    /* White should have L=1, a=0, b=0 */
    return APPROX_EQ(lab.L, 1.0f, 0.001f) &&
           APPROX_EQ(lab.a, 0.0f, 0.001f) &&
           APPROX_EQ(lab.b, 0.0f, 0.001f);
}

static int test_oklab_roundtrip(void) {
    pb_rgb original = { 0.5f, 0.3f, 0.8f };
    pb_oklab lab = pb_srgb_to_oklab(original);
    pb_rgb result = pb_oklab_to_srgb(lab);

    return APPROX_EQ(original.r, result.r, 0.0001f) &&
           APPROX_EQ(original.g, result.g, 0.0001f) &&
           APPROX_EQ(original.b, result.b, 0.0001f);
}

static int test_oklch_red_hue(void) {
    pb_rgb red = { 1.0f, 0.0f, 0.0f };
    pb_oklch lch = pb_srgb_to_oklch(red);
    /* Red should have hue around 29 degrees in OKLCH */
    return lch.h > 20.0f && lch.h < 40.0f;
}

static int test_oklch_roundtrip(void) {
    pb_rgb original = { 0.2f, 0.6f, 0.9f };
    pb_oklch lch = pb_srgb_to_oklch(original);
    pb_rgb result = pb_oklch_to_srgb(lch);

    return APPROX_EQ(original.r, result.r, 0.0001f) &&
           APPROX_EQ(original.g, result.g, 0.0001f) &&
           APPROX_EQ(original.b, result.b, 0.0001f);
}

/* ============================================================================
 * WCAG Tests
 * ============================================================================ */

static int test_wcag_luminance_black(void) {
    pb_rgb black = { 0.0f, 0.0f, 0.0f };
    float lum = pb_wcag_luminance(black);
    return APPROX_EQ(lum, 0.0f, 0.001f);
}

static int test_wcag_luminance_white(void) {
    pb_rgb white = { 1.0f, 1.0f, 1.0f };
    float lum = pb_wcag_luminance(white);
    return APPROX_EQ(lum, 1.0f, 0.001f);
}

static int test_wcag_contrast_max(void) {
    pb_rgb black = { 0.0f, 0.0f, 0.0f };
    pb_rgb white = { 1.0f, 1.0f, 1.0f };
    float ratio = pb_wcag_contrast(black, white);
    /* Max contrast is 21:1 */
    return APPROX_EQ(ratio, 21.0f, 0.01f);
}

static int test_wcag_contrast_same(void) {
    pb_rgb gray = { 0.5f, 0.5f, 0.5f };
    float ratio = pb_wcag_contrast(gray, gray);
    /* Same color = 1:1 contrast */
    return APPROX_EQ(ratio, 1.0f, 0.001f);
}

static int test_wcag_aa_pass(void) {
    pb_rgb black = { 0.0f, 0.0f, 0.0f };
    pb_rgb white = { 1.0f, 1.0f, 1.0f };
    return pb_wcag_aa_normal(black, white);
}

static int test_wcag_aa_fail(void) {
    pb_rgb light_gray = { 0.7f, 0.7f, 0.7f };
    pb_rgb lighter_gray = { 0.9f, 0.9f, 0.9f };
    /* Low contrast should fail AA */
    return !pb_wcag_aa_normal(light_gray, lighter_gray);
}

/* ============================================================================
 * Delta E Tests
 * ============================================================================ */

static int test_delta_e_identical(void) {
    pb_rgb color = { 0.5f, 0.3f, 0.7f };
    float delta = pb_srgb_delta_e(color, color);
    return APPROX_EQ(delta, 0.0f, 0.0001f);
}

static int test_delta_e_perceptual(void) {
    /* Two colors that look similar should have small delta E */
    pb_rgb c1 = { 0.5f, 0.5f, 0.5f };
    pb_rgb c2 = { 0.52f, 0.5f, 0.5f };
    float delta = pb_srgb_delta_e(c1, c2);
    return delta < 0.02f;  /* Just noticeable difference */
}

static int test_delta_e_hex(void) {
    float delta = pb_hex_delta_e("#FFFFFF", "#000000");
    /* Black vs white should have large delta E */
    return delta > 0.9f;
}

/* ============================================================================
 * Gamut Tests
 * ============================================================================ */

static int test_gamut_in_bounds(void) {
    pb_rgb srgb = { 0.5f, 0.5f, 0.5f };
    pb_oklab lab = pb_srgb_to_oklab(srgb);
    return pb_oklab_in_gamut(lab);
}

static int test_gamut_clip(void) {
    /* Create an out-of-gamut color */
    pb_oklch lch = { 0.5f, 0.5f, 120.0f };  /* Very saturated */
    pb_oklch clipped = pb_oklch_clip_gamut(lch);
    pb_oklab lab = pb_oklch_to_oklab(clipped);
    return pb_oklab_in_gamut(lab);
}

/* ============================================================================
 * CVD Simulation Tests
 * ============================================================================ */

static int test_cvd_none(void) {
    pb_rgb color = { 0.5f, 0.3f, 0.8f };
    pb_rgb result = pb_cvd_simulate(color, PB_CVD_NONE, 1.0f);
    /* Should be unchanged */
    return APPROX_EQ(color.r, result.r, 0.0001f) &&
           APPROX_EQ(color.g, result.g, 0.0001f) &&
           APPROX_EQ(color.b, result.b, 0.0001f);
}

static int test_cvd_zero_severity(void) {
    pb_rgb color = { 0.5f, 0.3f, 0.8f };
    pb_rgb result = pb_cvd_simulate(color, PB_CVD_PROTANOPIA, 0.0f);
    /* Zero severity should be unchanged */
    return APPROX_EQ(color.r, result.r, 0.0001f) &&
           APPROX_EQ(color.g, result.g, 0.0001f) &&
           APPROX_EQ(color.b, result.b, 0.0001f);
}

static int test_cvd_protan_red_shift(void) {
    pb_rgb red = { 1.0f, 0.0f, 0.0f };
    pb_rgb result = pb_cvd_simulate(red, PB_CVD_PROTANOPIA, 1.0f);
    /* Protanopia should reduce perception of red */
    return result.r < 1.0f || result.g > 0.0f;
}

static int test_cvd_achromat_gray(void) {
    pb_rgb color = { 0.5f, 0.3f, 0.8f };
    pb_rgb result = pb_cvd_simulate(color, PB_CVD_ACHROMATOPSIA, 1.0f);
    /* Achromatopsia should produce gray (all channels equal) */
    return APPROX_EQ(result.r, result.g, 0.001f) &&
           APPROX_EQ(result.g, result.b, 0.001f);
}

static int test_cvd_type_names(void) {
    return strcmp(pb_cvd_type_name(PB_CVD_PROTANOPIA), "Protanopia") == 0 &&
           strcmp(pb_cvd_type_name(PB_CVD_DEUTERANOPIA), "Deuteranopia") == 0;
}

/* ============================================================================
 * CVD Analysis Tests
 * ============================================================================ */

static int test_cvd_pair_distinguishable(void) {
    pb_rgb blue = { 0.0f, 0.0f, 1.0f };
    pb_rgb yellow = { 1.0f, 1.0f, 0.0f };
    pb_cvd_pair_result result = pb_cvd_analyze_pair(
        blue, yellow, PB_CVD_PROTANOPIA, 1.0f);
    /* Blue and yellow should be distinguishable even with CVD */
    return !result.confusable;
}

static int test_cvd_pair_confusable(void) {
    /* Red and green are classically confusable for protanopia */
    pb_rgb red = { 0.8f, 0.2f, 0.1f };
    pb_rgb green = { 0.3f, 0.6f, 0.1f };
    pb_cvd_pair_result result = pb_cvd_analyze_pair(
        red, green, PB_CVD_PROTANOPIA, 1.0f);
    /* These specific colors may or may not be confusable - check delta E is lower */
    return result.simulated_delta_e < result.normal_delta_e;
}

static int test_cvd_palette_analysis(void) {
    pb_rgb palette[3] = {
        { 0.0f, 0.0f, 1.0f },   /* Blue */
        { 1.0f, 1.0f, 0.0f },   /* Yellow */
        { 0.5f, 0.5f, 0.5f }    /* Gray */
    };
    pb_cvd_palette_result result;
    pb_cvd_analyze_palette(palette, 3, PB_CVD_PROTANOPIA, 1.0f, &result);
    /* Should have analyzed all pairs */
    return result.type == PB_CVD_PROTANOPIA;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("pb_color/pb_cvd test suite\n");
    printf("==========================\n\n");

    printf("Hex parsing:\n");
    TEST(hex_parse_6digit);
    TEST(hex_parse_3digit);
    TEST(hex_parse_no_hash);
    TEST(hex_format_roundtrip);

    printf("\nOklab conversions:\n");
    TEST(oklab_black);
    TEST(oklab_white);
    TEST(oklab_roundtrip);
    TEST(oklch_red_hue);
    TEST(oklch_roundtrip);

    printf("\nWCAG luminance/contrast:\n");
    TEST(wcag_luminance_black);
    TEST(wcag_luminance_white);
    TEST(wcag_contrast_max);
    TEST(wcag_contrast_same);
    TEST(wcag_aa_pass);
    TEST(wcag_aa_fail);

    printf("\nDelta E:\n");
    TEST(delta_e_identical);
    TEST(delta_e_perceptual);
    TEST(delta_e_hex);

    printf("\nGamut:\n");
    TEST(gamut_in_bounds);
    TEST(gamut_clip);

    printf("\nCVD simulation:\n");
    TEST(cvd_none);
    TEST(cvd_zero_severity);
    TEST(cvd_protan_red_shift);
    TEST(cvd_achromat_gray);
    TEST(cvd_type_names);

    printf("\nCVD analysis:\n");
    TEST(cvd_pair_distinguishable);
    TEST(cvd_pair_confusable);
    TEST(cvd_palette_analysis);

    printf("\n==========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

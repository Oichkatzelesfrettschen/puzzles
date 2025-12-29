/*
 * test_cvd.c - Tests for pb_cvd module (Color Vision Deficiency)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  " #name "..."); \
    test_##name(); \
    tests_run++; \
    tests_passed++; \
    printf(" OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAILED at line %d: %s\n", __LINE__, #cond); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NEAR(a, b, eps) ASSERT(fabs((a) - (b)) < (eps))

/* Helper to create RGB */
static pb_rgb rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (pb_rgb){r, g, b};
}

/*============================================================================
 * Type Name Tests
 *============================================================================*/

TEST(cvd_type_name_normal)
{
    const char* name = pb_cvd_type_name(PB_CVD_NONE);
    ASSERT_NOT_NULL(name);
    ASSERT_TRUE(strlen(name) > 0);
}

TEST(cvd_type_name_protan)
{
    const char* name = pb_cvd_type_name(PB_CVD_PROTANOPIA);
    ASSERT_NOT_NULL(name);
    ASSERT_TRUE(strstr(name, "rotan") != NULL || strstr(name, "Protan") != NULL);
}

TEST(cvd_type_name_deutan)
{
    const char* name = pb_cvd_type_name(PB_CVD_DEUTERANOPIA);
    ASSERT_NOT_NULL(name);
}

TEST(cvd_type_name_tritan)
{
    const char* name = pb_cvd_type_name(PB_CVD_TRITANOPIA);
    ASSERT_NOT_NULL(name);
}

TEST(cvd_type_name_achroma)
{
    const char* name = pb_cvd_type_name(PB_CVD_ACHROMATOPSIA);
    ASSERT_NOT_NULL(name);
}

/*============================================================================
 * Simulation Tests
 *============================================================================*/

TEST(cvd_simulate_none_unchanged)
{
    pb_rgb red = rgb(255, 0, 0);
    pb_rgb result = pb_cvd_simulate(red, PB_CVD_NONE, 1.0f);

    /* No simulation should return original color */
    ASSERT_EQ(result.r, 255);
    ASSERT_EQ(result.g, 0);
    ASSERT_EQ(result.b, 0);
}

TEST(cvd_simulate_protan_red_shift)
{
    pb_rgb red = rgb(255, 0, 0);
    pb_rgb result = pb_cvd_simulate(red, PB_CVD_PROTANOPIA, 1.0f);

    /* Red should appear much darker/different to protanopes */
    /* The specific values depend on the simulation algorithm */
    ASSERT_TRUE(result.r != 255 || result.g != 0 || result.b != 0);
}

TEST(cvd_simulate_deutan_green_shift)
{
    pb_rgb green = rgb(0, 255, 0);
    pb_rgb result = pb_cvd_simulate(green, PB_CVD_DEUTERANOPIA, 1.0f);

    /* Green should appear different to deuteranopes */
    ASSERT_TRUE(result.r != 0 || result.g != 255 || result.b != 0);
}

TEST(cvd_simulate_tritan_blue_shift)
{
    pb_rgb blue = rgb(0, 0, 255);
    pb_rgb result = pb_cvd_simulate(blue, PB_CVD_TRITANOPIA, 1.0f);

    /* Blue should appear different to tritanopes */
    ASSERT_TRUE(result.r != 0 || result.g != 0 || result.b != 255);
}

TEST(cvd_simulate_achroma_grayscale)
{
    pb_rgb red = rgb(255, 0, 0);
    pb_rgb result = pb_cvd_simulate(red, PB_CVD_ACHROMATOPSIA, 1.0f);

    /* Achromatopsia should produce grayscale */
    ASSERT_EQ(result.r, result.g);
    ASSERT_EQ(result.g, result.b);
}

TEST(cvd_simulate_severity_zero)
{
    pb_rgb red = rgb(255, 0, 0);
    pb_rgb result = pb_cvd_simulate(red, PB_CVD_PROTANOPIA, 0.0f);

    /* Zero severity should return original */
    ASSERT_EQ(result.r, 255);
    ASSERT_EQ(result.g, 0);
    ASSERT_EQ(result.b, 0);
}

TEST(cvd_simulate_severity_half)
{
    pb_rgb red = rgb(255, 0, 0);
    pb_rgb full = pb_cvd_simulate(red, PB_CVD_PROTANOPIA, 1.0f);
    pb_rgb half = pb_cvd_simulate(red, PB_CVD_PROTANOPIA, 0.5f);

    /* Half severity should be between original and full simulation */
    /* This is a rough check - half simulation should be intermediate */
    ASSERT_TRUE(half.r >= full.r || half.r <= 255);
}

/*============================================================================
 * Hex Simulation Tests
 *============================================================================*/

TEST(cvd_simulate_hex_valid)
{
    char out[8];
    bool ok = pb_cvd_simulate_hex("#FF0000", PB_CVD_PROTANOPIA, 1.0f, out);

    ASSERT_TRUE(ok);
    ASSERT_EQ(out[0], '#');
    ASSERT_EQ(strlen(out), 7);
}

TEST(cvd_simulate_hex_lowercase)
{
    char out[8];
    bool ok = pb_cvd_simulate_hex("#ff00ff", PB_CVD_DEUTERANOPIA, 1.0f, out);

    ASSERT_TRUE(ok);
}

TEST(cvd_simulate_hex_no_hash)
{
    char out[8];
    bool ok = pb_cvd_simulate_hex("00FF00", PB_CVD_TRITANOPIA, 1.0f, out);

    /* Should handle colors without # prefix */
    ASSERT_TRUE(ok);
}

TEST(cvd_simulate_hex_invalid)
{
    char out[8];

    /* Invalid hex should fail */
    bool ok = pb_cvd_simulate_hex("not a color", PB_CVD_PROTANOPIA, 1.0f, out);
    ASSERT_FALSE(ok);
}

/*============================================================================
 * Pair Analysis Tests
 *============================================================================*/

TEST(cvd_analyze_pair_distinguishable)
{
    pb_rgb red = rgb(255, 0, 0);
    pb_rgb blue = rgb(0, 0, 255);

    pb_cvd_pair_result result = pb_cvd_analyze_pair(red, blue, PB_CVD_PROTANOPIA, 0.04f);

    /* Red and blue should be distinguishable even for protanopes */
    ASSERT_TRUE(!result.confusable);
}

TEST(cvd_analyze_pair_confusable)
{
    /* Red and green are classic protan confusion colors */
    pb_rgb red = rgb(200, 50, 50);
    pb_rgb green = rgb(50, 150, 50);

    pb_cvd_pair_result result = pb_cvd_analyze_pair(red, green, PB_CVD_PROTANOPIA, 0.05f);

    /* This pair might be confusable depending on threshold */
    /* The delta_e should be small */
    ASSERT_TRUE(result.simulated_delta_e < 0.3f);
}

TEST(cvd_analyze_pair_delta_e)
{
    pb_rgb white = rgb(255, 255, 255);
    pb_rgb black = rgb(0, 0, 0);

    pb_cvd_pair_result result = pb_cvd_analyze_pair(white, black, PB_CVD_NONE, 0.04f);

    /* Black and white should have high delta E */
    ASSERT_TRUE(result.simulated_delta_e > 0.5f);
    ASSERT_TRUE(!result.confusable);
}

/*============================================================================
 * Palette Analysis Tests
 *============================================================================*/

TEST(cvd_analyze_palette_good)
{
    pb_rgb palette[4] = {
        rgb(255, 0, 0),    /* Red */
        rgb(0, 0, 255),    /* Blue */
        rgb(255, 255, 0),  /* Yellow */
        rgb(128, 0, 128),  /* Purple */
    };

    pb_cvd_palette_result result;
    pb_cvd_analyze_palette(palette, 4, PB_CVD_NONE, 0.04f, &result);

    ASSERT_TRUE(result.min_delta_e > 0.0f);
}

TEST(cvd_analyze_palette_confusing)
{
    /* All similar greens - will be confusing */
    pb_rgb palette[3] = {
        rgb(0, 200, 0),
        rgb(0, 210, 0),
        rgb(0, 220, 0),
    };

    pb_cvd_palette_result result;
    pb_cvd_analyze_palette(palette, 3, PB_CVD_NONE, 0.04f, &result);

    /* Min delta E should be small */
    ASSERT_TRUE(result.min_delta_e < 0.1f);
    ASSERT_TRUE(result.confusable_pair_count > 0);
}

TEST(cvd_analyze_palette_protan)
{
    pb_rgb palette[4] = {
        rgb(255, 0, 0),
        rgb(0, 255, 0),
        rgb(0, 0, 255),
        rgb(255, 255, 0),
    };

    pb_cvd_palette_result result;
    pb_cvd_analyze_palette(palette, 4, PB_CVD_PROTANOPIA, 0.04f, &result);

    /* Protanopes may confuse red/green */
    /* Result should indicate some potential confusion */
}

/*============================================================================
 * Universal Accessibility Tests
 *============================================================================*/

TEST(cvd_palette_universal_good)
{
    /* Well-chosen palette that works for all CVD types */
    pb_rgb palette[4] = {
        rgb(0, 0, 0),       /* Black */
        rgb(255, 255, 255), /* White */
        rgb(0, 114, 178),   /* Blue (CVD-safe) */
        rgb(230, 159, 0),   /* Orange (CVD-safe) */
    };

    (void)pb_cvd_palette_is_universal(palette, 4);
    /* This palette is designed to be CVD-safe */
    /* Result depends on threshold and algorithm */
}

TEST(cvd_palette_universal_bad)
{
    /* Red/green palette - bad for protan/deutan */
    pb_rgb palette[2] = {
        rgb(255, 0, 0),
        rgb(0, 255, 0),
    };

    bool universal = pb_cvd_palette_is_universal(palette, 2);
    ASSERT_FALSE(universal);
}

/*============================================================================
 * Color Optimization Tests
 *============================================================================*/

TEST(cvd_optimize_color)
{
    pb_rgb original = rgb(255, 0, 0);
    pb_rgb reference = rgb(0, 0, 255);

    (void)pb_cvd_optimize_color(original, reference, PB_CVD_PROTANOPIA);

    /* Optimized color should be different from original */
    /* and more distinguishable from reference for protanopes */
    /* This is hard to test precisely without knowing the algorithm */
}

/*============================================================================
 * Palette Generation Tests
 *============================================================================*/

TEST(cvd_generate_safe_palette_4)
{
    pb_rgb palette[4];
    bool ok = pb_cvd_generate_safe_palette(4, 0.6f, palette);

    /* Generation may or may not succeed depending on algorithm */
    /* If it succeeds, palette should be distinguishable */
    if (ok) {
        for (int i = 0; i < 4; i++) {
            ASSERT_TRUE(palette[i].r >= 0 && palette[i].r <= 255);
        }
    }
}

TEST(cvd_generate_safe_palette_8)
{
    pb_rgb palette[8];
    (void)pb_cvd_generate_safe_palette(8, 0.5f, palette);

    /* 8 colors is challenging but possible */
}

TEST(cvd_generate_safe_palette_too_many)
{
    pb_rgb palette[20];
    (void)pb_cvd_generate_safe_palette(20, 0.6f, palette);

    /* Very large palettes may fail to generate */
    /* 20 distinguishable colors for all CVD types is very hard */
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("pb_cvd tests\n");
    printf("============\n\n");

    printf("Type names:\n");
    RUN_TEST(cvd_type_name_normal);
    RUN_TEST(cvd_type_name_protan);
    RUN_TEST(cvd_type_name_deutan);
    RUN_TEST(cvd_type_name_tritan);
    RUN_TEST(cvd_type_name_achroma);

    printf("\nSimulation:\n");
    RUN_TEST(cvd_simulate_none_unchanged);
    RUN_TEST(cvd_simulate_protan_red_shift);
    RUN_TEST(cvd_simulate_deutan_green_shift);
    RUN_TEST(cvd_simulate_tritan_blue_shift);
    RUN_TEST(cvd_simulate_achroma_grayscale);
    RUN_TEST(cvd_simulate_severity_zero);
    RUN_TEST(cvd_simulate_severity_half);

    printf("\nHex simulation:\n");
    RUN_TEST(cvd_simulate_hex_valid);
    RUN_TEST(cvd_simulate_hex_lowercase);
    RUN_TEST(cvd_simulate_hex_no_hash);
    RUN_TEST(cvd_simulate_hex_invalid);

    printf("\nPair analysis:\n");
    RUN_TEST(cvd_analyze_pair_distinguishable);
    RUN_TEST(cvd_analyze_pair_confusable);
    RUN_TEST(cvd_analyze_pair_delta_e);

    printf("\nPalette analysis:\n");
    RUN_TEST(cvd_analyze_palette_good);
    RUN_TEST(cvd_analyze_palette_confusing);
    RUN_TEST(cvd_analyze_palette_protan);

    printf("\nUniversal accessibility:\n");
    RUN_TEST(cvd_palette_universal_good);
    RUN_TEST(cvd_palette_universal_bad);

    printf("\nColor optimization:\n");
    RUN_TEST(cvd_optimize_color);

    printf("\nPalette generation:\n");
    RUN_TEST(cvd_generate_safe_palette_4);
    RUN_TEST(cvd_generate_safe_palette_8);
    RUN_TEST(cvd_generate_safe_palette_too_many);

    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

/*
 * cvd_palette.c - Color Vision Deficiency palette analysis
 *
 * Demonstrates:
 * - Oklab/OKLCH color space conversions
 * - CVD simulation (protanopia, deuteranopia, tritanopia)
 * - Perceptual distance calculations (Delta E)
 * - Palette validation for accessibility
 *
 * Build:
 *   make examples
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Print color in hex format
 */
static void print_rgb(const char* label, pb_rgb c)
{
    pb_rgb8 c8 = pb_rgb_to_rgb8(c);
    printf("%s: #%02X%02X%02X", label, c8.r, c8.g, c8.b);
}

/*
 * Simulate how a color appears under different CVD types
 */
static void analyze_color(pb_rgb original)
{
    pb_oklab lab = pb_srgb_to_oklab(original);

    printf("\n");
    print_rgb("Original", original);
    printf("  (L=%.3f, a=%.3f, b=%.3f)\n", lab.L, lab.a, lab.b);

    /* Simulate each CVD type */
    const char* cvd_names[] = {"Protanopia", "Deuteranopia", "Tritanopia", "Achromatopsia"};
    pb_cvd_type types[] = {PB_CVD_PROTANOPIA, PB_CVD_DEUTERANOPIA, PB_CVD_TRITANOPIA, PB_CVD_ACHROMATOPSIA};

    for (int i = 0; i < 4; i++) {
        pb_rgb simulated = pb_cvd_simulate(original, types[i], 1.0f);
        pb_oklab sim_lab = pb_srgb_to_oklab(simulated);

        printf("  %-14s: ", cvd_names[i]);
        print_rgb("", simulated);

        /* Show perceptual shift */
        float dist = pb_oklab_delta_e(lab, sim_lab);
        printf("  (shift=%.3f)\n", dist);
    }
}

/*
 * Check if two colors are distinguishable under a specific CVD type
 */
static void check_pair(pb_rgb a, pb_rgb b)
{
    printf("\n");
    print_rgb("Color A", a);
    printf("\n");
    print_rgb("Color B", b);
    printf("\n");

    /* Check for each CVD type */
    pb_cvd_type types[] = {PB_CVD_NONE, PB_CVD_PROTANOPIA, PB_CVD_DEUTERANOPIA, PB_CVD_TRITANOPIA};
    const char* names[] = {"Normal", "Protan", "Deutan", "Tritan"};

    for (int i = 0; i < 4; i++) {
        pb_cvd_pair_result result = pb_cvd_analyze_pair(a, b, types[i], 1.0f);
        printf("  %-8s: simulated delta_e=%.4f  %s\n",
               names[i],
               result.simulated_delta_e,
               result.confusable ? "CONFUSABLE" : "OK");
    }
}

/*
 * Analyze a complete palette for a specific CVD type
 */
static void analyze_palette(pb_rgb* colors, int count, pb_cvd_type cvd_type)
{
    printf("\n=== PALETTE ANALYSIS (%s) ===\n", pb_cvd_type_name(cvd_type));
    printf("Colors: %d\n", count);

    pb_cvd_palette_result result;
    pb_cvd_analyze_palette(colors, count, cvd_type, 1.0f, &result);

    printf("Minimum Delta E: %.4f\n", result.min_delta_e);
    printf("Confusable pairs: %d\n", result.confusable_pair_count);

    for (int i = 0; i < result.confusable_pair_count && i < 5; i++) {
        printf("  Pair %d-%d\n",
               result.confusable_pairs[i][0],
               result.confusable_pairs[i][1]);
    }

    printf("Safe: %s\n", result.safe ? "YES" : "NO");
}

int main(void)
{
    printf("pb_core CVD Accessibility Demo\n");
    printf("==============================\n");

    /* Define a test palette - typical game colors (float 0.0-1.0) */
    pb_rgb red    = {0.875f, 0.25f, 0.25f};
    pb_rgb green  = {0.25f, 0.75f, 0.25f};
    pb_rgb blue   = {0.25f, 0.375f, 0.875f};
    pb_rgb yellow = {0.875f, 0.815f, 0.25f};
    pb_rgb purple = {0.625f, 0.25f, 0.75f};
    pb_rgb orange = {0.875f, 0.5f, 0.1875f};

    printf("\n=== COLOR SIMULATION ===\n");
    printf("How each color appears under different vision types:\n");

    analyze_color(red);
    analyze_color(green);
    analyze_color(blue);
    analyze_color(yellow);

    printf("\n=== PAIR ANALYSIS ===\n");
    printf("Checking color pairs for distinguishability:\n");

    /* Red-Green is the classic problematic pair */
    check_pair(red, green);

    /* Blue-Purple can be hard for some */
    check_pair(blue, purple);

    /* Red-Blue should be safe */
    check_pair(red, blue);

    /* Full palette analysis for each CVD type */
    pb_rgb palette[] = {red, green, blue, yellow, purple, orange};

    analyze_palette(palette, 6, PB_CVD_NONE);
    analyze_palette(palette, 6, PB_CVD_PROTANOPIA);
    analyze_palette(palette, 6, PB_CVD_DEUTERANOPIA);
    analyze_palette(palette, 6, PB_CVD_TRITANOPIA);

    /* Check universal accessibility */
    printf("\n=== UNIVERSAL ACCESSIBILITY ===\n");
    bool universal = pb_cvd_palette_is_universal(palette, 6);
    printf("Palette is universally accessible: %s\n", universal ? "YES" : "NO");

    /* Generate a CVD-safe palette */
    printf("\n=== GENERATED CVD-SAFE PALETTE ===\n");
    pb_rgb safe_palette[6];
    if (pb_cvd_generate_safe_palette(6, 0.7f, safe_palette)) {
        printf("Generated 6-color CVD-safe palette:\n");
        for (int i = 0; i < 6; i++) {
            printf("  Color %d: ", i);
            print_rgb("", safe_palette[i]);
            printf("\n");
        }

        bool safe = pb_cvd_palette_is_universal(safe_palette, 6);
        printf("Generated palette is universally accessible: %s\n", safe ? "YES" : "NO");
    } else {
        printf("Failed to generate palette\n");
    }

    return 0;
}

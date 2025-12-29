/**
 * @file test_softmath.c
 * @brief Tests for pb_softmath pure-integer math library
 *
 * Validates CORDIC trigonometry, Newton-Raphson operations,
 * and integer-only exp/log implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pb/pb_softmath.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void name(void)

#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  %s... ", #name); \
    name(); \
    tests_passed++; \
    printf("OK\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", #cond, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_NEAR(actual, expected, tolerance) do { \
    double _a = (double)(actual); \
    double _e = (double)(expected); \
    double _t = (double)(tolerance); \
    if (fabs(_a - _e) > _t) { \
        printf("FAIL: expected %.6f, got %.6f (tol %.6f) at line %d\n", \
               _e, _a, _t, __LINE__); \
        exit(1); \
    } \
} while(0)

/* Convert brad16 to degrees for easier comparison */
static double brad16_to_deg(pb_brad16_t b) {
    return (double)b * 360.0 / 65536.0;
}

/* Convert Q16.16 to float */
static double q16_to_float(int32_t x) {
    return (double)x / 65536.0;
}

/* Convert Q1.15 to float */
static double q15_to_float(int16_t x) {
    return (double)x / 32768.0;
}

/* ============================================================================
 * CORDIC Trigonometry Tests
 * ============================================================================ */

TEST(test_cordic_sin_cos_quadrants) {
    int16_t s, c;

    /* 0 degrees: sin=0, cos=1 */
    pb_softmath_sincos_brad16(0, &s, &c);
    ASSERT_NEAR(q15_to_float(s), 0.0, 0.002);
    ASSERT_NEAR(q15_to_float(c), 1.0, 0.002);

    /* 90 degrees = 16384 brads: sin=1, cos=0 */
    pb_softmath_sincos_brad16(PB_BRAD16_QUARTER, &s, &c);
    ASSERT_NEAR(q15_to_float(s), 1.0, 0.002);
    ASSERT_NEAR(q15_to_float(c), 0.0, 0.002);

    /* 180 degrees = 32768 brads: sin=0, cos=-1 */
    pb_softmath_sincos_brad16(PB_BRAD16_HALF, &s, &c);
    ASSERT_NEAR(q15_to_float(s), 0.0, 0.002);
    ASSERT_NEAR(q15_to_float(c), -1.0, 0.002);

    /* 270 degrees = -16384 brads: sin=-1, cos=0 */
    pb_softmath_sincos_brad16(-PB_BRAD16_QUARTER, &s, &c);
    ASSERT_NEAR(q15_to_float(s), -1.0, 0.002);
    ASSERT_NEAR(q15_to_float(c), 0.0, 0.002);
}

TEST(test_cordic_sin_cos_45deg) {
    int16_t s, c;
    double sqrt2_2 = 0.7071067811865;  /* sqrt(2)/2 */

    /* 45 degrees = 8192 brads */
    pb_softmath_sincos_brad16(PB_BRAD16_EIGHTH, &s, &c);
    ASSERT_NEAR(q15_to_float(s), sqrt2_2, 0.003);
    ASSERT_NEAR(q15_to_float(c), sqrt2_2, 0.003);

    /* 135 degrees */
    pb_softmath_sincos_brad16(PB_BRAD16_QUARTER + PB_BRAD16_EIGHTH, &s, &c);
    ASSERT_NEAR(q15_to_float(s), sqrt2_2, 0.003);
    ASSERT_NEAR(q15_to_float(c), -sqrt2_2, 0.003);
}

TEST(test_cordic_sin_cos_30_60) {
    int16_t s, c;
    pb_brad16_t deg30 = PB_DEG_TO_BRAD16(30);
    pb_brad16_t deg60 = PB_DEG_TO_BRAD16(60);

    /* 30 degrees: sin=0.5, cos=sqrt(3)/2 ≈ 0.866 */
    pb_softmath_sincos_brad16(deg30, &s, &c);
    ASSERT_NEAR(q15_to_float(s), 0.5, 0.003);
    ASSERT_NEAR(q15_to_float(c), 0.866, 0.003);

    /* 60 degrees: sin=sqrt(3)/2, cos=0.5 */
    pb_softmath_sincos_brad16(deg60, &s, &c);
    ASSERT_NEAR(q15_to_float(s), 0.866, 0.003);
    ASSERT_NEAR(q15_to_float(c), 0.5, 0.003);
}

TEST(test_cordic_sin_q16) {
    int32_t result;
    pb_brad16_t deg45 = PB_DEG_TO_BRAD16(45);

    result = pb_softmath_sin_q16(0);
    ASSERT_NEAR(q16_to_float(result), 0.0, 0.003);

    result = pb_softmath_sin_q16(PB_BRAD16_QUARTER);
    ASSERT_NEAR(q16_to_float(result), 1.0, 0.003);

    result = pb_softmath_sin_q16(deg45);
    ASSERT_NEAR(q16_to_float(result), 0.7071, 0.003);
}

TEST(test_cordic_cos_q16) {
    int32_t result;

    result = pb_softmath_cos_q16(0);
    ASSERT_NEAR(q16_to_float(result), 1.0, 0.003);

    result = pb_softmath_cos_q16(PB_BRAD16_QUARTER);
    ASSERT_NEAR(q16_to_float(result), 0.0, 0.003);

    result = pb_softmath_cos_q16(PB_BRAD16_HALF);
    ASSERT_NEAR(q16_to_float(result), -1.0, 0.003);
}

/* ============================================================================
 * CORDIC atan2 Tests
 * ============================================================================ */

TEST(test_cordic_atan2_quadrants) {
    pb_brad16_t angle;
    int32_t mag;

    /* atan2(0, 1) = 0 degrees */
    angle = pb_softmath_atan2_brad16(0, 0x10000, &mag);
    ASSERT_NEAR(brad16_to_deg(angle), 0.0, 1.0);

    /* atan2(1, 0) = 90 degrees */
    angle = pb_softmath_atan2_brad16(0x10000, 0, &mag);
    ASSERT_NEAR(brad16_to_deg(angle), 90.0, 1.0);

    /* atan2(0, -1) = 180 degrees */
    angle = pb_softmath_atan2_brad16(0, -0x10000, &mag);
    ASSERT_NEAR(fabs(brad16_to_deg(angle)), 180.0, 1.0);

    /* atan2(-1, 0) = -90 degrees */
    angle = pb_softmath_atan2_brad16(-0x10000, 0, &mag);
    ASSERT_NEAR(brad16_to_deg(angle), -90.0, 1.0);
}

TEST(test_cordic_atan2_45deg) {
    pb_brad16_t angle;
    int32_t mag;

    /* atan2(1, 1) = 45 degrees */
    angle = pb_softmath_atan2_brad16(0x10000, 0x10000, &mag);
    ASSERT_NEAR(brad16_to_deg(angle), 45.0, 1.0);

    /* atan2(-1, 1) = -45 degrees */
    angle = pb_softmath_atan2_brad16(-0x10000, 0x10000, &mag);
    ASSERT_NEAR(brad16_to_deg(angle), -45.0, 1.0);
}

/* ============================================================================
 * Newton-Raphson Reciprocal Tests
 * ============================================================================ */

TEST(test_nr_recip32_powers_of_2) {
    uint32_t result;

    /* 2^32 / 2 = 2^31 = 0x80000000 */
    result = pb_softmath_recip32(2);
    ASSERT(result == 0x80000000u);

    /* 2^32 / 4 = 2^30 = 0x40000000 */
    result = pb_softmath_recip32(4);
    ASSERT_NEAR((double)result / 0x40000000u, 1.0, 0.01);  /* Allow 1% error */

    /* 2^32 / 256 = 2^24 = 0x01000000 */
    result = pb_softmath_recip32(256);
    ASSERT_NEAR((double)result / 0x01000000u, 1.0, 0.01);
}

TEST(test_nr_invsqrt_q16) {
    int32_t result;

    /* 1/sqrt(1.0) = 1.0 */
    result = pb_softmath_invsqrt_q16(0x10000);  /* 1.0 in Q16.16 */
    ASSERT_NEAR(q16_to_float(result), 1.0, 0.02);

    /* 1/sqrt(4.0) = 0.5 */
    result = pb_softmath_invsqrt_q16(0x40000);  /* 4.0 in Q16.16 */
    ASSERT_NEAR(q16_to_float(result), 0.5, 0.02);

    /* 1/sqrt(0.25) = 2.0 */
    result = pb_softmath_invsqrt_q16(0x4000);   /* 0.25 in Q16.16 */
    ASSERT_NEAR(q16_to_float(result), 2.0, 0.05);
}

TEST(test_nr_sqrt_q16) {
    int32_t result;

    /* sqrt(1.0) = 1.0 */
    result = pb_softmath_sqrt_q16(0x10000);
    ASSERT_NEAR(q16_to_float(result), 1.0, 0.02);

    /* sqrt(4.0) = 2.0 */
    result = pb_softmath_sqrt_q16(0x40000);
    ASSERT_NEAR(q16_to_float(result), 2.0, 0.03);

    /* sqrt(2.0) ≈ 1.414 */
    result = pb_softmath_sqrt_q16(0x20000);
    ASSERT_NEAR(q16_to_float(result), 1.414, 0.03);

    /* sqrt(0.25) = 0.5 */
    result = pb_softmath_sqrt_q16(0x4000);
    ASSERT_NEAR(q16_to_float(result), 0.5, 0.02);
}

/* ============================================================================
 * Integer Division Tests
 * ============================================================================ */

TEST(test_softdiv_basic) {
    uint32_t result;

    result = pb_softmath_div32(100, 10);
    ASSERT(result == 10);

    result = pb_softmath_div32(1000, 7);
    ASSERT(result == 142);  /* 1000/7 = 142.857... truncated */

    result = pb_softmath_div32(0xFFFFFFFF, 2);
    ASSERT(result == 0x7FFFFFFF);
}

TEST(test_softdiv_signed) {
    int32_t result;

    result = pb_softmath_sdiv32(100, 10);
    ASSERT(result == 10);

    result = pb_softmath_sdiv32(-100, 10);
    ASSERT(result == -10);

    result = pb_softmath_sdiv32(100, -10);
    ASSERT(result == -10);

    result = pb_softmath_sdiv32(-100, -10);
    ASSERT(result == 10);
}

/* ============================================================================
 * Exponential and Logarithm Tests
 * ============================================================================ */

TEST(test_exp_q16_basic) {
    int32_t result;

    /* exp(0) = 1.0 */
    result = pb_softmath_exp_q16(0);
    ASSERT_NEAR(q16_to_float(result), 1.0, 0.01);

    /* exp(1) ≈ 2.718 */
    result = pb_softmath_exp_q16(0x10000);  /* 1.0 in Q16.16 */
    ASSERT_NEAR(q16_to_float(result), 2.718, 0.1);

    /* exp(-1) ≈ 0.368 */
    result = pb_softmath_exp_q16(-0x10000);
    ASSERT_NEAR(q16_to_float(result), 0.368, 0.05);

    /* exp(2) ≈ 7.389 */
    result = pb_softmath_exp_q16(0x20000);
    ASSERT_NEAR(q16_to_float(result), 7.389, 0.2);
}

TEST(test_ln_q16_basic) {
    int32_t result;

    /* ln(1) = 0 */
    result = pb_softmath_ln_q16(0x10000);
    ASSERT_NEAR(q16_to_float(result), 0.0, 0.02);

    /* ln(e) ≈ 1.0 (e ≈ 2.718 = 0x2B7E1 in Q16.16) */
    result = pb_softmath_ln_q16(0x2B7E1);
    ASSERT_NEAR(q16_to_float(result), 1.0, 0.1);

    /* ln(2) ≈ 0.693 */
    result = pb_softmath_ln_q16(0x20000);  /* 2.0 in Q16.16 */
    ASSERT_NEAR(q16_to_float(result), 0.693, 0.05);

    /* ln(0.5) ≈ -0.693 */
    result = pb_softmath_ln_q16(0x8000);  /* 0.5 in Q16.16 */
    ASSERT_NEAR(q16_to_float(result), -0.693, 0.1);
}

/* ============================================================================
 * Angle Conversion Tests
 * ============================================================================ */

TEST(test_angle_conversions) {
    pb_brad16_t brad;

    /* Degrees to brads */
    brad = PB_DEG_TO_BRAD16(0);
    ASSERT(brad == 0);

    brad = PB_DEG_TO_BRAD16(90);
    ASSERT(brad == 16384);

    brad = PB_DEG_TO_BRAD16(180);
    ASSERT(brad == 32767 || brad == -32768);  /* Both valid representations */

    brad = PB_DEG_TO_BRAD16(45);
    ASSERT(brad == 8192);

    /* Brads to degrees (verify round-trip) */
    ASSERT(PB_BRAD16_TO_DEG(16384) == 90);
    ASSERT(PB_BRAD16_TO_DEG(8192) == 45);
}

/* ============================================================================
 * Edge Cases and Boundary Tests
 * ============================================================================ */

TEST(test_edge_division_by_zero) {
    uint32_t u_result;
    int32_t s_result;

    /* Division by zero should saturate */
    u_result = pb_softmath_div32(100, 0);
    ASSERT(u_result == 0xFFFFFFFFu);

    s_result = pb_softmath_sdiv32(100, 0);
    ASSERT(s_result == 0x7FFFFFFF);

    s_result = pb_softmath_sdiv32(-100, 0);
    ASSERT(s_result == (int32_t)0x80000000);
}

TEST(test_edge_sqrt_negative) {
    int32_t result;

    /* sqrt of negative should return 0 */
    result = pb_softmath_sqrt_q16(-0x10000);
    ASSERT(result == 0);

    result = pb_softmath_sqrt_q16(0);
    ASSERT(result == 0);
}

TEST(test_edge_ln_invalid) {
    int32_t result;

    /* ln(0) should return MIN */
    result = pb_softmath_ln_q16(0);
    ASSERT(result == (int32_t)0x80000000);

    /* ln(negative) should return MIN */
    result = pb_softmath_ln_q16(-0x10000);
    ASSERT(result == (int32_t)0x80000000);
}

TEST(test_edge_exp_overflow) {
    int32_t result;

    /* Large positive exp should saturate */
    result = pb_softmath_exp_q16(15 << 16);  /* exp(15) ≈ 3.3M, overflow */
    ASSERT(result == 0x7FFFFFFF);

    /* Large negative exp should underflow to 0 */
    result = pb_softmath_exp_q16(-(15 << 16));  /* -15 in Q16.16 */
    ASSERT(result == 0);
}

/* ============================================================================
 * Stress Tests - Full Circle Sweep
 * ============================================================================ */

TEST(test_cordic_full_circle_sweep) {
    int16_t s, c;
    int i;

    /* Sweep through all angles in 1-degree increments */
    for (i = 0; i < 360; i++) {
        pb_brad16_t brad = PB_DEG_TO_BRAD16(i);
        double expected_sin = sin(i * 3.14159265358979 / 180.0);
        double expected_cos = cos(i * 3.14159265358979 / 180.0);

        pb_softmath_sincos_brad16(brad, &s, &c);

        /* Allow 0.5% error for CORDIC */
        double sin_val = q15_to_float(s);
        double cos_val = q15_to_float(c);

        if (fabs(sin_val - expected_sin) > 0.005) {
            printf("FAIL: sin(%d deg) = %.4f, expected %.4f\n",
                   i, sin_val, expected_sin);
            exit(1);
        }
        if (fabs(cos_val - expected_cos) > 0.005) {
            printf("FAIL: cos(%d deg) = %.4f, expected %.4f\n",
                   i, cos_val, expected_cos);
            exit(1);
        }
    }
}

TEST(test_cordic_pythagorean_identity) {
    int16_t s, c;
    int i;

    /* sin²(x) + cos²(x) = 1 for all angles */
    for (i = 0; i < 360; i += 15) {
        pb_brad16_t brad = PB_DEG_TO_BRAD16(i);
        pb_softmath_sincos_brad16(brad, &s, &c);

        double sin_val = q15_to_float(s);
        double cos_val = q15_to_float(c);
        double sum = sin_val * sin_val + cos_val * cos_val;

        /* Should be very close to 1.0 */
        if (fabs(sum - 1.0) > 0.01) {
            printf("FAIL: sin²(%d) + cos²(%d) = %.4f, expected 1.0\n",
                   i, i, sum);
            exit(1);
        }
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("pb_softmath test suite\n");
    printf("======================\n\n");

    printf("CORDIC Trigonometry:\n");
    RUN_TEST(test_cordic_sin_cos_quadrants);
    RUN_TEST(test_cordic_sin_cos_45deg);
    RUN_TEST(test_cordic_sin_cos_30_60);
    RUN_TEST(test_cordic_sin_q16);
    RUN_TEST(test_cordic_cos_q16);

    printf("\nCORDIC atan2:\n");
    RUN_TEST(test_cordic_atan2_quadrants);
    RUN_TEST(test_cordic_atan2_45deg);

    printf("\nNewton-Raphson Operations:\n");
    RUN_TEST(test_nr_recip32_powers_of_2);
    RUN_TEST(test_nr_invsqrt_q16);
    RUN_TEST(test_nr_sqrt_q16);

    printf("\nInteger Division:\n");
    RUN_TEST(test_softdiv_basic);
    RUN_TEST(test_softdiv_signed);

    printf("\nExponential and Logarithm:\n");
    RUN_TEST(test_exp_q16_basic);
    RUN_TEST(test_ln_q16_basic);

    printf("\nAngle Conversions:\n");
    RUN_TEST(test_angle_conversions);

    printf("\nEdge Cases:\n");
    RUN_TEST(test_edge_division_by_zero);
    RUN_TEST(test_edge_sqrt_negative);
    RUN_TEST(test_edge_ln_invalid);
    RUN_TEST(test_edge_exp_overflow);

    printf("\nStress Tests:\n");
    RUN_TEST(test_cordic_full_circle_sweep);
    RUN_TEST(test_cordic_pythagorean_identity);

    printf("\n======================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

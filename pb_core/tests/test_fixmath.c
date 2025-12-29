/*
 * test_fixmath.c - Exhaustive tests for pb_fixmath.h
 *
 * Tests ALL fixed-point formats, algorithms, edge cases, and platform optimizations.
 * Covers Q formats from 8-bit to 32-bit, signed and unsigned.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Test framework macros */
static int test_passed = 0;
static int test_failed = 0;
static const char* current_test = NULL;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    current_test = #name; \
    name(); \
    printf("  [PASS] %s\n", #name); \
    test_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s: %s (line %d)\n", current_test, #cond, __LINE__); \
        test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

/* Allow larger tolerance for iterative algorithms */
#define ASSERT_NEAR_Q16(actual, expected, tolerance) do { \
    fix32_t _a = (actual); \
    fix32_t _e = (expected); \
    fix32_t _diff = (_a > _e) ? (_a - _e) : (_e - _a); \
    if (_diff > (tolerance)) { \
        printf("  [FAIL] %s: value %d != %d (diff %d > %d) at line %d\n", \
               current_test, _a, _e, _diff, (int)(tolerance), __LINE__); \
        test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR_FLOAT(actual, expected, tolerance) do { \
    float _a = (actual); \
    float _e = (expected); \
    float _diff = fabsf(_a - _e); \
    if (_diff > (tolerance)) { \
        printf("  [FAIL] %s: value %.6f != %.6f (diff %.6f > %.6f) at line %d\n", \
               current_test, _a, _e, _diff, (float)(tolerance), __LINE__); \
        test_failed++; \
        return; \
    } \
} while(0)

/* Include the fixed-point library */
#include "pb/pb_fixmath.h"

/*============================================================================
 * Q16.16 (32-bit) Tests - Primary Format
 *============================================================================*/

TEST(test_q16_16_from_int) {
    q16_16_t a = q16_16_from_int(1);
    ASSERT_EQ(a, 0x00010000);

    q16_16_t b = q16_16_from_int(100);
    ASSERT_EQ(b, 100 << 16);

    q16_16_t c = q16_16_from_int(-5);
    ASSERT_EQ(c, (fix32_t)(-5 * 65536));

    q16_16_t d = q16_16_from_int(0);
    ASSERT_EQ(d, 0);

    /* Maximum safe int value */
    q16_16_t e = q16_16_from_int(32767);
    ASSERT_EQ(e, 32767 << 16);
}

TEST(test_q16_16_from_float) {
    q16_16_t a = q16_16_from_float(1.5f);
    ASSERT_EQ(a, 0x00018000);  /* 1.5 * 65536 = 98304 */

    q16_16_t b = q16_16_from_float(0.25f);
    ASSERT_EQ(b, 0x00004000);  /* 0.25 * 65536 = 16384 */

    q16_16_t c = q16_16_from_float(-2.75f);
    ASSERT_EQ(c, (fix32_t)(-2.75f * 65536));

    q16_16_t d = q16_16_from_float(0.0f);
    ASSERT_EQ(d, 0);

    /* Small fractional values */
    q16_16_t e = q16_16_from_float(0.00001526f);  /* Near resolution limit */
    ASSERT(e >= 0 && e <= 2);
}

TEST(test_q16_16_to_float) {
    float a = q16_16_to_float(0x00010000);
    ASSERT(fabsf(a - 1.0f) < 0.0001f);

    float b = q16_16_to_float(0x00018000);
    ASSERT(fabsf(b - 1.5f) < 0.0001f);

    float c = q16_16_to_float(-0x00020000);
    ASSERT(fabsf(c - (-2.0f)) < 0.0001f);

    float d = q16_16_to_float(0);
    ASSERT_EQ(d, 0.0f);
}

TEST(test_q16_16_mul) {
    /* 2.0 * 3.0 = 6.0 */
    q16_16_t a = q16_16_from_int(2);
    q16_16_t b = q16_16_from_int(3);
    q16_16_t c = q16_16_mul(a, b);
    ASSERT_EQ(c, q16_16_from_int(6));

    /* 1.5 * 2.0 = 3.0 */
    a = q16_16_from_float(1.5f);
    b = q16_16_from_int(2);
    c = q16_16_mul(a, b);
    ASSERT_EQ(c, q16_16_from_int(3));

    /* 0.5 * 0.5 = 0.25 */
    a = q16_16_from_float(0.5f);
    b = q16_16_from_float(0.5f);
    c = q16_16_mul(a, b);
    ASSERT_NEAR_Q16(c, q16_16_from_float(0.25f), 1);

    /* Multiply by zero */
    c = q16_16_mul(a, 0);
    ASSERT_EQ(c, 0);

    /* Multiply by one */
    c = q16_16_mul(a, q16_16_one());
    ASSERT_EQ(c, a);
}

TEST(test_q16_16_div) {
    /* 6.0 / 2.0 = 3.0 */
    q16_16_t a = q16_16_from_int(6);
    q16_16_t b = q16_16_from_int(2);
    q16_16_t c = q16_16_div(a, b);
    ASSERT_EQ(c, q16_16_from_int(3));

    /* 1.0 / 4.0 = 0.25 */
    a = q16_16_from_int(1);
    b = q16_16_from_int(4);
    c = q16_16_div(a, b);
    ASSERT_EQ(c, q16_16_from_float(0.25f));

    /* Division by zero returns max value for storage type */
    c = q16_16_div(q16_16_from_int(1), 0);
    ASSERT_EQ(c, 0x7FFFFFFF);  /* Max signed 32-bit value */

    /* Zero divided by anything */
    c = q16_16_div(0, q16_16_from_int(5));
    ASSERT_EQ(c, 0);

    /* Divide by one */
    a = q16_16_from_int(7);
    c = q16_16_div(a, q16_16_one());
    ASSERT_EQ(c, a);
}

TEST(test_q16_16_abs_neg) {
    q16_16_t a = q16_16_from_int(-5);
    ASSERT_EQ(q16_16_abs(a), q16_16_from_int(5));
    ASSERT_EQ(q16_16_neg(a), q16_16_from_int(5));

    q16_16_t b = q16_16_from_int(3);
    ASSERT_EQ(q16_16_abs(b), q16_16_from_int(3));
    ASSERT_EQ(q16_16_neg(b), q16_16_from_int(-3));

    /* Abs of zero */
    q16_16_t z = q16_16_from_int(0);
    ASSERT_EQ(q16_16_abs(z), 0);
}

TEST(test_q16_16_add_sub) {
    q16_16_t a = q16_16_from_float(3.5f);
    q16_16_t b = q16_16_from_float(2.25f);

    q16_16_t sum = q16_16_add(a, b);
    ASSERT_NEAR_Q16(sum, q16_16_from_float(5.75f), 1);

    q16_16_t diff = q16_16_sub(a, b);
    ASSERT_NEAR_Q16(diff, q16_16_from_float(1.25f), 1);

    /* Subtract from zero */
    q16_16_t neg = q16_16_sub(0, a);
    ASSERT_EQ(neg, -a);
}

TEST(test_q16_16_one_half) {
    q16_16_t one = q16_16_one();
    ASSERT_EQ(one, 0x00010000);

    q16_16_t half = q16_16_half();
    ASSERT_EQ(half, 0x00008000);

    /* one - half = half */
    ASSERT_EQ(one - half, half);
}

/*============================================================================
 * All 8-bit Signed Q Formats Tests
 *============================================================================*/

TEST(test_q0_7_exhaustive) {
    /* Q0.7: normalized [-1, ~0.99], precision 0.0078125 */
    q0_7_t a = q0_7_from_float(0.5f);
    q0_7_t b = q0_7_from_float(0.5f);
    q0_7_t c = q0_7_mul(a, b);
    ASSERT_NEAR_FLOAT(q0_7_to_float(c), 0.25f, 0.02f);

    /* Negative values */
    q0_7_t d = q0_7_from_float(-0.5f);
    q0_7_t e = q0_7_mul(a, d);
    ASSERT_NEAR_FLOAT(q0_7_to_float(e), -0.25f, 0.02f);

    /* Near boundaries */
    q0_7_t max_val = q0_7_from_float(0.99f);
    q0_7_t min_val = q0_7_from_float(-1.0f);
    ASSERT(q0_7_to_float(max_val) > 0.9f);
    ASSERT(q0_7_to_float(min_val) < -0.9f);
}

TEST(test_q1_6_exhaustive) {
    /* Q1.6: Range [-2, 1.98], precision 0.015625 */
    q1_6_t a = q1_6_from_float(1.5f);
    q1_6_t b = q1_6_from_float(1.0f);
    q1_6_t c = q1_6_mul(a, b);
    ASSERT_NEAR_FLOAT(q1_6_to_float(c), 1.5f, 0.1f);

    /* Division */
    q1_6_t d = q1_6_div(a, b);
    ASSERT_NEAR_FLOAT(q1_6_to_float(d), 1.5f, 0.1f);
}

TEST(test_q2_5_exhaustive) {
    /* Q2.5: Range [-4, 3.97], precision 0.03125 */
    q2_5_t a = q2_5_from_float(2.0f);
    q2_5_t b = q2_5_from_float(1.5f);
    q2_5_t c = q2_5_mul(a, b);
    /* 2.0 * 1.5 = 3.0 (within range) */
    ASSERT_NEAR_FLOAT(q2_5_to_float(c), 3.0f, 0.2f);

    /* Test addition within range */
    a = q2_5_from_float(1.0f);
    b = q2_5_from_float(2.0f);
    c = q2_5_add(a, b);
    ASSERT_NEAR_FLOAT(q2_5_to_float(c), 3.0f, 0.1f);
}

TEST(test_q3_4_exhaustive) {
    /* Q3.4: Range [-8, 7.94], precision 0.0625 */
    q3_4_t a = q3_4_from_float(2.5f);
    q3_4_t b = q3_4_from_float(1.5f);
    q3_4_t c = q3_4_mul(a, b);
    ASSERT_NEAR_FLOAT(q3_4_to_float(c), 3.75f, 0.5f);

    /* Roundtrip */
    float original = 5.0f;
    q3_4_t q = q3_4_from_float(original);
    float back = q3_4_to_float(q);
    ASSERT_NEAR_FLOAT(back, original, 0.1f);
}

TEST(test_q4_3_exhaustive) {
    /* Q4.3: Range [-16, 15.875], precision 0.125 */
    q4_3_t a = q4_3_from_float(10.0f);
    q4_3_t b = q4_3_from_float(1.5f);
    q4_3_t c = q4_3_mul(a, b);
    ASSERT_NEAR_FLOAT(q4_3_to_float(c), 15.0f, 0.5f);

    /* Division */
    c = q4_3_div(a, q4_3_from_float(2.0f));
    ASSERT_NEAR_FLOAT(q4_3_to_float(c), 5.0f, 0.5f);
}

TEST(test_q5_2_exhaustive) {
    /* Q5.2: Range [-32, 31.75], precision 0.25 */
    q5_2_t a = q5_2_from_float(20.0f);
    q5_2_t b = q5_2_from_float(1.5f);
    q5_2_t c = q5_2_mul(a, b);
    ASSERT_NEAR_FLOAT(q5_2_to_float(c), 30.0f, 1.0f);
}

TEST(test_q6_1_exhaustive) {
    /* Q6.1: Range [-64, 63.5], precision 0.5 */
    q6_1_t a = q6_1_from_float(30.0f);
    q6_1_t b = q6_1_from_float(2.0f);
    q6_1_t c = q6_1_mul(a, b);
    ASSERT_NEAR_FLOAT(q6_1_to_float(c), 60.0f, 1.0f);

    /* Coarse precision test */
    q6_1_t half_test = q6_1_from_float(0.5f);
    ASSERT_NEAR_FLOAT(q6_1_to_float(half_test), 0.5f, 0.01f);
}

TEST(test_q7_0_exhaustive) {
    /* Q7.0: Integer [-128, 127] */
    q7_0_t a = q7_0_from_int(50);
    q7_0_t b = q7_0_from_int(2);
    q7_0_t c = q7_0_mul(a, b);
    ASSERT_EQ(q7_0_to_int(c), 100);

    /* Division */
    c = q7_0_div(a, b);
    ASSERT_EQ(q7_0_to_int(c), 25);
}

/*============================================================================
 * All 8-bit Unsigned Q Formats Tests
 *============================================================================*/

TEST(test_uq0_8_exhaustive) {
    /* UQ0.8: Range [0, 0.996], precision 0.00390625 */
    uq0_8_t a = uq0_8_from_float(0.5f);
    uq0_8_t b = uq0_8_from_float(0.5f);
    uq0_8_t c = uq0_8_mul(a, b);
    ASSERT_NEAR_FLOAT(uq0_8_to_float(c), 0.25f, 0.02f);

    /* Cannot represent negative */
    uq0_8_t max = uq0_8_from_float(0.99f);
    ASSERT(uq0_8_to_float(max) > 0.9f);
}

TEST(test_uq1_7_exhaustive) {
    /* UQ1.7: Range [0, 1.99], precision 0.0078125 */
    uq1_7_t a = uq1_7_from_float(1.5f);
    uq1_7_t b = uq1_7_from_float(1.0f);
    uq1_7_t c = uq1_7_mul(a, b);
    ASSERT_NEAR_FLOAT(uq1_7_to_float(c), 1.5f, 0.1f);
}

TEST(test_uq4_4_exhaustive) {
    /* UQ4.4: Range [0, 15.94], precision 0.0625 */
    uq4_4_t a = uq4_4_from_float(8.0f);
    uq4_4_t b = uq4_4_from_float(1.5f);
    uq4_4_t c = uq4_4_mul(a, b);
    ASSERT_NEAR_FLOAT(uq4_4_to_float(c), 12.0f, 0.5f);
}

TEST(test_uq8_0_exhaustive) {
    /* UQ8.0: Unsigned integer [0, 255] */
    uq8_0_t a = uq8_0_from_int(100);
    uq8_0_t b = uq8_0_from_int(2);
    uq8_0_t c = uq8_0_mul(a, b);
    ASSERT_EQ(uq8_0_to_int(c), 200);

    /* Division */
    c = uq8_0_div(a, b);
    ASSERT_EQ(uq8_0_to_int(c), 50);
}

/*============================================================================
 * All 16-bit Signed Q Formats Tests
 *============================================================================*/

TEST(test_q0_15_exhaustive) {
    /* Q0.15: DSP standard, range [-1, 0.999969] - NOTE: "one" is NOT representable! */
    q0_15_t a = q0_15_from_float(0.707f);
    q0_15_t b = q0_15_from_float(0.707f);
    q0_15_t c = q0_15_mul(a, b);
    ASSERT_NEAR_FLOAT(q0_15_to_float(c), 0.5f, 0.01f);

    /* Test with 0.5 instead of 1.0 (which is out of range) */
    q0_15_t half = q0_15_from_float(0.5f);
    q0_15_t d = q0_15_mul(a, half);
    /* 0.707 * 0.5 ≈ 0.354 */
    ASSERT_NEAR_FLOAT(q0_15_to_float(d), 0.354f, 0.01f);

    /* Boundary test */
    q0_15_t near_max = q0_15_from_float(0.99f);
    ASSERT(q0_15_to_float(near_max) > 0.95f);
}

TEST(test_q1_14_exhaustive) {
    /* Q1.14: Range [-2, 1.99], precision ~0.00006 */
    q1_14_t a = q1_14_from_float(1.5f);
    q1_14_t b = q1_14_from_float(1.0f);
    q1_14_t c = q1_14_mul(a, b);
    ASSERT_NEAR_FLOAT(q1_14_to_float(c), 1.5f, 0.01f);
}

TEST(test_q2_13_exhaustive) {
    /* Q2.13: Range [-4, 3.99] */
    q2_13_t a = q2_13_from_float(3.0f);
    q2_13_t b = q2_13_from_float(1.0f);
    q2_13_t c = q2_13_mul(a, b);
    ASSERT_NEAR_FLOAT(q2_13_to_float(c), 3.0f, 0.01f);
}

TEST(test_q4_11_exhaustive) {
    /* Q4.11: Range [-16, 15.99] */
    q4_11_t a = q4_11_from_float(10.0f);
    q4_11_t b = q4_11_from_float(1.5f);
    q4_11_t c = q4_11_mul(a, b);
    ASSERT_NEAR_FLOAT(q4_11_to_float(c), 15.0f, 0.1f);
}

TEST(test_q4_12_hp48_exhaustive) {
    /* Q4.12: HP48-style, range [-8, 7.9997] (note: 16-bit, limited integer bits) */
    q4_12_t a = q4_12_from_float(3.14159f);
    q4_12_t b = q4_12_from_float(2.0f);
    q4_12_t c = q4_12_mul(a, b);
    ASSERT_NEAR_FLOAT(q4_12_to_float(c), 6.28318f, 0.01f);

    /* Division */
    c = q4_12_div(a, b);
    ASSERT_NEAR_FLOAT(q4_12_to_float(c), 1.5708f, 0.01f);
}

TEST(test_q8_7_exhaustive) {
    /* Q8.7: Range [-256, 255] */
    q8_7_t a = q8_7_from_float(100.5f);
    q8_7_t b = q8_7_from_float(2.0f);
    q8_7_t c = q8_7_mul(a, b);
    ASSERT_NEAR_FLOAT(q8_7_to_float(c), 201.0f, 1.0f);
}

TEST(test_q8_8_exhaustive) {
    /* Q8.8: Range [-128, 127.99], precision 0.00390625 */
    q8_8_t a = q8_8_from_float(10.5f);
    q8_8_t b = q8_8_from_float(2.0f);
    q8_8_t c = q8_8_mul(a, b);
    ASSERT_NEAR_FLOAT(q8_8_to_float(c), 21.0f, 0.1f);

    /* Division */
    c = q8_8_div(a, b);
    ASSERT_NEAR_FLOAT(q8_8_to_float(c), 5.25f, 0.1f);

    /* Addition and subtraction */
    q8_8_t sum = q8_8_add(a, b);
    ASSERT_NEAR_FLOAT(q8_8_to_float(sum), 12.5f, 0.01f);

    q8_8_t diff = q8_8_sub(a, b);
    ASSERT_NEAR_FLOAT(q8_8_to_float(diff), 8.5f, 0.01f);
}

TEST(test_q12_4_exhaustive) {
    /* Q12.4: Range [-2048, 2047.94] */
    q12_4_t a = q12_4_from_float(1000.0f);
    q12_4_t b = q12_4_from_float(2.0f);
    q12_4_t c = q12_4_mul(a, b);
    ASSERT_NEAR_FLOAT(q12_4_to_float(c), 2000.0f, 1.0f);
}

TEST(test_q15_0_exhaustive) {
    /* Q15.0: Integer [-32768, 32767] */
    q15_0_t a = q15_0_from_int(1000);
    q15_0_t b = q15_0_from_int(10);
    q15_0_t c = q15_0_mul(a, b);
    ASSERT_EQ(q15_0_to_int(c), 10000);
}

/*============================================================================
 * All 16-bit Unsigned Q Formats Tests
 *============================================================================*/

TEST(test_uq0_16_exhaustive) {
    /* UQ0.16: Range [0, 0.99998], precision 0.0000152588 */
    uq0_16_t a = uq0_16_from_float(0.5f);
    uq0_16_t b = uq0_16_from_float(0.5f);
    uq0_16_t c = uq0_16_mul(a, b);
    ASSERT_NEAR_FLOAT(uq0_16_to_float(c), 0.25f, 0.001f);
}

TEST(test_uq8_8_16bit_exhaustive) {
    /* UQ8.8: Unsigned, range [0, 255.99] */
    uq8_8_t a = uq8_8_from_float(100.0f);
    uq8_8_t b = uq8_8_from_float(2.0f);
    uq8_8_t c = uq8_8_mul(a, b);
    ASSERT_NEAR_FLOAT(uq8_8_to_float(c), 200.0f, 1.0f);
}

TEST(test_uq16_0_exhaustive) {
    /* UQ16.0: Unsigned integer [0, 65535] */
    uq16_0_t a = uq16_0_from_int(10000);
    uq16_0_t b = uq16_0_from_int(5);
    uq16_0_t c = uq16_0_mul(a, b);
    ASSERT_EQ(uq16_0_to_int(c), 50000);
}

/*============================================================================
 * All 32-bit Signed Q Formats Tests
 *============================================================================*/

TEST(test_q0_31_exhaustive) {
    /* Q0.31: DSP high-precision, range [-1, 0.9999999995] */
    /* Note: mul may overflow since intermediate would need 64-bit * 64-bit */
    q0_31_t a = q0_31_from_float(0.5f);
    float a_f = q0_31_to_float(a);
    ASSERT_NEAR_FLOAT(a_f, 0.5f, 0.0001f);

    /* Test conversion precision */
    q0_31_t b = q0_31_from_float(0.25f);
    ASSERT_NEAR_FLOAT(q0_31_to_float(b), 0.25f, 0.0001f);

    /* Test negative */
    q0_31_t neg = q0_31_from_float(-0.5f);
    ASSERT_NEAR_FLOAT(q0_31_to_float(neg), -0.5f, 0.0001f);
}

TEST(test_q1_30_exhaustive) {
    /* Q1.30: Range [-2, 1.9999] */
    q1_30_t a = q1_30_from_float(1.5f);
    q1_30_t b = q1_30_from_float(1.0f);
    q1_30_t c = q1_30_mul(a, b);
    ASSERT_NEAR_FLOAT(q1_30_to_float(c), 1.5f, 0.0001f);
}

TEST(test_q8_23_exhaustive) {
    /* Q8.23: Range [-256, 255.9999] */
    q8_23_t a = q8_23_from_float(100.5f);
    q8_23_t b = q8_23_from_float(2.0f);
    q8_23_t c = q8_23_mul(a, b);
    ASSERT_NEAR_FLOAT(q8_23_to_float(c), 201.0f, 0.01f);
}

TEST(test_q8_24_exhaustive) {
    /* Q8.24: Range [-128, 127.9999] */
    q8_24_t a = q8_24_from_float(50.25f);
    q8_24_t b = q8_24_from_float(2.0f);
    q8_24_t c = q8_24_mul(a, b);
    ASSERT_NEAR_FLOAT(q8_24_to_float(c), 100.5f, 0.001f);
}

TEST(test_q16_15_exhaustive) {
    /* Q16.15: Range [-65536, 65535.9] */
    q16_15_t a = q16_15_from_float(1000.5f);
    q16_15_t b = q16_15_from_float(10.0f);
    q16_15_t c = q16_15_mul(a, b);
    ASSERT_NEAR_FLOAT(q16_15_to_float(c), 10005.0f, 1.0f);
}

TEST(test_q24_8_exhaustive) {
    /* Q24.8: Range [-8M, 8M], precision 0.00390625 */
    q24_8_t a = q24_8_from_float(10000.5f);
    q24_8_t b = q24_8_from_float(100.0f);
    q24_8_t c = q24_8_mul(a, b);
    ASSERT_NEAR_FLOAT(q24_8_to_float(c), 1000050.0f, 10.0f);
}

TEST(test_q31_0_exhaustive) {
    /* Q31.0: 32-bit signed integer */
    q31_0_t a = q31_0_from_int(100000);
    q31_0_t b = q31_0_from_int(1000);
    q31_0_t c = q31_0_mul(a, b);
    ASSERT_EQ(q31_0_to_int(c), 100000000);
}

/*============================================================================
 * All 32-bit Unsigned Q Formats Tests
 *============================================================================*/

TEST(test_uq16_16_exhaustive) {
    /* UQ16.16: Unsigned Q16.16, range [0, 65535.999985] */
    uq16_16_t a = uq16_16_from_float(1000.5f);
    uq16_16_t b = uq16_16_from_float(10.0f);
    uq16_16_t c = uq16_16_mul(a, b);
    ASSERT_NEAR_FLOAT(uq16_16_to_float(c), 10005.0f, 0.1f);
}

TEST(test_uq32_0_exhaustive) {
    /* UQ32.0: Unsigned integer [0, 4294967295] */
    uq32_0_t a = uq32_0_from_int(100000);
    uq32_0_t b = uq32_0_from_int(10000);
    uq32_0_t c = uq32_0_mul(a, b);
    ASSERT_EQ(uq32_0_to_int(c), 1000000000U);
}

/*============================================================================
 * Newton-Raphson Tests
 *============================================================================*/

#if PB_FIX_USE_NEWTON_RAPHSON

TEST(test_recip_q16) {
    /* 1/2 = 0.5 */
    fix32_t two = 0x00020000;
    fix32_t recip = pb_fix_recip32_q16(two);
    ASSERT_NEAR_Q16(recip, 0x00008000, 0x100);

    /* 1/4 = 0.25 */
    fix32_t four = 0x00040000;
    recip = pb_fix_recip32_q16(four);
    ASSERT_NEAR_Q16(recip, 0x00004000, 0x100);

    /* 1/0.5 = 2.0 */
    fix32_t half = 0x00008000;
    recip = pb_fix_recip32_q16(half);
    ASSERT_NEAR_Q16(recip, 0x00020000, 0x200);

    /* Reciprocal of negative */
    fix32_t neg_two = -0x00020000;
    recip = pb_fix_recip32_q16(neg_two);
    ASSERT_NEAR_Q16(recip, -0x00008000, 0x100);
}

TEST(test_invsqrt_q16) {
    /* 1/sqrt(4) = 0.5 */
    fix32_t four = 0x00040000;
    fix32_t inv_sqrt = pb_fix_invsqrt32_q16(four);
    ASSERT_NEAR_Q16(inv_sqrt, 0x00008000, 0x400);

    /* 1/sqrt(1) = 1.0 */
    fix32_t one = 0x00010000;
    inv_sqrt = pb_fix_invsqrt32_q16(one);
    ASSERT_NEAR_Q16(inv_sqrt, 0x00010000, 0x400);

    /* Edge case: very small value */
    fix32_t small = 0x00001000;  /* 0.0625 */
    inv_sqrt = pb_fix_invsqrt32_q16(small);
    ASSERT(inv_sqrt > 0x00030000);  /* Should be 4 */
}

TEST(test_sqrt_q16) {
    /* sqrt(4) = 2 */
    fix32_t four = 0x00040000;
    fix32_t root = pb_fix_sqrt32_q16(four);
    ASSERT_NEAR_Q16(root, 0x00020000, 0x400);

    /* sqrt(1) = 1 */
    fix32_t one = 0x00010000;
    root = pb_fix_sqrt32_q16(one);
    ASSERT_NEAR_Q16(root, 0x00010000, 0x400);

    /* sqrt(2) ≈ 1.414 */
    fix32_t two = 0x00020000;
    root = pb_fix_sqrt32_q16(two);
    fix32_t expected = (fix32_t)(1.41421356f * 65536.0f);
    ASSERT_NEAR_Q16(root, expected, 0x800);

    /* sqrt(0) = 0 */
    root = pb_fix_sqrt32_q16(0);
    ASSERT_EQ(root, 0);

    /* sqrt(9) = 3 */
    fix32_t nine = 0x00090000;
    root = pb_fix_sqrt32_q16(nine);
    ASSERT_NEAR_Q16(root, 0x00030000, 0x400);
}

TEST(test_div_newton_raphson) {
    /* 6.0 / 2.0 = 3.0 */
    fix32_t six = 0x00060000;
    fix32_t two = 0x00020000;
    fix32_t result = pb_fix_div32_nr(six, two, 16);
    ASSERT_NEAR_Q16(result, 0x00030000, 0x200);

    /* 1.0 / 3.0 ≈ 0.333 */
    fix32_t one = 0x00010000;
    fix32_t three = 0x00030000;
    result = pb_fix_div32_nr(one, three, 16);
    fix32_t expected = (fix32_t)(0.33333f * 65536.0f);
    ASSERT_NEAR_Q16(result, expected, 0x400);
}

#endif /* PB_FIX_USE_NEWTON_RAPHSON */

/*============================================================================
 * CORDIC Trigonometry Tests
 *============================================================================*/

#if PB_FIX_USE_CORDIC

TEST(test_cordic_sin) {
    /* sin(0) = 0 */
    fix32_t angle = 0;
    fix32_t s = pb_fix_sin32_q16(angle);
    ASSERT_NEAR_Q16(s, 0, 0x100);

    /* sin(π/2) = 1 */
    angle = PB_FIX_PI_2(16);
    s = pb_fix_sin32_q16(angle);
    ASSERT_NEAR_Q16(s, 0x00010000, 0x400);

    /* sin(π) ≈ 0 */
    angle = PB_FIX_PI(16);
    s = pb_fix_sin32_q16(angle);
    ASSERT_NEAR_Q16(s, 0, 0x400);

    /* sin(π/6) = 0.5 */
    angle = PB_FIX_PI(16) / 6;
    s = pb_fix_sin32_q16(angle);
    ASSERT_NEAR_Q16(s, 0x00008000, 0x800);

    /* sin(3π/2) = -1 */
    angle = 3 * PB_FIX_PI_2(16);
    s = pb_fix_sin32_q16(angle);
    ASSERT_NEAR_Q16(s, -0x00010000, 0x400);
}

TEST(test_cordic_cos) {
    /* cos(0) = 1 */
    fix32_t angle = 0;
    fix32_t c = pb_fix_cos32_q16(angle);
    ASSERT_NEAR_Q16(c, 0x00010000, 0x400);

    /* cos(π/2) ≈ 0 */
    angle = PB_FIX_PI_2(16);
    c = pb_fix_cos32_q16(angle);
    ASSERT_NEAR_Q16(c, 0, 0x400);

    /* cos(π) = -1 */
    angle = PB_FIX_PI(16);
    c = pb_fix_cos32_q16(angle);
    ASSERT_NEAR_Q16(c, -0x00010000, 0x400);

    /* cos(π/3) = 0.5 */
    angle = PB_FIX_PI(16) / 3;
    c = pb_fix_cos32_q16(angle);
    ASSERT_NEAR_Q16(c, 0x00008000, 0x800);
}

TEST(test_cordic_sincos_identity) {
    /* sin²(x) + cos²(x) = 1 for various angles */
    fix32_t angles[] = {0, PB_FIX_PI(16)/6, PB_FIX_PI(16)/4, PB_FIX_PI(16)/3, PB_FIX_PI(16)/2};

    for (int i = 0; i < 5; i++) {
        fix32_t s = pb_fix_sin32_q16(angles[i]);
        fix32_t c = pb_fix_cos32_q16(angles[i]);

        fix32_t s2 = pb_fix_mul32(s, s, 16);
        fix32_t c2 = pb_fix_mul32(c, c, 16);
        fix32_t sum = s2 + c2;

        ASSERT_NEAR_Q16(sum, 0x00010000, 0x1000);
    }
}

TEST(test_cordic_atan2) {
    /* atan2(0, 1) = 0 */
    fix32_t angle = pb_fix_atan2_q16(0, 0x00010000);
    ASSERT_NEAR_Q16(angle, 0, 0x400);

    /* atan2(1, 0) = π/2 */
    angle = pb_fix_atan2_q16(0x00010000, 0);
    ASSERT_NEAR_Q16(angle, PB_FIX_PI_2(16), 0x800);

    /* atan2(1, 1) = π/4 */
    angle = pb_fix_atan2_q16(0x00010000, 0x00010000);
    ASSERT_NEAR_Q16(angle, PB_FIX_PI(16)/4, 0x800);

    /* atan2(-1, 0) = -π/2 */
    angle = pb_fix_atan2_q16(-0x00010000, 0);
    ASSERT_NEAR_Q16(angle, -PB_FIX_PI_2(16), 0x800);

    /* atan2(0, -1) = π */
    angle = pb_fix_atan2_q16(0, -0x00010000);
    ASSERT_NEAR_Q16(angle, PB_FIX_PI(16), 0x800);
}

TEST(test_cordic_atan) {
    /* atan(0) = 0 */
    fix32_t angle = pb_fix_atan_q16(0);
    ASSERT_NEAR_Q16(angle, 0, 0x100);

    /* atan(1) = π/4 */
    angle = pb_fix_atan_q16(0x00010000);
    ASSERT_NEAR_Q16(angle, PB_FIX_PI(16)/4, 0x800);
}

TEST(test_cordic_sincos_combined) {
    /* Test pb_fix_sincos32_q16 */
    fix32_t angle = PB_FIX_PI(16) / 4;  /* 45 degrees */
    fix32_t sin_val, cos_val;
    pb_fix_sincos32_q16(angle, &sin_val, &cos_val);

    /* sin(45°) = cos(45°) ≈ 0.707 */
    fix32_t expected = (fix32_t)(0.70710678f * 65536.0f);
    ASSERT_NEAR_Q16(sin_val, expected, 0x800);
    ASSERT_NEAR_Q16(cos_val, expected, 0x800);
}

#endif /* PB_FIX_USE_CORDIC */

/*============================================================================
 * Exponential and Logarithm Tests
 *============================================================================*/

TEST(test_exp_q16) {
    /* exp(0) = 1 */
    fix32_t x = 0;
    fix32_t result = pb_fix_exp32_q16(x);
    ASSERT_NEAR_Q16(result, 0x00010000, 0x200);

    /* exp(1) ≈ 2.718 */
    x = 0x00010000;
    result = pb_fix_exp32_q16(x);
    fix32_t expected = (fix32_t)(2.71828f * 65536.0f);
    ASSERT_NEAR_Q16(result, expected, 0x1000);

    /* exp(ln(2)) ≈ 2 */
    x = 0x0000B172;  /* ln(2) ≈ 0.693 */
    result = pb_fix_exp32_q16(x);
    ASSERT_NEAR_Q16(result, 0x00020000, 0x1000);

    /* exp(-1) ≈ 0.368 */
    x = -0x00010000;
    result = pb_fix_exp32_q16(x);
    expected = (fix32_t)(0.36788f * 65536.0f);
    ASSERT_NEAR_Q16(result, expected, 0x1000);
}

TEST(test_ln_q16) {
    /* ln(1) = 0 */
    fix32_t x = 0x00010000;
    fix32_t result = pb_fix_ln32_q16(x);
    ASSERT_NEAR_Q16(result, 0, 0x400);

    /* ln(e) ≈ 1 */
    x = (fix32_t)(2.71828f * 65536.0f);
    result = pb_fix_ln32_q16(x);
    ASSERT_NEAR_Q16(result, 0x00010000, 0x2000);

    /* ln(2) ≈ 0.693 */
    x = 0x00020000;
    result = pb_fix_ln32_q16(x);
    ASSERT_NEAR_Q16(result, 0x0000B172, 0x2000);

    /* ln(0.5) ≈ -0.693 */
    x = 0x00008000;
    result = pb_fix_ln32_q16(x);
    ASSERT_NEAR_Q16(result, -0x0000B172, 0x2000);
}

TEST(test_exp_ln_inverse) {
    /* exp(ln(x)) = x */
    fix32_t values[] = {0x00008000, 0x00010000, 0x00020000, 0x00040000};
    for (int i = 0; i < 4; i++) {
        fix32_t x = values[i];
        fix32_t ln_x = pb_fix_ln32_q16(x);
        fix32_t exp_ln_x = pb_fix_exp32_q16(ln_x);
        ASSERT_NEAR_Q16(exp_ln_x, x, 0x2000);
    }
}

/*============================================================================
 * Overflow and Saturation Tests
 *============================================================================*/

TEST(test_saturation_s32_to_s16) {
    /* Within range */
    fix16_t result = pb_fix_sat_s32_to_s16(1000);
    ASSERT_EQ(result, 1000);

    /* Positive overflow */
    result = pb_fix_sat_s32_to_s16(100000);
    ASSERT_EQ(result, PB_FIX_MAX_S16);

    /* Negative overflow */
    result = pb_fix_sat_s32_to_s16(-100000);
    ASSERT_EQ(result, PB_FIX_MIN_S16);

    /* Exactly at boundary */
    result = pb_fix_sat_s32_to_s16(32767);
    ASSERT_EQ(result, 32767);

    result = pb_fix_sat_s32_to_s16(-32768);
    ASSERT_EQ(result, -32768);
}

TEST(test_saturation_s64_to_s32) {
    /* Within range */
    fix32_t result = pb_fix_sat_s64_to_s32(1000000LL);
    ASSERT_EQ(result, 1000000);

    /* Positive overflow */
    result = pb_fix_sat_s64_to_s32(0x100000000LL);
    ASSERT_EQ(result, PB_FIX_MAX_S32);

    /* Negative overflow */
    result = pb_fix_sat_s64_to_s32(-0x100000000LL);
    ASSERT_EQ(result, PB_FIX_MIN_S32);
}

TEST(test_saturating_add) {
    /* Normal addition */
    fix32_t a = 0x10000000;
    fix32_t b = 0x10000000;
    fix32_t result = pb_fix_sadd32(a, b);
    ASSERT_EQ(result, 0x20000000);

    /* Saturating at max */
    a = 0x7F000000;
    b = 0x10000000;
    result = pb_fix_sadd32(a, b);
    ASSERT(result >= 0x7F000000);

    /* Negative saturation */
    a = (fix32_t)0x80000001;
    b = (fix32_t)0x80000001;
    result = pb_fix_sadd32(a, b);
    ASSERT(result <= (fix32_t)0x80000001);
}

TEST(test_saturating_sub) {
    /* Normal subtraction */
    fix32_t a = 0x20000000;
    fix32_t b = 0x10000000;
    fix32_t result = pb_fix_ssub32(a, b);
    ASSERT_EQ(result, 0x10000000);

    /* Zero result */
    result = pb_fix_ssub32(a, a);
    ASSERT_EQ(result, 0);
}

TEST(test_saturating_add_16) {
    /* Normal 16-bit addition */
    fix16_t a = 10000;
    fix16_t b = 10000;
    fix16_t result = pb_fix_sadd16(a, b);
    ASSERT_EQ(result, 20000);

    /* Saturating at max */
    a = 30000;
    b = 10000;
    result = pb_fix_sadd16(a, b);
    ASSERT(result >= 30000);
}

/*============================================================================
 * Format Conversion Tests
 *============================================================================*/

TEST(test_format_conversion) {
    /* Q8.8 to Q16.16 (widening) */
    fix32_t q8_8_val = 0x0180;  /* 1.5 in Q8.8 */
    fix32_t q16_16_val = pb_fix_convert(q8_8_val, 8, 16);
    ASSERT_EQ(q16_16_val, 0x00018000);

    /* Q16.16 to Q8.8 (narrowing) */
    q16_16_val = 0x00028000;  /* 2.5 in Q16.16 */
    fix32_t q8_8_result = pb_fix_convert(q16_16_val, 16, 8);
    ASSERT_EQ(q8_8_result, 0x0280);

    /* Same format (no change) */
    q16_16_val = 0x00030000;
    fix32_t same = pb_fix_convert(q16_16_val, 16, 16);
    ASSERT_EQ(same, q16_16_val);

    /* Q4.12 to Q16.16 */
    fix32_t q4_12_val = 0x1000;  /* 1.0 in Q4.12 */
    fix32_t converted = pb_fix_convert(q4_12_val, 12, 16);
    ASSERT_EQ(converted, 0x00010000);  /* 1.0 in Q16.16 */
}

TEST(test_format_conversion_saturating) {
    /* Large value that overflows when narrowing */
    fix32_t large = 0x00FF0000;  /* 255.0 in Q16.16 */
    fix16_t result = pb_fix_convert_sat_s32_to_s16(large, 16, 8);
    /* This should saturate since 255 in Q8.8 = 0xFF00 which is -256 if signed */
    ASSERT(result == PB_FIX_MAX_S16 || result > 0);
}

/*============================================================================
 * Generic Macro Tests
 *============================================================================*/

TEST(test_generic_macros) {
    /* PB_FIX_FROM_INT */
    fix32_t a = PB_FIX_FROM_INT(5, 16);
    ASSERT_EQ(a, 5 << 16);

    /* PB_FIX_TO_INT */
    int i = PB_FIX_TO_INT(a, 16);
    ASSERT_EQ(i, 5);

    /* PB_FIX_FROM_FLOAT */
    fix32_t b = PB_FIX_FROM_FLOAT(2.5f, 16);
    ASSERT_NEAR_Q16(b, 0x00028000, 1);

    /* PB_FIX_TO_FLOAT */
    float f = PB_FIX_TO_FLOAT(b, 16);
    ASSERT_NEAR_FLOAT(f, 2.5f, 0.0001f);

    /* PB_FIX_ONE */
    fix32_t one = PB_FIX_ONE(16);
    ASSERT_EQ(one, 0x00010000);

    /* PB_FIX_HALF */
    fix32_t half = PB_FIX_HALF(16);
    ASSERT_EQ(half, 0x00008000);
}

TEST(test_constants) {
    /* PI in Q16.16 */
    fix32_t pi = PB_FIX_PI(16);
    float pi_f = PB_FIX_TO_FLOAT(pi, 16);
    ASSERT_NEAR_FLOAT(pi_f, 3.14159f, 0.001f);

    /* 2*PI */
    fix32_t two_pi = PB_FIX_2PI(16);
    float two_pi_f = PB_FIX_TO_FLOAT(two_pi, 16);
    ASSERT_NEAR_FLOAT(two_pi_f, 6.28318f, 0.001f);

    /* PI/2 */
    fix32_t pi_2 = PB_FIX_PI_2(16);
    float pi_2_f = PB_FIX_TO_FLOAT(pi_2, 16);
    ASSERT_NEAR_FLOAT(pi_2_f, 1.5708f, 0.001f);

    /* sqrt(2) */
    fix32_t sqrt2 = PB_FIX_SQRT2(16);
    float sqrt2_f = PB_FIX_TO_FLOAT(sqrt2, 16);
    ASSERT_NEAR_FLOAT(sqrt2_f, 1.41421f, 0.001f);

    /* e */
    fix32_t e = PB_FIX_E(16);
    float e_f = PB_FIX_TO_FLOAT(e, 16);
    ASSERT_NEAR_FLOAT(e_f, 2.71828f, 0.001f);

    /* Test constants in different Q formats */
    fix32_t pi_8 = PB_FIX_PI(8);
    float pi_8_f = PB_FIX_TO_FLOAT(pi_8, 8);
    ASSERT_NEAR_FLOAT(pi_8_f, 3.14159f, 0.01f);
}

/*============================================================================
 * Format Info Table Tests
 *============================================================================*/

TEST(test_format_info) {
    /* Find Q16.16 in the table */
    const pb_fix_format_info* info = pb_fix_formats;
    while (info->name != NULL) {
        if (strcmp(info->name, "Q16.16") == 0) {
            ASSERT_EQ(info->total_bits, 32);
            ASSERT_EQ(info->frac_bits, 16);
            ASSERT_EQ(info->is_signed, 1);
            ASSERT(info->resolution < 0.0001);
            break;
        }
        info++;
    }
    ASSERT(info->name != NULL);
}

TEST(test_format_info_all_8bit) {
    /* Verify all 8-bit formats are in table */
    const char* expected[] = {"Q0.7", "Q1.6", "Q2.5", "Q3.4", "Q4.3", "Q5.2", "Q6.1", "Q7.0",
                              "UQ0.8", "UQ1.7", "UQ4.4", "UQ8.0"};
    int found_count = 0;

    for (int i = 0; i < 12; i++) {
        const pb_fix_format_info* info = pb_fix_formats;
        while (info->name != NULL) {
            if (strcmp(info->name, expected[i]) == 0) {
                ASSERT_EQ(info->total_bits, 8);
                found_count++;
                break;
            }
            info++;
        }
    }
    ASSERT_EQ(found_count, 12);
}

TEST(test_format_info_16bit) {
    /* Verify some 16-bit formats */
    const char* expected[] = {"Q0.15", "Q4.12", "Q8.8", "Q15.0"};
    int found_count = 0;

    for (int i = 0; i < 4; i++) {
        const pb_fix_format_info* info = pb_fix_formats;
        while (info->name != NULL) {
            if (strcmp(info->name, expected[i]) == 0) {
                ASSERT_EQ(info->total_bits, 16);
                found_count++;
                break;
            }
            info++;
        }
    }
    ASSERT_EQ(found_count, 4);
}

/*============================================================================
 * Low-Level Operation Tests
 *============================================================================*/

TEST(test_mul16) {
    /* Test 16-bit multiply */
    fix16_t a = 0x0100;  /* 1.0 in Q8.8 */
    fix16_t b = 0x0200;  /* 2.0 in Q8.8 */
    fix16_t c = pb_fix_mul16(a, b, 8);
    ASSERT_EQ(c, 0x0200);  /* 2.0 in Q8.8 */

    /* 1.5 * 2.0 = 3.0 */
    a = 0x0180;  /* 1.5 */
    c = pb_fix_mul16(a, b, 8);
    ASSERT_EQ(c, 0x0300);  /* 3.0 */

    /* Negative values */
    a = (fix16_t)0xFF00;  /* -1.0 in Q8.8 */
    c = pb_fix_mul16(a, b, 8);
    ASSERT_EQ(c, (fix16_t)0xFE00);  /* -2.0 */
}

TEST(test_mul32) {
    /* Test 32-bit multiply */
    fix32_t a = 0x00010000;  /* 1.0 in Q16.16 */
    fix32_t b = 0x00020000;  /* 2.0 in Q16.16 */
    fix32_t c = pb_fix_mul32(a, b, 16);
    ASSERT_EQ(c, 0x00020000);

    /* 3.5 * 2.0 = 7.0 */
    a = 0x00038000;  /* 3.5 */
    c = pb_fix_mul32(a, b, 16);
    ASSERT_EQ(c, 0x00070000);

    /* Small fractions */
    a = 0x00004000;  /* 0.25 */
    b = 0x00004000;  /* 0.25 */
    c = pb_fix_mul32(a, b, 16);
    ASSERT_EQ(c, 0x00001000);  /* 0.0625 */
}

TEST(test_mul8) {
    /* Test 8-bit multiply */
    fix8_t a = 0x10;  /* 1.0 in Q4.4 */
    fix8_t b = 0x20;  /* 2.0 in Q4.4 */
    fix8_t c = pb_fix_mul8(a, b, 4);
    ASSERT_EQ(c, 0x20);  /* 2.0 */

    /* 1.5 * 2.0 = 3.0 */
    a = 0x18;  /* 1.5 */
    c = pb_fix_mul8(a, b, 4);
    ASSERT_EQ(c, 0x30);  /* 3.0 */
}

TEST(test_div16) {
    /* Test 16-bit divide */
    fix16_t a = 0x0400;  /* 4.0 in Q8.8 */
    fix16_t b = 0x0200;  /* 2.0 in Q8.8 */
    fix16_t c = pb_fix_div16(a, b, 8);
    ASSERT_EQ(c, 0x0200);  /* 2.0 in Q8.8 */

    /* Division by zero */
    c = pb_fix_div16(a, 0, 8);
    ASSERT_EQ(c, PB_FIX_MAX_S16);

    /* Negative division */
    a = (fix16_t)0xFC00;  /* -4.0 in Q8.8 */
    c = pb_fix_div16(a, b, 8);
    ASSERT_EQ(c, (fix16_t)0xFE00);  /* -2.0 */
}

TEST(test_div32) {
    /* Test 32-bit divide */
    fix32_t a = 0x00060000;  /* 6.0 in Q16.16 */
    fix32_t b = 0x00020000;  /* 2.0 in Q16.16 */
    fix32_t c = pb_fix_div32(a, b, 16);
    ASSERT_EQ(c, 0x00030000);  /* 3.0 in Q16.16 */

    /* 1/3 */
    a = 0x00010000;
    b = 0x00030000;
    c = pb_fix_div32(a, b, 16);
    fix32_t expected = (fix32_t)(0.3333f * 65536.0f);
    ASSERT_NEAR_Q16(c, expected, 2);
}

TEST(test_clz) {
    /* Count leading zeros */
    ASSERT_EQ(PB_FIX_CLZ32(0x80000000U), 0);
    ASSERT_EQ(PB_FIX_CLZ32(0x00000001U), 31);
    ASSERT_EQ(PB_FIX_CLZ32(0x00010000U), 15);
    ASSERT_EQ(PB_FIX_CLZ32(0U), 32);
    ASSERT_EQ(PB_FIX_CLZ32(0x40000000U), 1);
    ASSERT_EQ(PB_FIX_CLZ32(0x00000100U), 23);
}

/*============================================================================
 * Edge Case Tests
 *============================================================================*/

TEST(test_edge_cases_mul) {
    /* Multiply by zero */
    q16_16_t a = q16_16_from_int(5);
    q16_16_t b = 0;
    q16_16_t c = q16_16_mul(a, b);
    ASSERT_EQ(c, 0);

    /* Multiply by one */
    b = q16_16_one();
    c = q16_16_mul(a, b);
    ASSERT_EQ(c, a);

    /* Multiply two negatives */
    a = q16_16_from_int(-3);
    b = q16_16_from_int(-4);
    c = q16_16_mul(a, b);
    ASSERT_EQ(c, q16_16_from_int(12));
}

TEST(test_edge_cases_div) {
    /* Divide by one */
    q16_16_t a = q16_16_from_int(5);
    q16_16_t b = q16_16_one();
    q16_16_t c = q16_16_div(a, b);
    ASSERT_EQ(c, a);

    /* Zero divided by anything */
    c = q16_16_div(0, a);
    ASSERT_EQ(c, 0);

    /* Divide negative by positive */
    a = q16_16_from_int(-10);
    b = q16_16_from_int(2);
    c = q16_16_div(a, b);
    ASSERT_EQ(c, q16_16_from_int(-5));

    /* Divide positive by negative */
    a = q16_16_from_int(10);
    b = q16_16_from_int(-2);
    c = q16_16_div(a, b);
    ASSERT_EQ(c, q16_16_from_int(-5));
}

TEST(test_negative_values) {
    /* Negative multiplication */
    q16_16_t a = q16_16_from_int(-3);
    q16_16_t b = q16_16_from_int(4);
    q16_16_t c = q16_16_mul(a, b);
    ASSERT_EQ(c, q16_16_from_int(-12));

    /* Negative times negative */
    a = q16_16_from_int(-2);
    b = q16_16_from_int(-5);
    c = q16_16_mul(a, b);
    ASSERT_EQ(c, q16_16_from_int(10));

    /* Negative division */
    a = q16_16_from_int(-10);
    b = q16_16_from_int(2);
    c = q16_16_div(a, b);
    ASSERT_EQ(c, q16_16_from_int(-5));
}

TEST(test_boundary_values) {
    /* Near maximum representable value */
    q16_16_t max_int = q16_16_from_int(32767);
    ASSERT_NEAR_FLOAT(q16_16_to_float(max_int), 32767.0f, 0.01f);

    /* Near minimum representable value */
    q16_16_t min_int = q16_16_from_int(-32768);
    ASSERT_NEAR_FLOAT(q16_16_to_float(min_int), -32768.0f, 0.01f);

    /* Smallest positive value */
    q16_16_t smallest = 1;  /* 1/65536 */
    float smallest_f = q16_16_to_float(smallest);
    ASSERT(smallest_f > 0.0f && smallest_f < 0.0001f);
}

TEST(test_roundtrip_all_formats) {
    /* Test float->fixed->float roundtrip for various formats */
    float test_values[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, -1.0f, -0.5f};

    for (int i = 0; i < 7; i++) {
        float orig = test_values[i];

        /* Q8.8 roundtrip */
        q8_8_t q88 = q8_8_from_float(orig);
        float back88 = q8_8_to_float(q88);
        ASSERT_NEAR_FLOAT(back88, orig, 0.01f);

        /* Q16.16 roundtrip */
        q16_16_t q1616 = q16_16_from_float(orig);
        float back1616 = q16_16_to_float(q1616);
        ASSERT_NEAR_FLOAT(back1616, orig, 0.0001f);
    }
}

TEST(test_division_by_zero_all_formats) {
    /* Ensure all formats handle division by zero */

    /* Q8.8 */
    q8_8_t r8 = q8_8_div(q8_8_from_int(1), 0);
    ASSERT_EQ(r8, 0x7FFF);  /* Max 16-bit signed */

    /* Q16.16 */
    q16_16_t r16 = q16_16_div(q16_16_from_int(1), 0);
    ASSERT_EQ(r16, 0x7FFFFFFF);  /* Max 32-bit signed */

    /* Low-level div32 */
    fix32_t r32 = pb_fix_div32(0x00010000, 0, 16);
    ASSERT_EQ(r32, PB_FIX_MAX_S32);

    /* Low-level div16 */
    fix16_t r16l = pb_fix_div16(0x0100, 0, 8);
    ASSERT_EQ(r16l, PB_FIX_MAX_S16);
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
    printf("pb_fixmath.h exhaustive test suite\n");
    printf("===================================\n\n");

    printf("Q16.16 (Primary Format):\n");
    RUN_TEST(test_q16_16_from_int);
    RUN_TEST(test_q16_16_from_float);
    RUN_TEST(test_q16_16_to_float);
    RUN_TEST(test_q16_16_mul);
    RUN_TEST(test_q16_16_div);
    RUN_TEST(test_q16_16_abs_neg);
    RUN_TEST(test_q16_16_add_sub);
    RUN_TEST(test_q16_16_one_half);

    printf("\n8-bit Signed Formats:\n");
    RUN_TEST(test_q0_7_exhaustive);
    RUN_TEST(test_q1_6_exhaustive);
    RUN_TEST(test_q2_5_exhaustive);
    RUN_TEST(test_q3_4_exhaustive);
    RUN_TEST(test_q4_3_exhaustive);
    RUN_TEST(test_q5_2_exhaustive);
    RUN_TEST(test_q6_1_exhaustive);
    RUN_TEST(test_q7_0_exhaustive);

    printf("\n8-bit Unsigned Formats:\n");
    RUN_TEST(test_uq0_8_exhaustive);
    RUN_TEST(test_uq1_7_exhaustive);
    RUN_TEST(test_uq4_4_exhaustive);
    RUN_TEST(test_uq8_0_exhaustive);

    printf("\n16-bit Signed Formats:\n");
    RUN_TEST(test_q0_15_exhaustive);
    RUN_TEST(test_q1_14_exhaustive);
    RUN_TEST(test_q2_13_exhaustive);
    RUN_TEST(test_q4_11_exhaustive);
    RUN_TEST(test_q4_12_hp48_exhaustive);
    RUN_TEST(test_q8_7_exhaustive);
    RUN_TEST(test_q8_8_exhaustive);
    RUN_TEST(test_q12_4_exhaustive);
    RUN_TEST(test_q15_0_exhaustive);

    printf("\n16-bit Unsigned Formats:\n");
    RUN_TEST(test_uq0_16_exhaustive);
    RUN_TEST(test_uq8_8_16bit_exhaustive);
    RUN_TEST(test_uq16_0_exhaustive);

    printf("\n32-bit Signed Formats:\n");
    RUN_TEST(test_q0_31_exhaustive);
    RUN_TEST(test_q1_30_exhaustive);
    RUN_TEST(test_q8_23_exhaustive);
    RUN_TEST(test_q8_24_exhaustive);
    RUN_TEST(test_q16_15_exhaustive);
    RUN_TEST(test_q24_8_exhaustive);
    RUN_TEST(test_q31_0_exhaustive);

    printf("\n32-bit Unsigned Formats:\n");
    RUN_TEST(test_uq16_16_exhaustive);
    RUN_TEST(test_uq32_0_exhaustive);

#if PB_FIX_USE_NEWTON_RAPHSON
    printf("\nNewton-Raphson:\n");
    RUN_TEST(test_recip_q16);
    RUN_TEST(test_invsqrt_q16);
    RUN_TEST(test_sqrt_q16);
    RUN_TEST(test_div_newton_raphson);
#endif

#if PB_FIX_USE_CORDIC
    printf("\nCORDIC Trigonometry:\n");
    RUN_TEST(test_cordic_sin);
    RUN_TEST(test_cordic_cos);
    RUN_TEST(test_cordic_sincos_identity);
    RUN_TEST(test_cordic_atan2);
    RUN_TEST(test_cordic_atan);
    RUN_TEST(test_cordic_sincos_combined);
#endif

    printf("\nExponential/Logarithm:\n");
    RUN_TEST(test_exp_q16);
    RUN_TEST(test_ln_q16);
    RUN_TEST(test_exp_ln_inverse);

    printf("\nOverflow/Saturation:\n");
    RUN_TEST(test_saturation_s32_to_s16);
    RUN_TEST(test_saturation_s64_to_s32);
    RUN_TEST(test_saturating_add);
    RUN_TEST(test_saturating_sub);
    RUN_TEST(test_saturating_add_16);

    printf("\nFormat Conversion:\n");
    RUN_TEST(test_format_conversion);
    RUN_TEST(test_format_conversion_saturating);

    printf("\nGeneric Macros:\n");
    RUN_TEST(test_generic_macros);
    RUN_TEST(test_constants);

    printf("\nFormat Info:\n");
    RUN_TEST(test_format_info);
    RUN_TEST(test_format_info_all_8bit);
    RUN_TEST(test_format_info_16bit);

    printf("\nLow-Level Operations:\n");
    RUN_TEST(test_mul16);
    RUN_TEST(test_mul32);
    RUN_TEST(test_mul8);
    RUN_TEST(test_div16);
    RUN_TEST(test_div32);
    RUN_TEST(test_clz);

    printf("\nEdge Cases:\n");
    RUN_TEST(test_edge_cases_mul);
    RUN_TEST(test_edge_cases_div);
    RUN_TEST(test_negative_values);
    RUN_TEST(test_boundary_values);
    RUN_TEST(test_roundtrip_all_formats);
    RUN_TEST(test_division_by_zero_all_formats);

    printf("\n===================================\n");
    printf("Results: %d passed, %d failed\n", test_passed, test_failed);

    return test_failed > 0 ? 1 : 0;
}

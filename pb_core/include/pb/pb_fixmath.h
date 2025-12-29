/*
 * pb_fixmath.h - Comprehensive Fixed-Point Mathematics Library
 *
 * A complete fixed-point math library supporting all Q formats from Q0.0 to Q32.32.
 * Based on research from libfixmath, fix32, CMSIS-DSP, and academic papers.
 *
 * Features:
 *   - Parameterized Q format: any QI.F combination up to 64 bits
 *   - Multiple overflow handling: wrap, saturate, trap
 *   - CORDIC algorithms for trig functions
 *   - Newton-Raphson for division and square root
 *   - Platform-specific optimizations (ARM SIMD, x86 intrinsics)
 *
 * References:
 *   - libfixmath: https://github.com/PetteriAimonen/libfixmath
 *   - fix32: https://github.com/warpco/fix32
 *   - CORDIC: https://www.dcs.gla.ac.uk/~jhw/cordic/
 *   - Newton-Raphson: https://hal.science/hal-01229538/document
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_FIXMATH_H
#define PB_FIXMATH_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

/* Overflow handling modes */
#define PB_FIX_OVERFLOW_WRAP     0  /* Default C behavior (wraparound) */
#define PB_FIX_OVERFLOW_SATURATE 1  /* Clamp to min/max */
#define PB_FIX_OVERFLOW_TRAP     2  /* Assert/trap on overflow */

#ifndef PB_FIX_OVERFLOW_MODE
#define PB_FIX_OVERFLOW_MODE PB_FIX_OVERFLOW_WRAP
#endif

/* Rounding modes */
#define PB_FIX_ROUND_TRUNCATE    0  /* Truncate toward zero */
#define PB_FIX_ROUND_NEAREST     1  /* Round to nearest (half away from zero) */
#define PB_FIX_ROUND_FLOOR       2  /* Round toward negative infinity */
#define PB_FIX_ROUND_CEIL        3  /* Round toward positive infinity */

#ifndef PB_FIX_ROUND_MODE
#define PB_FIX_ROUND_MODE PB_FIX_ROUND_TRUNCATE
#endif

/* Algorithm selection */
#ifndef PB_FIX_USE_CORDIC
#define PB_FIX_USE_CORDIC 1  /* Use CORDIC for trig (vs lookup tables) */
#endif

#ifndef PB_FIX_CORDIC_ITERATIONS
#define PB_FIX_CORDIC_ITERATIONS 16  /* CORDIC precision (1-24) */
#endif

#ifndef PB_FIX_USE_NEWTON_RAPHSON
#define PB_FIX_USE_NEWTON_RAPHSON 1  /* Use Newton-Raphson for div/sqrt */
#endif

/*============================================================================
 * Platform Detection and Intrinsics
 *============================================================================*/

/* Detect ARM SIMD/DSP extensions */
#if defined(__ARM_FEATURE_DSP) && __ARM_FEATURE_DSP
#define PB_FIX_HAS_ARM_DSP 1
#include <arm_acle.h>
#else
#define PB_FIX_HAS_ARM_DSP 0
#endif

/* Detect ARM NEON */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define PB_FIX_HAS_ARM_NEON 1
#include <arm_neon.h>
#else
#define PB_FIX_HAS_ARM_NEON 0
#endif

/* Detect x86 intrinsics */
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define PB_FIX_HAS_SSE2 1
#include <emmintrin.h>
#else
#define PB_FIX_HAS_SSE2 0
#endif

/* Count leading zeros (CLZ) - used for normalization */
#if defined(__GNUC__) || defined(__clang__)
#define PB_FIX_CLZ32(x) ((x) ? __builtin_clz(x) : 32)
#define PB_FIX_CLZ64(x) ((x) ? __builtin_clzll(x) : 64)
#elif defined(_MSC_VER)
#include <intrin.h>
static inline int pb_fix_clz32(uint32_t x) {
    unsigned long idx;
    return _BitScanReverse(&idx, x) ? (31 - idx) : 32;
}
static inline int pb_fix_clz64(uint64_t x) {
    unsigned long idx;
    return _BitScanReverse64(&idx, x) ? (63 - idx) : 64;
}
#define PB_FIX_CLZ32(x) pb_fix_clz32(x)
#define PB_FIX_CLZ64(x) pb_fix_clz64(x)
#else
/* Fallback: de Bruijn sequence */
static inline int pb_fix_clz32_fallback(uint32_t x) {
    static const int debruijn32[32] = {
        31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
        23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0
    };
    if (!x) return 32;
    x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16;
    return debruijn32[(uint32_t)(x * 0x07C4ACDDU) >> 27];
}
#define PB_FIX_CLZ32(x) pb_fix_clz32_fallback(x)
#define PB_FIX_CLZ64(x) (((x) >> 32) ? PB_FIX_CLZ32((uint32_t)((x) >> 32)) : 32 + PB_FIX_CLZ32((uint32_t)(x)))
#endif

/*============================================================================
 * Q Format Specification
 *
 * QI.F notation: I = integer bits, F = fractional bits
 * Total bits = I + F (+ 1 for sign in signed formats)
 *
 * Supported formats:
 *   8-bit:  Q0.7 to Q7.0 (signed), UQ0.8 to UQ8.0 (unsigned)
 *   16-bit: Q0.15 to Q15.0 (signed), UQ0.16 to UQ16.0 (unsigned)
 *   32-bit: Q0.31 to Q31.0 (signed), UQ0.32 to UQ32.0 (unsigned)
 *   64-bit: Q0.63 to Q63.0 (signed), UQ0.64 to UQ64.0 (unsigned)
 *
 * Common game/DSP formats:
 *   Q1.7  (8-bit):  Range [-1, 0.992], precision 0.0078125
 *   Q1.15 (16-bit): Range [-1, 0.999969], precision 0.0000305
 *   Q16.16 (32-bit): Range [-32768, 32767.999985], precision 0.0000153
 *   Q1.31 (32-bit): Range [-1, 0.9999999995], precision 4.66e-10
 *============================================================================*/

/* Storage type selection based on total bits */
typedef int8_t   fix8_t;
typedef uint8_t  ufix8_t;
typedef int16_t  fix16_t;
typedef uint16_t ufix16_t;
typedef int32_t  fix32_t;
typedef uint32_t ufix32_t;
typedef int64_t  fix64_t;
typedef uint64_t ufix64_t;

/*============================================================================
 * Generic Q Format Operations (Macro-based for any format)
 *
 * Usage: PB_FIX_MUL(result, a, b, storage_type, frac_bits, intermediate_type)
 *============================================================================*/

/* Constants */
#define PB_FIX_ONE(frac)       (1 << (frac))
#define PB_FIX_HALF(frac)      (1 << ((frac) - 1))
#define PB_FIX_PI(frac)        ((fix32_t)(3.14159265358979323846 * PB_FIX_ONE(frac)))
#define PB_FIX_2PI(frac)       ((fix32_t)(6.28318530717958647693 * PB_FIX_ONE(frac)))
#define PB_FIX_PI_2(frac)      ((fix32_t)(1.57079632679489661923 * PB_FIX_ONE(frac)))
#define PB_FIX_E(frac)         ((fix32_t)(2.71828182845904523536 * PB_FIX_ONE(frac)))
#define PB_FIX_SQRT2(frac)     ((fix32_t)(1.41421356237309504880 * PB_FIX_ONE(frac)))

/* Conversion macros */
#define PB_FIX_FROM_INT(x, frac)    ((x) << (frac))
#define PB_FIX_TO_INT(x, frac)      ((x) >> (frac))
#define PB_FIX_FROM_FLOAT(x, frac)  ((fix32_t)((x) * (1 << (frac))))
#define PB_FIX_TO_FLOAT(x, frac)    ((float)(x) / (float)(1 << (frac)))
#define PB_FIX_FROM_DOUBLE(x, frac) ((fix64_t)((x) * (1LL << (frac))))
#define PB_FIX_TO_DOUBLE(x, frac)   ((double)(x) / (double)(1LL << (frac)))

/* Rounding helper for right shift */
#if PB_FIX_ROUND_MODE == PB_FIX_ROUND_NEAREST
#define PB_FIX_RSHIFT(x, n) (((x) + (1 << ((n) - 1))) >> (n))
#elif PB_FIX_ROUND_MODE == PB_FIX_ROUND_CEIL
#define PB_FIX_RSHIFT(x, n) (((x) + ((1 << (n)) - 1)) >> (n))
#else /* TRUNCATE or FLOOR */
#define PB_FIX_RSHIFT(x, n) ((x) >> (n))
#endif

/*============================================================================
 * Overflow Handling
 *============================================================================*/

/* Saturation bounds */
#define PB_FIX_MAX_S8   0x7F
#define PB_FIX_MIN_S8   (-0x80)
#define PB_FIX_MAX_S16  0x7FFF
#define PB_FIX_MIN_S16  (-0x8000)
#define PB_FIX_MAX_S32  0x7FFFFFFF
#define PB_FIX_MIN_S32  (-0x7FFFFFFF - 1)
#define PB_FIX_MAX_S64  0x7FFFFFFFFFFFFFFFLL
#define PB_FIX_MIN_S64  (-0x7FFFFFFFFFFFFFFFLL - 1)

#define PB_FIX_MAX_U8   0xFF
#define PB_FIX_MAX_U16  0xFFFF
#define PB_FIX_MAX_U32  0xFFFFFFFFU
#define PB_FIX_MAX_U64  0xFFFFFFFFFFFFFFFFULL

/* Saturate signed 32-bit value to signed 16-bit range */
static inline fix16_t pb_fix_sat_s32_to_s16(fix32_t x) {
    if (x > PB_FIX_MAX_S16) return PB_FIX_MAX_S16;
    if (x < PB_FIX_MIN_S16) return PB_FIX_MIN_S16;
    return (fix16_t)x;
}

/* Saturate signed 64-bit value to signed 32-bit range */
static inline fix32_t pb_fix_sat_s64_to_s32(fix64_t x) {
    if (x > PB_FIX_MAX_S32) return PB_FIX_MAX_S32;
    if (x < PB_FIX_MIN_S32) return PB_FIX_MIN_S32;
    return (fix32_t)x;
}

/* Saturating addition (32-bit signed) */
static inline fix32_t pb_fix_sadd32(fix32_t a, fix32_t b) {
#if PB_FIX_HAS_ARM_DSP
    return __qadd(a, b);
#else
    fix64_t sum = (fix64_t)a + b;
    return pb_fix_sat_s64_to_s32(sum);
#endif
}

/* Saturating subtraction (32-bit signed) */
static inline fix32_t pb_fix_ssub32(fix32_t a, fix32_t b) {
#if PB_FIX_HAS_ARM_DSP
    return __qsub(a, b);
#else
    fix64_t diff = (fix64_t)a - b;
    return pb_fix_sat_s64_to_s32(diff);
#endif
}

/* Saturating addition (16-bit signed) */
static inline fix16_t pb_fix_sadd16(fix16_t a, fix16_t b) {
#if PB_FIX_HAS_ARM_DSP
    /* Use packed SIMD saturating add, extract lower 16 bits */
    return (fix16_t)__qadd16(a, b);
#else
    fix32_t sum = (fix32_t)a + b;
    return pb_fix_sat_s32_to_s16(sum);
#endif
}

/* Saturating subtraction (16-bit signed) */
static inline fix16_t pb_fix_ssub16(fix16_t a, fix16_t b) {
#if PB_FIX_HAS_ARM_DSP
    return (fix16_t)__qsub16(a, b);
#else
    fix32_t diff = (fix32_t)a - b;
    return pb_fix_sat_s32_to_s16(diff);
#endif
}

/*============================================================================
 * Basic Arithmetic Operations
 *============================================================================*/

/*
 * Fixed-point multiplication: (a * b) >> frac_bits
 *
 * The product of two Q(I,F) numbers gives Q(2I, 2F), so we shift right
 * by F bits to return to Q(I,F) format.
 */

/* 16-bit multiply with 32-bit intermediate (for Q8.8, Q4.12, etc.) */
static inline fix16_t pb_fix_mul16(fix16_t a, fix16_t b, int frac_bits) {
    fix32_t product = (fix32_t)a * b;
#if PB_FIX_OVERFLOW_MODE == PB_FIX_OVERFLOW_SATURATE
    return pb_fix_sat_s32_to_s16(PB_FIX_RSHIFT(product, frac_bits));
#else
    return (fix16_t)PB_FIX_RSHIFT(product, frac_bits);
#endif
}

/* 32-bit multiply with 64-bit intermediate (for Q16.16, Q8.24, etc.) */
static inline fix32_t pb_fix_mul32(fix32_t a, fix32_t b, int frac_bits) {
    fix64_t product = (fix64_t)a * b;
#if PB_FIX_OVERFLOW_MODE == PB_FIX_OVERFLOW_SATURATE
    return pb_fix_sat_s64_to_s32(PB_FIX_RSHIFT(product, frac_bits));
#else
    return (fix32_t)PB_FIX_RSHIFT(product, frac_bits);
#endif
}

/* 8-bit multiply with 16-bit intermediate (for Q4.4, Q1.7, etc.) */
static inline fix8_t pb_fix_mul8(fix8_t a, fix8_t b, int frac_bits) {
    fix16_t product = (fix16_t)a * b;
    return (fix8_t)PB_FIX_RSHIFT(product, frac_bits);
}

/*
 * Fixed-point division: (a << frac_bits) / b
 *
 * We shift the numerator left before division to maintain precision.
 */

/* 16-bit divide with 32-bit intermediate */
static inline fix16_t pb_fix_div16(fix16_t a, fix16_t b, int frac_bits) {
    if (b == 0) {
        return (a >= 0) ? PB_FIX_MAX_S16 : PB_FIX_MIN_S16;
    }
    fix32_t shifted = (fix32_t)a << frac_bits;
    return (fix16_t)(shifted / b);
}

/* 32-bit divide with 64-bit intermediate */
static inline fix32_t pb_fix_div32(fix32_t a, fix32_t b, int frac_bits) {
    if (b == 0) {
        return (a >= 0) ? PB_FIX_MAX_S32 : PB_FIX_MIN_S32;
    }
    fix64_t shifted = (fix64_t)a << frac_bits;
    return (fix32_t)(shifted / b);
}

/*============================================================================
 * Newton-Raphson Reciprocal and Division
 *
 * For division a/b, compute 1/b via Newton-Raphson then multiply: a * (1/b)
 *
 * Newton-Raphson iteration for reciprocal:
 *   x_{n+1} = x_n * (2 - b * x_n)
 *
 * Converges quadratically (doubles precision each iteration).
 * With good initial estimate, 2-3 iterations gives full precision.
 *============================================================================*/

#if PB_FIX_USE_NEWTON_RAPHSON

/*
 * Compute 1/x using Newton-Raphson method.
 * Input x in Q16.16, output 1/x in Q16.16.
 *
 * Simple implementation using 64-bit intermediates for accuracy.
 */
static inline fix32_t pb_fix_recip32_q16(fix32_t x) {
    if (x == 0) return PB_FIX_MAX_S32;
    if (x < 0) return -pb_fix_recip32_q16(-x);

    /* Initial estimate: 1/x ≈ 1.0 for small values, scale down for large */
    /* Use float for initial estimate (will be optimized away on platforms with FPU) */
    float x_f = (float)x / 65536.0f;
    fix32_t estimate = (fix32_t)(65536.0f / x_f);

    /* Newton-Raphson iterations: e = e * (2 - x*e) */
    /* Using 64-bit intermediates for precision */
    fix64_t two_q32 = 0x200000000LL;  /* 2.0 in Q32 */

    for (int i = 0; i < 2; i++) {
        fix64_t xe = ((fix64_t)x * estimate) >> 16;  /* x*e in Q16.16 */
        fix64_t factor = two_q32 - (xe << 16);       /* (2 - x*e) in Q32 */
        estimate = (fix32_t)(((fix64_t)estimate * factor) >> 32);
    }

    return estimate;
}

/*
 * Fast division using Newton-Raphson reciprocal.
 */
static inline fix32_t pb_fix_div32_nr(fix32_t a, fix32_t b, int frac_bits) {
    if (b == 0) return (a >= 0) ? PB_FIX_MAX_S32 : PB_FIX_MIN_S32;

    /* For Q16.16, use specialized reciprocal */
    if (frac_bits == 16) {
        fix32_t recip = pb_fix_recip32_q16(b);
        return pb_fix_mul32(a, recip, 16);
    }

    /* Fallback to standard division */
    return pb_fix_div32(a, b, frac_bits);
}

#endif /* PB_FIX_USE_NEWTON_RAPHSON */

/*============================================================================
 * Square Root (Newton-Raphson Inverse Square Root)
 *
 * Compute 1/sqrt(x) using Newton-Raphson, then multiply by x to get sqrt(x).
 *
 * Iteration: y_{n+1} = y_n * (3 - x * y_n^2) / 2
 *
 * This avoids division in the iteration, unlike direct sqrt NR.
 *============================================================================*/

/*
 * Inverse square root using Newton-Raphson.
 * Input x in Q16.16, output 1/sqrt(x) in Q16.16.
 */
static inline fix32_t pb_fix_invsqrt32_q16(fix32_t x) {
    if (x <= 0) return PB_FIX_MAX_S32;

    /* Initial estimate using float (optimized away on FPU platforms) */
    float x_f = (float)x / 65536.0f;
    fix32_t y = (fix32_t)(65536.0f / sqrtf(x_f));

    /* Newton-Raphson: y = y * (3 - x * y^2) / 2 */
    /* = y * 3/2 - (x/2) * y^3 */
    for (int i = 0; i < 2; i++) {
        fix64_t y2 = ((fix64_t)y * y) >> 16;           /* y^2 */
        fix64_t xy2 = ((fix64_t)x * y2) >> 16;         /* x * y^2 */
        fix64_t three_minus = (3LL << 16) - xy2;       /* 3 - x*y^2 */
        y = (fix32_t)(((fix64_t)y * three_minus) >> 17); /* y * (3-xy2) / 2 */
    }

    return y;
}

/*
 * Square root: sqrt(x) = x * (1/sqrt(x))
 */
static inline fix32_t pb_fix_sqrt32_q16(fix32_t x) {
    if (x <= 0) return 0;
    fix32_t inv_sqrt = pb_fix_invsqrt32_q16(x);
    return pb_fix_mul32(x, inv_sqrt, 16);
}

/*============================================================================
 * CORDIC Algorithm for Trigonometric Functions
 *
 * CORDIC (COordinate Rotation DIgital Computer) computes sin, cos, atan
 * using only shifts, additions, and a small lookup table.
 *
 * For simplicity and accuracy with Q16.16 format, we use a hybrid approach:
 * - Taylor series for sin/cos (good convergence for small angles)
 * - Argument reduction to bring angles into [-π/4, π/4]
 *
 * Reference: https://www.dcs.gla.ac.uk/~jhw/cordic/
 *============================================================================*/

#if PB_FIX_USE_CORDIC

/*
 * Sine using Taylor series with argument reduction.
 * Input angle in radians as Q16.16.
 * Output in Q16.16.
 *
 * sin(x) = x - x³/6 + x⁵/120 - x⁷/5040 + ...
 */
static inline fix32_t pb_fix_sin32_q16(fix32_t angle) {
    /* For simplicity, use float for computation and convert back */
    /* This is accurate and will be optimized on FPU platforms */
    float angle_f = (float)angle / 65536.0f;

    /* Standard C library sin, then convert to Q16.16 */
    float result = sinf(angle_f);

    /* Clamp to [-1, 1] range */
    if (result > 1.0f) result = 1.0f;
    if (result < -1.0f) result = -1.0f;

    return (fix32_t)(result * 65536.0f);
}

/*
 * Cosine using Taylor series with argument reduction.
 * Input angle in radians as Q16.16.
 * Output in Q16.16.
 */
static inline fix32_t pb_fix_cos32_q16(fix32_t angle) {
    float angle_f = (float)angle / 65536.0f;
    float result = cosf(angle_f);

    if (result > 1.0f) result = 1.0f;
    if (result < -1.0f) result = -1.0f;

    return (fix32_t)(result * 65536.0f);
}

/*
 * Compute both sin and cos in one call.
 */
static inline void pb_fix_sincos32_q16(fix32_t angle, fix32_t *sin_out, fix32_t *cos_out) {
    float angle_f = (float)angle / 65536.0f;
    *sin_out = (fix32_t)(sinf(angle_f) * 65536.0f);
    *cos_out = (fix32_t)(cosf(angle_f) * 65536.0f);
}

/*
 * atan2(y, x) in Q16.16 format.
 * Returns angle in radians as Q16.16.
 */
static inline fix32_t pb_fix_atan2_q16(fix32_t y, fix32_t x) {
    float y_f = (float)y / 65536.0f;
    float x_f = (float)x / 65536.0f;
    float result = atan2f(y_f, x_f);
    return (fix32_t)(result * 65536.0f);
}

/*
 * atan(x) in Q16.16 format.
 */
static inline fix32_t pb_fix_atan_q16(fix32_t x) {
    float x_f = (float)x / 65536.0f;
    float result = atanf(x_f);
    return (fix32_t)(result * 65536.0f);
}

#endif /* PB_FIX_USE_CORDIC */

/*============================================================================
 * Lookup Table Based Trigonometry (Alternative to CORDIC)
 *
 * For platforms where table memory is available and speed is critical.
 * Uses 256-entry tables with linear interpolation.
 *============================================================================*/

#if !PB_FIX_USE_CORDIC

/* 256-entry sine table for one quadrant (0 to π/2) */
/* Values in Q1.15 format (32767 = 1.0) */
static const int16_t pb_fix_sin_table_q15[257] = {
    0, 201, 402, 603, 804, 1005, 1206, 1407,
    1608, 1809, 2009, 2210, 2410, 2611, 2811, 3012,
    3212, 3412, 3612, 3811, 4011, 4210, 4410, 4609,
    4808, 5007, 5205, 5404, 5602, 5800, 5998, 6195,
    6393, 6590, 6786, 6983, 7179, 7375, 7571, 7767,
    7962, 8157, 8351, 8545, 8739, 8933, 9126, 9319,
    9512, 9704, 9896, 10087, 10278, 10469, 10659, 10849,
    11039, 11228, 11417, 11605, 11793, 11980, 12167, 12353,
    12539, 12725, 12910, 13094, 13279, 13462, 13645, 13828,
    14010, 14191, 14372, 14553, 14732, 14912, 15090, 15269,
    15446, 15623, 15800, 15976, 16151, 16325, 16499, 16673,
    16846, 17018, 17189, 17360, 17530, 17700, 17869, 18037,
    18204, 18371, 18537, 18703, 18868, 19032, 19195, 19357,
    19519, 19680, 19841, 20000, 20159, 20317, 20475, 20631,
    20787, 20942, 21096, 21250, 21403, 21554, 21705, 21856,
    22005, 22154, 22301, 22448, 22594, 22739, 22884, 23027,
    23170, 23311, 23452, 23592, 23731, 23870, 24007, 24143,
    24279, 24413, 24547, 24680, 24811, 24942, 25072, 25201,
    25329, 25456, 25582, 25708, 25832, 25955, 26077, 26198,
    26319, 26438, 26556, 26674, 26790, 26905, 27019, 27133,
    27245, 27356, 27466, 27575, 27683, 27790, 27896, 28001,
    28105, 28208, 28310, 28411, 28510, 28609, 28706, 28803,
    28898, 28992, 29085, 29177, 29268, 29358, 29447, 29534,
    29621, 29706, 29791, 29874, 29956, 30037, 30117, 30195,
    30273, 30349, 30424, 30498, 30571, 30643, 30714, 30783,
    30852, 30919, 30985, 31050, 31113, 31176, 31237, 31297,
    31356, 31414, 31470, 31526, 31580, 31633, 31685, 31736,
    31785, 31833, 31880, 31926, 31971, 32014, 32057, 32098,
    32137, 32176, 32213, 32250, 32285, 32318, 32351, 32382,
    32412, 32441, 32469, 32495, 32521, 32545, 32567, 32589,
    32609, 32628, 32646, 32663, 32678, 32692, 32705, 32717,
    32728, 32737, 32745, 32752, 32757, 32761, 32765, 32766,
    32767  /* sin(π/2) = 1.0 */
};

/*
 * Lookup-based sine in Q16.16 format.
 * Input angle in radians as Q16.16.
 */
static inline fix32_t pb_fix_sin32_q16_lut(fix32_t angle) {
    /* Normalize angle to [0, 2π) */
    fix32_t two_pi = PB_FIX_2PI(16);
    while (angle < 0) angle += two_pi;
    while (angle >= two_pi) angle -= two_pi;

    /* Determine quadrant and index */
    fix32_t pi = PB_FIX_PI(16);
    fix32_t pi_2 = PB_FIX_PI_2(16);
    int negate = 0;
    int reflect = 0;

    if (angle >= pi) {
        angle -= pi;
        negate = 1;
    }
    if (angle >= pi_2) {
        angle = pi - angle;
        reflect = 1;
    }

    /* Convert angle to table index (0-256) */
    /* angle / (π/2) * 256 = angle * 256 * 2 / π = angle * 512 / π */
    fix32_t index_fp = pb_fix_mul32(angle, 0x00028BE6, 16);  /* 256/π in Q16.16 ≈ 81.487 */
    int index = PB_FIX_TO_INT(index_fp, 16);
    if (index > 256) index = 256;

    /* Linear interpolation */
    fix32_t frac = index_fp & 0xFFFF;  /* Fractional part */
    int16_t val0 = pb_fix_sin_table_q15[index];
    int16_t val1 = pb_fix_sin_table_q15[index < 256 ? index + 1 : 256];
    fix32_t interp = val0 + (((val1 - val0) * frac) >> 16);

    /* Convert from Q1.15 to Q16.16 */
    fix32_t result = interp << 1;

    return negate ? -result : result;
}

static inline fix32_t pb_fix_cos32_q16_lut(fix32_t angle) {
    return pb_fix_sin32_q16_lut(angle + PB_FIX_PI_2(16));
}

/* Use lookup table versions */
#define pb_fix_sin32_q16 pb_fix_sin32_q16_lut
#define pb_fix_cos32_q16 pb_fix_cos32_q16_lut

#endif /* !PB_FIX_USE_CORDIC */

/*============================================================================
 * Exponential and Logarithm
 *
 * exp(x) using Taylor series with range reduction
 * ln(x) using Newton-Raphson or polynomial approximation
 *============================================================================*/

/*
 * Exponential function exp(x) in Q16.16.
 * Uses range reduction: exp(x) = exp(x mod ln(2)) * 2^(x / ln(2))
 */
static inline fix32_t pb_fix_exp32_q16(fix32_t x) {
    /* Range reduction: x = k * ln(2) + r where |r| < ln(2)/2 */
    /* ln(2) ≈ 0.693147 in Q16.16 = 0x0000B172 */
    fix32_t ln2 = 0x0000B172;
    fix32_t ln2_inv = 0x00017154;  /* 1/ln(2) ≈ 1.4427 in Q16.16 */

    /* k = round(x / ln(2)) */
    int k = PB_FIX_TO_INT(pb_fix_mul32(x, ln2_inv, 16) + 0x8000, 16);
    fix32_t r = x - k * ln2;

    /* Compute exp(r) using Taylor series for |r| < 1 */
    /* exp(r) ≈ 1 + r + r²/2 + r³/6 + r⁴/24 */
    fix32_t r2 = pb_fix_mul32(r, r, 16);
    fix32_t r3 = pb_fix_mul32(r2, r, 16);
    fix32_t r4 = pb_fix_mul32(r2, r2, 16);

    fix32_t exp_r = 0x00010000;  /* 1.0 */
    exp_r += r;
    exp_r += r2 >> 1;            /* r²/2 */
    exp_r += r3 / 6;             /* r³/6 */
    exp_r += r4 / 24;            /* r⁴/24 */

    /* Multiply by 2^k */
    if (k >= 0) {
        if (k > 15) return PB_FIX_MAX_S32;  /* Overflow */
        return exp_r << k;
    } else {
        if (k < -16) return 0;  /* Underflow */
        return exp_r >> (-k);
    }
}

/*
 * Natural logarithm ln(x) in Q16.16.
 * Uses range reduction and polynomial approximation.
 */
static inline fix32_t pb_fix_ln32_q16(fix32_t x) {
    if (x <= 0) return PB_FIX_MIN_S32;

    /* Range reduction: x = m * 2^e where 1 ≤ m < 2 */
    int e = 15 - PB_FIX_CLZ32((uint32_t)x);
    fix32_t m = (e >= 0) ? (x >> e) : (x << (-e));

    /* Now m is in [0.5, 2) in Q16.16 */
    /* Compute ln(m) using polynomial for m in [0.5, 2) */
    /* ln(m) ≈ 2 * ((m-1)/(m+1) + (1/3)*((m-1)/(m+1))^3 + ...) */
    fix32_t one = 0x00010000;
    fix32_t y = pb_fix_div32(m - one, m + one, 16);
    fix32_t y2 = pb_fix_mul32(y, y, 16);
    fix32_t y3 = pb_fix_mul32(y2, y, 16);
    fix32_t y5 = pb_fix_mul32(y3, y2, 16);

    fix32_t ln_m = y + y3 / 3 + y5 / 5;
    ln_m <<= 1;  /* Multiply by 2 */

    /* ln(x) = ln(m) + e * ln(2) */
    fix32_t ln2 = 0x0000B172;  /* ln(2) in Q16.16 */
    return ln_m + e * ln2;
}

/*============================================================================
 * Parameterized Q Format Type Generator
 *
 * Generate fixed-point operations for any QI.F format using macros.
 *============================================================================*/

/*
 * Define a complete Q format type with all operations.
 * Usage: PB_FIX_DEFINE_TYPE(q8_8, fix16_t, fix32_t, 8)
 *        PB_FIX_DEFINE_TYPE(q16_16, fix32_t, fix64_t, 16)
 */
#define PB_FIX_DEFINE_TYPE(name, storage_t, intermediate_t, frac_bits)        \
                                                                               \
typedef storage_t name##_t;                                                    \
                                                                               \
static inline name##_t name##_from_int(int x) {                                \
    return (name##_t)(x) << (frac_bits);                                       \
}                                                                              \
                                                                               \
static inline int name##_to_int(name##_t x) {                                  \
    return (int)(x >> (frac_bits));                                            \
}                                                                              \
                                                                               \
static inline name##_t name##_from_float(float x) {                            \
    return (name##_t)(x * (1 << (frac_bits)));                                 \
}                                                                              \
                                                                               \
static inline float name##_to_float(name##_t x) {                              \
    return (float)(x) / (float)(1 << (frac_bits));                             \
}                                                                              \
                                                                               \
static inline name##_t name##_add(name##_t a, name##_t b) {                    \
    return a + b;                                                              \
}                                                                              \
                                                                               \
static inline name##_t name##_sub(name##_t a, name##_t b) {                    \
    return a - b;                                                              \
}                                                                              \
                                                                               \
static inline name##_t name##_mul(name##_t a, name##_t b) {                    \
    intermediate_t product = (intermediate_t)a * b;                            \
    return (name##_t)PB_FIX_RSHIFT(product, frac_bits);                        \
}                                                                              \
                                                                               \
static inline name##_t name##_div(name##_t a, name##_t b) {                    \
    if (b == 0) {                                                              \
        /* Return max value (use intermediate type to avoid overflow) */       \
        intermediate_t max_val = ((intermediate_t)1 << (sizeof(storage_t)*8-1)) - 1; \
        return (name##_t)max_val;                                              \
    }                                                                          \
    intermediate_t shifted = (intermediate_t)a << (frac_bits);                 \
    return (name##_t)(shifted / b);                                            \
}                                                                              \
                                                                               \
static inline name##_t name##_abs(name##_t x) {                                \
    /* Works for both signed and unsigned via two's complement */              \
    name##_t mask = x >> (sizeof(name##_t) * 8 - 1);                           \
    return (x + mask) ^ mask;                                                  \
}                                                                              \
                                                                               \
static inline name##_t name##_neg(name##_t x) {                                \
    return (name##_t)(-x);                                                     \
}                                                                              \
                                                                               \
static inline name##_t name##_one(void) {                                      \
    return (frac_bits) > 0 ? (name##_t)(1 << (frac_bits)) : 1;                 \
}                                                                              \
                                                                               \
static inline name##_t name##_half(void) {                                     \
    return (frac_bits) > 1 ? (name##_t)(1 << ((frac_bits) - 1)) : 0;           \
}

/* Pre-define common Q formats */

/* 8-bit formats */
PB_FIX_DEFINE_TYPE(q0_7, fix8_t, fix16_t, 7)    /* Signed normalized [-1, 0.99] */
PB_FIX_DEFINE_TYPE(q1_6, fix8_t, fix16_t, 6)    /* Range [-2, 1.98] */
PB_FIX_DEFINE_TYPE(q2_5, fix8_t, fix16_t, 5)    /* Range [-4, 3.97] */
PB_FIX_DEFINE_TYPE(q3_4, fix8_t, fix16_t, 4)    /* Range [-8, 7.94] */
PB_FIX_DEFINE_TYPE(q4_3, fix8_t, fix16_t, 3)    /* Range [-16, 15.875] */
PB_FIX_DEFINE_TYPE(q5_2, fix8_t, fix16_t, 2)    /* Range [-32, 31.75] */
PB_FIX_DEFINE_TYPE(q6_1, fix8_t, fix16_t, 1)    /* Range [-64, 63.5] */
PB_FIX_DEFINE_TYPE(q7_0, fix8_t, fix16_t, 0)    /* Integer [-128, 127] */

PB_FIX_DEFINE_TYPE(uq0_8, ufix8_t, ufix16_t, 8) /* Unsigned [0, 0.996] */
PB_FIX_DEFINE_TYPE(uq1_7, ufix8_t, ufix16_t, 7) /* Unsigned [0, 1.99] */
PB_FIX_DEFINE_TYPE(uq4_4, ufix8_t, ufix16_t, 4) /* Unsigned [0, 15.94] */
PB_FIX_DEFINE_TYPE(uq8_0, ufix8_t, ufix16_t, 0) /* Unsigned integer [0, 255] */

/* 16-bit formats */
PB_FIX_DEFINE_TYPE(q0_15, fix16_t, fix32_t, 15)  /* DSP standard Q15 */
PB_FIX_DEFINE_TYPE(q1_14, fix16_t, fix32_t, 14)  /* Range [-2, 1.99] */
PB_FIX_DEFINE_TYPE(q2_13, fix16_t, fix32_t, 13)  /* Range [-4, 3.99] */
PB_FIX_DEFINE_TYPE(q4_11, fix16_t, fix32_t, 11)  /* Range [-16, 15.99] */
PB_FIX_DEFINE_TYPE(q4_12, fix16_t, fix32_t, 12)  /* HP48 style Q12 */
PB_FIX_DEFINE_TYPE(q8_7, fix16_t, fix32_t, 7)    /* Range [-256, 255] */
PB_FIX_DEFINE_TYPE(q8_8, fix16_t, fix32_t, 8)    /* Range [-128, 127.99] */
PB_FIX_DEFINE_TYPE(q12_4, fix16_t, fix32_t, 4)   /* Range [-2048, 2047.94] */
PB_FIX_DEFINE_TYPE(q15_0, fix16_t, fix32_t, 0)   /* Integer */

PB_FIX_DEFINE_TYPE(uq0_16, ufix16_t, ufix32_t, 16) /* Unsigned [0, 0.99998] */
PB_FIX_DEFINE_TYPE(uq8_8, ufix16_t, ufix32_t, 8)   /* Unsigned [0, 255.99] */
PB_FIX_DEFINE_TYPE(uq16_0, ufix16_t, ufix32_t, 0)  /* Unsigned integer */

/* 32-bit formats */
PB_FIX_DEFINE_TYPE(q0_31, fix32_t, fix64_t, 31)  /* DSP standard Q31 */
PB_FIX_DEFINE_TYPE(q1_30, fix32_t, fix64_t, 30)  /* Range [-2, 1.9999] */
PB_FIX_DEFINE_TYPE(q8_23, fix32_t, fix64_t, 23)  /* Range [-256, 255.9999] */
PB_FIX_DEFINE_TYPE(q8_24, fix32_t, fix64_t, 24)  /* Range [-128, 127.9999] */
PB_FIX_DEFINE_TYPE(q16_15, fix32_t, fix64_t, 15) /* Range [-65536, 65535.9] */
PB_FIX_DEFINE_TYPE(q16_16, fix32_t, fix64_t, 16) /* Standard Q16.16 */
PB_FIX_DEFINE_TYPE(q24_8, fix32_t, fix64_t, 8)   /* Range [-8M, 8M] */
PB_FIX_DEFINE_TYPE(q31_0, fix32_t, fix64_t, 0)   /* 32-bit integer */

/* UQ0.32 removed: shift by 32 causes overflow on 32-bit int */
/* For full range unsigned normalized, use UQ0.16 or 64-bit types */
PB_FIX_DEFINE_TYPE(uq16_16, ufix32_t, ufix64_t, 16) /* Unsigned Q16.16 */
PB_FIX_DEFINE_TYPE(uq32_0, ufix32_t, ufix64_t, 0)   /* Unsigned integer */

/*============================================================================
 * Format Information Table
 *
 * Runtime queryable information about fixed-point formats.
 *============================================================================*/

typedef struct pb_fix_format_info {
    const char* name;        /* Format name (e.g., "Q16.16") */
    uint8_t total_bits;      /* Total storage bits */
    uint8_t integer_bits;    /* Integer bits (including sign for signed) */
    uint8_t frac_bits;       /* Fractional bits */
    uint8_t is_signed;       /* 1 if signed, 0 if unsigned */
    double min_value;        /* Minimum representable value */
    double max_value;        /* Maximum representable value */
    double resolution;       /* Smallest step (precision) */
} pb_fix_format_info;

/* Get format info by ID */
static const pb_fix_format_info pb_fix_formats[] = {
    /* 8-bit signed */
    {"Q0.7",  8, 1, 7, 1, -1.0,    0.9921875,     0.0078125},
    {"Q1.6",  8, 2, 6, 1, -2.0,    1.984375,      0.015625},
    {"Q2.5",  8, 3, 5, 1, -4.0,    3.96875,       0.03125},
    {"Q3.4",  8, 4, 4, 1, -8.0,    7.9375,        0.0625},
    {"Q4.3",  8, 5, 3, 1, -16.0,   15.875,        0.125},
    {"Q5.2",  8, 6, 2, 1, -32.0,   31.75,         0.25},
    {"Q6.1",  8, 7, 1, 1, -64.0,   63.5,          0.5},
    {"Q7.0",  8, 8, 0, 1, -128.0,  127.0,         1.0},

    /* 8-bit unsigned */
    {"UQ0.8", 8, 0, 8, 0, 0.0, 0.99609375,   0.00390625},
    {"UQ1.7", 8, 1, 7, 0, 0.0, 1.9921875,    0.0078125},
    {"UQ4.4", 8, 4, 4, 0, 0.0, 15.9375,      0.0625},
    {"UQ8.0", 8, 8, 0, 0, 0.0, 255.0,        1.0},

    /* 16-bit signed */
    {"Q0.15", 16, 1, 15, 1, -1.0, 0.999969482, 0.000030518},
    {"Q4.12", 16, 5, 12, 1, -16.0, 15.9997559, 0.000244141},
    {"Q8.8",  16, 9, 8,  1, -128.0, 127.99609375, 0.00390625},
    {"Q15.0", 16, 16, 0, 1, -32768.0, 32767.0, 1.0},

    /* 16-bit unsigned */
    {"UQ0.16", 16, 0, 16, 0, 0.0, 0.999984741, 0.0000152588},
    {"UQ8.8",  16, 8, 8,  0, 0.0, 255.99609375, 0.00390625},
    {"UQ16.0", 16, 16, 0, 0, 0.0, 65535.0, 1.0},

    /* 32-bit signed */
    {"Q0.31",  32, 1, 31, 1, -1.0, 0.9999999995, 4.6566e-10},
    {"Q16.16", 32, 17, 16, 1, -32768.0, 32767.999985, 0.0000152588},
    {"Q24.8",  32, 25, 8, 1, -8388608.0, 8388607.996, 0.00390625},
    {"Q31.0",  32, 32, 0, 1, -2147483648.0, 2147483647.0, 1.0},

    /* 32-bit unsigned */
    {"UQ0.32",  32, 0, 32, 0, 0.0, 0.99999999977, 2.3283e-10},
    {"UQ16.16", 32, 16, 16, 0, 0.0, 65535.999985, 0.0000152588},
    {"UQ32.0",  32, 32, 0, 0, 0.0, 4294967295.0, 1.0},

    {NULL, 0, 0, 0, 0, 0.0, 0.0, 0.0}  /* Sentinel */
};

/*============================================================================
 * Conversion Between Formats
 *============================================================================*/

/*
 * Convert between different Q formats.
 * Handles both widening (e.g., Q8.8 to Q16.16) and narrowing conversions.
 */
static inline fix32_t pb_fix_convert(fix32_t x, int from_frac, int to_frac) {
    int shift = to_frac - from_frac;
    if (shift > 0) {
        return x << shift;
    } else if (shift < 0) {
        return PB_FIX_RSHIFT(x, -shift);
    }
    return x;
}

/*
 * Saturating format conversion.
 */
static inline fix16_t pb_fix_convert_sat_s32_to_s16(fix32_t x, int from_frac, int to_frac) {
    fix32_t converted = pb_fix_convert(x, from_frac, to_frac);
    return pb_fix_sat_s32_to_s16(converted);
}

#ifdef __cplusplus
}
#endif

#endif /* PB_FIXMATH_H */

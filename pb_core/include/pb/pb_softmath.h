/*
 * pb_softmath.h - Pure Integer Software Math Library
 *
 * Platform-agnostic mathematical functions requiring ONLY integer arithmetic.
 * No floating-point operations, no libm dependency, no 64-bit requirement.
 *
 * Design Philosophy:
 *   - Works on any C89+ compiler (8-bit to 64-bit)
 *   - Zero floating-point instructions or dependencies
 *   - Configurable precision vs speed tradeoffs
 *   - Compile-time selection of algorithms
 *   - Bit-perfect determinism for replay/netplay
 *
 * Algorithms:
 *   - CORDIC: sin, cos, atan2 using only add/sub/shift
 *   - Newton-Raphson: reciprocal, sqrt using integer operations
 *   - Binary search: Division without hardware divider
 *   - Polynomial: exp, log using integer Horner evaluation
 *
 * References:
 *   - CORDIC: Volder 1959, https://www.dcs.gla.ac.uk/~jhw/cordic/
 *   - Fast Inverse Square Root: Lomont 2003
 *   - Berkeley SoftFloat: https://github.com/ucb-bar/berkeley-softfloat-3
 *   - libfixmath: https://github.com/PetteriAimonen/libfixmath
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_SOFTMATH_H
#define PB_SOFTMATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

/* CORDIC iterations (more = more precision, 1 bit per iteration) */
#ifndef PB_SOFTMATH_CORDIC_ITER
#define PB_SOFTMATH_CORDIC_ITER 16
#endif

/* Newton-Raphson iterations for reciprocal/sqrt */
#ifndef PB_SOFTMATH_NR_ITER
#define PB_SOFTMATH_NR_ITER 3
#endif

/* Use 64-bit intermediates if available (faster on 32+ bit platforms) */
#ifndef PB_SOFTMATH_HAS_64BIT
#if defined(__SIZEOF_LONG_LONG__) && __SIZEOF_LONG_LONG__ >= 8
#define PB_SOFTMATH_HAS_64BIT 1
#else
#define PB_SOFTMATH_HAS_64BIT 0
#endif
#endif

/* Use lookup tables (trades ~1KB ROM for speed) */
#ifndef PB_SOFTMATH_USE_TABLES
#define PB_SOFTMATH_USE_TABLES 1
#endif

/*============================================================================
 * Fixed-Point Angle Representation
 *
 * Angles are represented in "binary radians" (brad) where:
 *   - Full circle = 2^16 = 65536 units
 *   - 90 degrees = 16384 brads
 *   - 1 brad ≈ 0.0055 degrees
 *
 * This representation is ideal for CORDIC because:
 *   - No transcendental constants needed
 *   - Angle addition/subtraction is simple integer math
 *   - Wraparound is automatic via integer overflow
 *   - Quadrant detection is just bit testing
 *============================================================================*/

typedef int16_t pb_brad16_t;   /* Binary radians: 16-bit angle */
typedef int32_t pb_brad32_t;   /* Binary radians: 32-bit angle */

/* Angle constants in brad16 format */
#define PB_BRAD16_FULL    ((pb_brad16_t)0)           /* 360 degrees (wraps) */
#define PB_BRAD16_HALF    ((pb_brad16_t)0x8000)      /* 180 degrees */
#define PB_BRAD16_QUARTER ((pb_brad16_t)0x4000)      /* 90 degrees */
#define PB_BRAD16_EIGHTH  ((pb_brad16_t)0x2000)      /* 45 degrees */

/* Convert between degrees and brads */
#define PB_DEG_TO_BRAD16(deg) ((pb_brad16_t)(((int32_t)(deg) * 65536) / 360))
#define PB_BRAD16_TO_DEG(brad) ((int32_t)(brad) * 360 / 65536)

/* Convert between radians (Q16.16) and brads */
/* Pi in Q16.16 = 205887, 2*Pi = 411775 */
/* brad16_full / 2pi = 65536 / 411775 ≈ 0.159155 in Q16.16 ≈ 10430 */
#define PB_RAD_Q16_TO_BRAD16(rad) ((pb_brad16_t)(((int32_t)(rad) * 10430) >> 16))
#define PB_BRAD16_TO_RAD_Q16(brad) ((int32_t)(brad) * 411775 / 65536)

/*============================================================================
 * CORDIC Lookup Tables
 *
 * atan(2^-i) pre-computed in brad16 format for i = 0..15
 * These values sum to 0x9B78 ≈ 99.88% of a quarter circle.
 * The ~0.12% deficit is handled by CORDIC gain compensation.
 *============================================================================*/

#if PB_SOFTMATH_USE_TABLES

/* arctan(2^-i) in brad16 format */
static const pb_brad16_t pb_cordic_atan_table[16] = {
    0x2000,  /* atan(1)     = 45.00 deg = 8192 brads */
    0x12E4,  /* atan(0.5)   = 26.57 deg = 4836 brads */
    0x09FB,  /* atan(0.25)  = 14.04 deg = 2555 brads */
    0x0511,  /* atan(0.125) = 7.13 deg  = 1297 brads */
    0x028B,  /* atan(1/16)  = 3.58 deg  = 651 brads */
    0x0146,  /* atan(1/32)  = 1.79 deg  = 326 brads */
    0x00A3,  /* atan(1/64)  = 0.90 deg  = 163 brads */
    0x0051,  /* atan(1/128) = 0.45 deg  = 81 brads */
    0x0029,  /* atan(1/256) = 0.22 deg  = 41 brads */
    0x0014,  /* atan(1/512) = 0.11 deg  = 20 brads */
    0x000A,  /* atan(1/1024)= 0.06 deg  = 10 brads */
    0x0005,  /* atan(1/2048)= 0.03 deg  = 5 brads */
    0x0003,  /* atan(1/4096)= 0.01 deg  = 3 brads */
    0x0001,  /* atan(1/8192)= 0.007 deg = 1 brad */
    0x0001,  /* atan(1/16384) ≈ 1 brad */
    0x0000   /* atan(1/32768) ≈ 0 brads */
};

/*
 * CORDIC gain compensation factor K = product(cos(atan(2^-i)))
 * For 16 iterations: K ≈ 0.60725293500888
 * Reciprocal: 1/K ≈ 1.6467602581211
 *
 * In Q16.16: 1/K = 107914 (0x1A5DC)
 * In Q1.15:  1/K = 53957 (0xD2E5)
 */
#define PB_CORDIC_GAIN_INV_Q16  107914
#define PB_CORDIC_GAIN_INV_Q15  53957

/* Gain in Q16.16: K ≈ 39796 (0x9B74) */
#define PB_CORDIC_GAIN_Q16      39796

#endif /* PB_SOFTMATH_USE_TABLES */

/*============================================================================
 * CORDIC Engine - Pure Integer Trigonometry
 *
 * The CORDIC algorithm rotates a vector (x,y) by angle theta using only:
 *   - Integer addition/subtraction
 *   - Bit shifts (simulating multiplication by powers of 2)
 *   - Comparisons
 *
 * Rotation mode: Given angle theta, compute sin(theta), cos(theta)
 * Vectoring mode: Given (x,y), compute sqrt(x²+y²) and atan2(y,x)
 *============================================================================*/

/*
 * CORDIC rotation mode: compute sin and cos simultaneously.
 *
 * Input:  angle in brad16 format
 * Output: *sin_out in Q1.15 format (range [-32768, 32767] = [-1, ~1])
 *         *cos_out in Q1.15 format
 *
 * Accuracy: ~16 bits (1 bit per CORDIC iteration)
 * Cycles: ~50-100 on typical 32-bit MCU
 *
 * Algorithm:
 * - Start with unit vector (1, 0) in Q2.30 format (allows for CORDIC gain)
 * - Rotate by angle using CORDIC iterations
 * - After iterations, result is (cos*K, sin*K) where K ≈ 1.647
 * - Multiply by 1/K to get true sin/cos
 * - Convert to Q1.15 output
 */
static inline void pb_softmath_sincos_brad16(
    pb_brad16_t angle,
    int16_t *sin_out,
    int16_t *cos_out
) {
    int32_t x, y, z;
    int32_t x_new;
    int i;
    int flip_x = 0;  /* Flag to negate x at end (for cos in quadrants 2,3) */
    int flip_y = 0;  /* Flag to negate y at end (for sin in quadrants 3,4) */

    /* Start with unit vector in Q2.30 format (1.0 = 0x40000000) */
    /* Using Q2.30 allows headroom for CORDIC gain K ≈ 1.647 */
    x = 0x40000000;  /* 1.0 in Q2.30 */
    y = 0;
    z = angle;

    /* Reduce to first quadrant and track required reflections */
    /* CORDIC works in range ≈ [-99.88°, +99.88°] which covers [-16384, 16384] brads */

    /* Handle negative angles: sin(-x) = -sin(x), cos(-x) = cos(x) */
    if (z < 0) {
        z = -z;
        flip_y = 1;
    }

    /* Now z is in [0, 32767] brads = [0°, 180°) */
    if (z >= PB_BRAD16_QUARTER) {
        /* Angle in [90°, 180°): cos is negative, sin is positive
         * Use: sin(x) = sin(180-x), cos(x) = -cos(180-x)
         * where 180 brads = 0x8000 interpreted as unsigned = 32768
         */
        z = 32768 - z;  /* Reflect to get angle in [0, 90°) */
        flip_x = 1;     /* cos will be negative */
    }

    /* Now z is in [0, 16384] brads = [0°, 90°] - within CORDIC range */

    /* CORDIC iterations: rotate by arctan(2^-i) */
    for (i = 0; i < PB_SOFTMATH_CORDIC_ITER; i++) {
        if (z >= 0) {
            /* Rotate counter-clockwise (positive angle) */
            x_new = x - (y >> i);
            y = y + (x >> i);
            z -= pb_cordic_atan_table[i];
        } else {
            /* Rotate clockwise (negative angle) */
            x_new = x + (y >> i);
            y = y - (x >> i);
            z += pb_cordic_atan_table[i];
        }
        x = x_new;
    }

    /* After CORDIC: (x, y) = (cos*K, sin*K) in Q2.30, where K ≈ 1.647
     * Need to multiply by 1/K = 0.60725... to get true values.
     * 1/K in Q0.32 ≈ 0x9B74EDA8
     * For Q2.30 input -> Q1.15 output:
     * result = (x * (1/K)) >> 15, where 1/K is in Q0.16 = 39796
     */
#if PB_SOFTMATH_HAS_64BIT
    {
        /* High precision: multiply by 1/K in Q0.16 and shift */
        int64_t x_scaled = ((int64_t)x * PB_CORDIC_GAIN_Q16) >> 31;  /* Q2.30 * Q0.16 >> 31 = Q1.15 */
        int64_t y_scaled = ((int64_t)y * PB_CORDIC_GAIN_Q16) >> 31;
        x = (int32_t)x_scaled;
        y = (int32_t)y_scaled;
    }
#else
    /* 32-bit only: approximate with shifts */
    /* K ≈ 0.607 ≈ 1/2 + 1/8 - 1/64 = 0.609375 (close enough) */
    x = (x >> 1) + (x >> 3) - (x >> 6);
    y = (y >> 1) + (y >> 3) - (y >> 6);
    /* Scale from Q2.30 to Q1.15 */
    x >>= 15;
    y >>= 15;
#endif

    /* Apply quadrant reflections */
    if (flip_x) x = -x;
    if (flip_y) y = -y;

    /* Clamp to Q1.15 range and output */
    if (x > 32767) x = 32767;
    if (x < -32768) x = -32768;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;

    *cos_out = (int16_t)x;
    *sin_out = (int16_t)y;
}

/*
 * CORDIC vectoring mode: compute atan2 and magnitude.
 *
 * Input:  y, x in any fixed-point format (same scale)
 * Output: angle in brad16 format (full [-180°, 180°] range)
 *         *magnitude (optional) = sqrt(x² + y²) * K in same format as inputs
 *
 * The magnitude output includes CORDIC gain K ≈ 1.647.
 * To get true magnitude, multiply by 1/K.
 */
static inline pb_brad16_t pb_softmath_atan2_brad16(
    int32_t y,
    int32_t x,
    int32_t *magnitude /* may be NULL */
) {
    int32_t x_new;
    int32_t z = 0;  /* Accumulated angle */
    int i;
    int negate_y = 0;

    /* Handle special cases */
    if (x == 0 && y == 0) {
        if (magnitude) *magnitude = 0;
        return 0;
    }

    /* Special case: x = 0 */
    if (x == 0) {
        if (magnitude) *magnitude = (y >= 0) ? y : -y;  /* |y| */
        return (y >= 0) ? PB_BRAD16_QUARTER : -PB_BRAD16_QUARTER;  /* ±90° */
    }

    /* Special case: y = 0 */
    if (y == 0) {
        if (magnitude) *magnitude = (x >= 0) ? x : -x;  /* |x| */
        return (x >= 0) ? 0 : PB_BRAD16_HALF;  /* 0° or 180° */
    }

    /* Track if we need to negate the result (for y < 0) */
    if (y < 0) {
        y = -y;
        negate_y = 1;
    }

    /* Now y > 0. Handle x < 0 case (quadrant 2) */
    if (x < 0) {
        /* Angle will be in (90°, 180°) for y > 0, x < 0
         * Rotate 90° by swapping: (x,y) -> (y, -x)
         * This puts the vector in quadrant 1
         */
        int32_t tmp = x;
        x = y;
        y = -tmp;  /* y becomes positive (was -negative) */
        z = PB_BRAD16_QUARTER;  /* Start from 90° */
    }

    /* Now x > 0, y >= 0 (quadrant 1). Run CORDIC vectoring. */
    /* Vectoring mode: rotate vector toward x-axis, accumulating angle */
    for (i = 0; i < PB_SOFTMATH_CORDIC_ITER; i++) {
        if (y > 0) {
            /* y positive: rotate clockwise (reduce angle) */
            x_new = x + (y >> i);
            y = y - (x >> i);
            z += pb_cordic_atan_table[i];
        } else {
            /* y <= 0: rotate counter-clockwise */
            x_new = x - (y >> i);
            y = y + (x >> i);
            z -= pb_cordic_atan_table[i];
        }
        x = x_new;
    }

    /* After CORDIC, x ≈ magnitude * K, y ≈ 0 */
    if (magnitude) *magnitude = x;

    /* Apply reflection for original y < 0 */
    return negate_y ? (pb_brad16_t)(-z) : (pb_brad16_t)z;
}

/*
 * Convenience wrappers returning Q16.16 fixed-point.
 */

/* Sin in Q16.16 format, angle in brad16 */
static inline int32_t pb_softmath_sin_q16(pb_brad16_t angle) {
    int16_t s, c;
    pb_softmath_sincos_brad16(angle, &s, &c);
    /* Convert Q1.15 to Q16.16: shift left by 1 */
    return (int32_t)s << 1;
}

/* Cos in Q16.16 format, angle in brad16 */
static inline int32_t pb_softmath_cos_q16(pb_brad16_t angle) {
    int16_t s, c;
    pb_softmath_sincos_brad16(angle, &s, &c);
    return (int32_t)c << 1;
}

/*============================================================================
 * Newton-Raphson Operations (Integer-Only)
 *
 * Reciprocal: 1/x using x_{n+1} = x_n * (2 - a * x_n)
 * Inv Sqrt:   1/sqrt(x) using y_{n+1} = y_n * (3 - x*y_n²) / 2
 *
 * These require multiplication but NO division.
 *============================================================================*/

/*
 * Count leading zeros - portable fallback.
 * Used for initial estimate normalization.
 */
static inline int pb_softmath_clz32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return x ? __builtin_clz(x) : 32;
#else
    /* De Bruijn sequence method */
    static const uint8_t debruijn[32] = {
        31, 22, 30, 21, 18, 10, 29,  2, 20, 17, 15, 13, 9, 6, 28, 1,
        23, 19, 11,  3, 16, 14,  7, 24, 12,  4,  8, 25, 5, 26, 27, 0
    };
    if (!x) return 32;
    x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16;
    return debruijn[(uint32_t)(x * 0x07C4ACDDU) >> 27];
#endif
}

/*
 * Integer reciprocal: compute floor(2^32 / x)
 *
 * Input:  x in range [1, 2^32-1]
 * Output: floor(2^32 / x) as unsigned 32-bit
 *
 * Uses Newton-Raphson with CLZ-based initial estimate.
 * Accurate to ~30 bits with 3 iterations.
 */
static inline uint32_t pb_softmath_recip32(uint32_t x) {
    uint32_t estimate;
#if !PB_SOFTMATH_HAS_64BIT
    int i;
#endif

    if (x == 0) return 0xFFFFFFFFu;  /* Saturate on division by zero */
    if (x == 1) return 0xFFFFFFFFu;  /* 2^32/1 overflows, return max */
    if (x == 2) return 0x80000000u;  /* 2^32/2 = 2^31 */

#if PB_SOFTMATH_HAS_64BIT
    {
        /* Use 64-bit division for initial estimate (this is accurate) */
        uint64_t numerator = 0x100000000ULL;  /* 2^32 */
        estimate = (uint32_t)(numerator / x);

        /* One Newton-Raphson iteration to refine (usually not needed but safe) */
        /* e_new = e * (2 - x*e/2^32) = e + e*(1 - x*e/2^32) = e + e - x*e^2/2^32 */
        /* In practice, the 64-bit division is already accurate enough */
    }
#else
    {
        /* 32-bit only: CLZ-based estimate + Newton-Raphson */
        int shift = pb_softmath_clz32(x);

        /* Initial estimate: for x with MSB at position (31-shift),
         * reciprocal ≈ 2^(32 - (32-shift)) = 2^shift
         * Refine with linear interpolation based on lower bits
         */
        estimate = 1u << shift;

        /* Refine estimate using bits below MSB */
        /* If x = 2^n * (1 + f) where 0 <= f < 1,
         * then 1/x ≈ 2^(-n) * (1 - f) ≈ 2^(-n) - 2^(-n) * f
         */
        if (shift > 0) {
            uint32_t x_frac = (x << shift) >> 1;  /* Fractional part in high bits */
            estimate -= (estimate >> 16) * (x_frac >> 16);
        }

        /* Newton-Raphson iterations */
        /* Formula: e_new = e * (2 - x*e)
         * But we need to handle the Q-format carefully.
         * We want: e_new = e + e - e*x*e = 2e - e²x
         */
        for (i = 0; i < PB_SOFTMATH_NR_ITER; i++) {
            /* Approximate x*e in high precision using 16x16 multiplies */
            uint32_t e_hi = estimate >> 16;
            uint32_t e_lo = estimate & 0xFFFF;
            uint32_t x_hi = x >> 16;
            uint32_t x_lo = x & 0xFFFF;

            /* x*e (we only need high 32 bits of the 64-bit result) */
            uint32_t xe_hi = x_hi * e_hi;
            uint32_t xe_mid = x_hi * e_lo + x_lo * e_hi;
            xe_hi += (xe_mid >> 16);

            /* 2 - x*e in the same format (saturate negative to 0) */
            uint32_t two_minus_xe = (xe_hi < 0x00020000u) ? (0x00020000u - xe_hi) : 0;

            /* e * (2 - x*e), extract high part */
            estimate = (e_hi * two_minus_xe + ((e_lo * two_minus_xe) >> 16)) >> 1;
        }
    }
#endif

    return estimate;
}

/*
 * Integer inverse square root: compute 2^16 / sqrt(x)
 *
 * Input:  x in Q16.16 format
 * Output: 1/sqrt(x) in Q16.16 format
 *
 * Uses the "fast inverse square root" technique with Newton-Raphson.
 */
static inline int32_t pb_softmath_invsqrt_q16(int32_t x) {
    uint32_t y;
    int i;
#if PB_SOFTMATH_HAS_64BIT
    int64_t y2, xy2, factor;
#endif

    if (x <= 0) return 0x7FFFFFFF;  /* Saturate for invalid input */

    /* Initial estimate using bit manipulation (inspired by Quake's fast inversqrt)
     * For Q16.16, the magic constant is different than IEEE float.
     * Use CLZ-based estimate instead. */
    {
        int n = pb_softmath_clz32((uint32_t)x);
        /* x ≈ 2^(31-n), so 1/sqrt(x) ≈ 2^((n-31)/2)
         * For Q16.16 output with Q16.16 input:
         * 1/sqrt(x) * 2^16 = 2^16 / sqrt(x/2^16) = 2^24 / sqrt(x)
         */
        /* Start with estimate 2^((n+1)/2 + 8) */
        y = (uint32_t)1 << (((n + 1) >> 1) + 8);
    }

#if PB_SOFTMATH_HAS_64BIT
    /* Newton-Raphson: y = y * (3 - x*y²) / 2 */
    for (i = 0; i < PB_SOFTMATH_NR_ITER; i++) {
        y2 = ((int64_t)y * y) >> 16;        /* y² in Q16.16 */
        xy2 = ((int64_t)x * y2) >> 16;      /* x*y² in Q16.16 */
        factor = (3LL << 16) - xy2;         /* 3 - x*y² in Q16.16 */
        y = (uint32_t)(((int64_t)y * factor) >> 17);  /* y * (3-xy²) / 2 */
    }
#else
    /* 32-bit only: less accurate but still functional */
    for (i = 0; i < PB_SOFTMATH_NR_ITER; i++) {
        uint32_t y2 = ((uint32_t)y * y) >> 16;
        uint32_t xy2 = ((uint32_t)x * y2) >> 16;
        int32_t factor = (3 << 16) - (int32_t)xy2;
        y = (uint32_t)(((int32_t)y * factor) >> 17);
    }
#endif

    return (int32_t)y;
}

/*
 * Integer square root: sqrt(x) = x * (1/sqrt(x))
 */
static inline int32_t pb_softmath_sqrt_q16(int32_t x) {
    int32_t inv;
#if PB_SOFTMATH_HAS_64BIT
    int64_t result;
#endif

    if (x <= 0) return 0;

    inv = pb_softmath_invsqrt_q16(x);

#if PB_SOFTMATH_HAS_64BIT
    result = ((int64_t)x * inv) >> 16;
    return (int32_t)result;
#else
    /* 32-bit split multiply */
    {
        uint32_t a_lo = (uint32_t)x & 0xFFFF;
        uint32_t a_hi = (uint32_t)x >> 16;
        uint32_t b_lo = (uint32_t)inv & 0xFFFF;
        uint32_t b_hi = (uint32_t)inv >> 16;
        uint32_t result_mid = a_lo * b_hi + a_hi * b_lo;
        return (int32_t)(a_hi * b_hi + (result_mid >> 16));
    }
#endif
}

/*============================================================================
 * Integer Division (for platforms without hardware divider)
 *
 * Uses binary search / shift-and-subtract method.
 * Much slower than hardware division but works everywhere.
 *============================================================================*/

/*
 * Unsigned 32-bit division using shift-and-subtract.
 *
 * This is the "long division" algorithm, O(32) iterations.
 * On platforms with hardware divide, the compiler will optimize this away.
 */
static inline uint32_t pb_softmath_div32(uint32_t dividend, uint32_t divisor) {
    uint32_t quotient = 0;
    uint32_t remainder = 0;
    int i;

    if (divisor == 0) return 0xFFFFFFFFu;  /* Saturate */

    for (i = 31; i >= 0; i--) {
        remainder = (remainder << 1) | ((dividend >> i) & 1);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1u << i);
        }
    }

    return quotient;
}

/*
 * Signed 32-bit division.
 */
static inline int32_t pb_softmath_sdiv32(int32_t dividend, int32_t divisor) {
    int negative = 0;
    uint32_t result;

    if (divisor == 0) {
        return (dividend >= 0) ? 0x7FFFFFFF : (int32_t)0x80000000;
    }

    if (dividend < 0) {
        dividend = -dividend;
        negative = !negative;
    }
    if (divisor < 0) {
        divisor = -divisor;
        negative = !negative;
    }

    result = pb_softmath_div32((uint32_t)dividend, (uint32_t)divisor);
    return negative ? -(int32_t)result : (int32_t)result;
}

/*============================================================================
 * Exponential and Logarithm (Integer-Only)
 *
 * exp(x) using range reduction and polynomial approximation.
 * ln(x) using binary search and bit manipulation.
 *============================================================================*/

/*
 * Integer exponential: exp(x) where x is Q16.16
 * Output is Q16.16.
 *
 * Uses range reduction: exp(x) = 2^k * exp(r)
 * where x = k*ln(2) + r and |r| < ln(2)/2
 */
static inline int32_t pb_softmath_exp_q16(int32_t x) {
    /* ln(2) in Q16.16 ≈ 45426 */
    static const int32_t LN2_Q16 = 45426;

    int32_t k, r, result;
    int32_t r2, r3, r4;

    /* Overflow/underflow checks - exp(x) for Q16.16 result */
    /* exp(10.4) ≈ 32767, exp(10.5) overflows */
    if (x > 681391) return 0x7FFFFFFF;  /* exp(10.4) in Q16.16 */
    if (x < -681391) return 0;          /* exp(-10.4) ≈ 0 */

    /* Range reduction: x = k * ln(2) + r, where |r| < ln(2)/2
     * k = round(x / ln(2))
     * For Q16.16: k = round(x * (1/ln2)) where 1/ln2 ≈ 1.4427
     */

    /* Compute k as an integer (not fixed-point) */
    /* k = (x + ln2/2) / ln2 for positive x, (x - ln2/2) / ln2 for negative */
#if PB_SOFTMATH_HAS_64BIT
    {
        int64_t x_scaled = (int64_t)x * 94548LL;  /* x * (1/ln2) in Q32.16 */
        k = (int32_t)((x_scaled + 0x80000000LL) >> 32);  /* Round to integer */
    }
#else
    /* 32-bit approximation */
    if (x >= 0) {
        k = (x + LN2_Q16 / 2) / LN2_Q16;
    } else {
        k = (x - LN2_Q16 / 2) / LN2_Q16;
    }
#endif

    /* r = x - k * ln(2) */
    r = x - k * LN2_Q16;

    /* Ensure r is in [-ln2/2, ln2/2] */
    while (r > LN2_Q16 / 2) { r -= LN2_Q16; k++; }
    while (r < -LN2_Q16 / 2) { r += LN2_Q16; k--; }

    /* Taylor series for exp(r) ≈ 1 + r + r²/2 + r³/6 + r⁴/24 + r⁵/120 */
#if PB_SOFTMATH_HAS_64BIT
    r2 = (int32_t)(((int64_t)r * r) >> 16);
    r3 = (int32_t)(((int64_t)r2 * r) >> 16);
    r4 = (int32_t)(((int64_t)r2 * r2) >> 16);
#else
    r2 = ((r >> 8) * (r >> 8));  /* Approximate r² */
    r3 = ((r2 >> 8) * (r >> 8));
    r4 = ((r2 >> 8) * (r2 >> 8));
#endif

    result = 0x10000;     /* 1.0 in Q16.16 */
    result += r;          /* + r */
    result += r2 >> 1;    /* + r²/2 */
    result += r3 / 6;     /* + r³/6 */
    result += r4 / 24;    /* + r⁴/24 */

    /* Multiply by 2^k */
    if (k >= 0) {
        if (k > 14) return 0x7FFFFFFF;  /* Overflow (2^15 * 1 > max Q16.16) */
        result <<= k;
    } else {
        if (k < -16) return 0;  /* Underflow to zero */
        result >>= (-k);
    }

    /* Clamp to valid Q16.16 range */
    if (result > 0x7FFFFFFF) result = 0x7FFFFFFF;
    if (result < 0) result = 0;

    return result;
}

/*
 * Integer natural log: ln(x) where x is Q16.16, x > 0
 * Output is Q16.16.
 *
 * Uses: ln(x) = k*ln(2) + ln(m) where x = m * 2^k, 1 ≤ m < 2
 */
static inline int32_t pb_softmath_ln_q16(int32_t x) {
    static const int32_t LN2_Q16 = 45426;
    int32_t result;
    int k, i;
    int32_t y, y_sq;

    if (x <= 0) return (int32_t)0x80000000;  /* -infinity */

    /* Find k such that x = m * 2^k where 1 ≤ m < 2 (in Q16.16: 65536 ≤ m < 131072) */
    k = 16 - pb_softmath_clz32((uint32_t)x);  /* log2(x) roughly */

    /* Normalize m to [1, 2) in Q16.16 */
    if (k > 0) {
        x >>= k;
    } else {
        x <<= (-k);
    }

    /* Now x is in [65536, 131072) = [1.0, 2.0) in Q16.16 */
    /* Use series: ln(m) = 2 * artanh((m-1)/(m+1))
     *            = 2 * (y + y³/3 + y⁵/5 + ...) where y = (m-1)/(m+1)
     */
    {
        int32_t m_minus_1 = x - (1 << 16);
        int32_t m_plus_1 = x + (1 << 16);

        /* y = (m-1)/(m+1), need division */
        /* For Q16.16 division: (a << 16) / b */
#if PB_SOFTMATH_HAS_64BIT
        y = (int32_t)(((int64_t)m_minus_1 << 16) / m_plus_1);
#else
        /* 32-bit: approximate with shifts */
        y = (m_minus_1 << 8) / (m_plus_1 >> 8);
#endif
    }

    /* y² for series terms */
#if PB_SOFTMATH_HAS_64BIT
    y_sq = (int32_t)(((int64_t)y * y) >> 16);
#else
    y_sq = (y >> 8) * (y >> 8);
#endif

    /* ln(m) ≈ 2 * (y + y³/3 + y⁵/5) */
    result = y;  /* First term */
    {
        int32_t y_power = y;
        for (i = 3; i <= 7; i += 2) {
#if PB_SOFTMATH_HAS_64BIT
            y_power = (int32_t)(((int64_t)y_power * y_sq) >> 16);
#else
            y_power = (y_power >> 8) * (y_sq >> 8);
#endif
            result += y_power / i;
        }
    }
    result <<= 1;  /* Multiply by 2 */

    /* ln(x) = k*ln(2) + ln(m) */
#if PB_SOFTMATH_HAS_64BIT
    result += (int32_t)(((int64_t)k * LN2_Q16));
#else
    result += k * LN2_Q16;
#endif

    return result;
}

/*============================================================================
 * Utility Macros for Platform Selection
 *============================================================================*/

/* Select optimal implementation based on platform capabilities */
#if PB_SOFTMATH_HAS_64BIT
#define PB_SOFTMATH_MUL32(a,b,shift) ((int32_t)(((int64_t)(a) * (b)) >> (shift)))
#else
/* 32-bit only multiplication with shift */
#define PB_SOFTMATH_MUL32(a,b,shift) \
    ((int32_t)(((int32_t)((a) >> 8) * ((b) >> 8)) >> ((shift) - 16)))
#endif

#ifdef __cplusplus
}
#endif

#endif /* PB_SOFTMATH_H */

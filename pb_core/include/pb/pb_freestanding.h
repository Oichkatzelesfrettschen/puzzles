/*
 * pb_freestanding.h - Minimal C library replacements for freestanding environments
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * When PB_FREESTANDING is defined, this header provides minimal implementations
 * of commonly-used C library functions (memset, memcpy, sqrt, sin, cos).
 *
 * These are NOT full replacements - they're sized for the limited needs of
 * pb_core on embedded systems (8-bit AVR, Z80, 6502, Game Boy, etc.).
 */

#ifndef PB_FREESTANDING_H
#define PB_FREESTANDING_H

#include "pb_config.h"

/* Freestanding requires stddef.h for NULL and size_t */
#include <stddef.h>

/*============================================================================
 * Automatic Freestanding Detection
 *============================================================================*/

/* Auto-detect freestanding if using known embedded compilers */
#ifndef PB_FREESTANDING
    #if PB_PLATFORM_8BIT || PB_PLATFORM_FREESTANDING
        #define PB_FREESTANDING 1
    #else
        #define PB_FREESTANDING 0
    #endif
#endif

/*============================================================================
 * Hosted Environment - Use Standard Headers
 *============================================================================*/

#if !PB_FREESTANDING

#include <string.h>
#include <math.h>

/* Pass-through macros to standard functions */
#define pb_memset   memset
#define pb_memcpy   memcpy
#define pb_memcmp   memcmp
#define pb_strlen   strlen
#define pb_strncpy  strncpy
#define pb_sqrt     sqrt
#define pb_sqrtf    sqrtf
#define pb_sin      sin
#define pb_sinf     sinf
#define pb_cos      cos
#define pb_cosf     cosf
#define pb_floor    floor
#define pb_floorf   floorf
#define pb_fabs     fabs
#define pb_fabsf    fabsf
#define pb_round    round
#define pb_roundf   roundf
#define pb_powf     powf
#define pb_cbrtf    cbrtf
#define pb_atan2f   atan2f
#define pb_fmodf    fmodf

#else /* PB_FREESTANDING */

/*============================================================================
 * Freestanding Memory Functions
 *============================================================================*/

/* Use builtin if GCC/Clang, otherwise provide simple implementation */
#if defined(__GNUC__) || defined(__clang__)

#define pb_memset(dest, c, n)  __builtin_memset((dest), (c), (n))
#define pb_memcpy(dest, src, n) __builtin_memcpy((dest), (src), (n))
#define pb_memcmp(a, b, n)     __builtin_memcmp((a), (b), (n))

#else /* Non-GCC freestanding */

PB_INLINE void* pb_memset(void* dest, int c, unsigned int n)
{
    unsigned char* d = (unsigned char*)dest;
    while (n--) {
        *d++ = (unsigned char)c;
    }
    return dest;
}

PB_INLINE void* pb_memcpy(void* dest, const void* src, unsigned int n)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

PB_INLINE int pb_memcmp(const void* a, const void* b, unsigned int n)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--) {
        if (*pa != *pb) {
            return (*pa < *pb) ? -1 : 1;
        }
        pa++;
        pb++;
    }
    return 0;
}

#endif /* __GNUC__ */

PB_INLINE unsigned int pb_strlen(const char* s)
{
    unsigned int len = 0;
    while (*s++) len++;
    return len;
}

PB_INLINE char* pb_strncpy(char* dest, const char* src, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

/*============================================================================
 * Freestanding Math Functions (Fixed-Point Approximations)
 *============================================================================*/

/*
 * Q16.16 fixed-point math for freestanding.
 * These are integer-only approximations suitable for game physics.
 *
 * The fixed-point sqrt uses Newton-Raphson iteration.
 * Trig functions use lookup tables or Taylor series approximations.
 */

#include <stdint.h>

/* Fixed-point type (Q16.16) */
typedef int32_t pb_fixed_t;
#define PB_FIXED_ONE    ((pb_fixed_t)1 << 16)
#define PB_FIXED_HALF   ((pb_fixed_t)1 << 15)

/* Convert between fixed and float (compile-time only for constants) */
#define PB_FP_FROM_FLOAT(f)  ((pb_fixed_t)((f) * PB_FIXED_ONE))
#define PB_FP_TO_FLOAT(fp)   ((float)(fp) / PB_FIXED_ONE)

/* Fixed-point multiply */
PB_INLINE pb_fixed_t pb_fp_mul(pb_fixed_t a, pb_fixed_t b)
{
    int64_t result = (int64_t)a * b;
    return (pb_fixed_t)(result >> 16);
}

/* Fixed-point divide */
PB_INLINE pb_fixed_t pb_fp_div(pb_fixed_t a, pb_fixed_t b)
{
    int64_t temp = (int64_t)a << 16;
    return (pb_fixed_t)(temp / b);
}

/*
 * Fixed-point square root using Newton-Raphson.
 * Input: Q16.16 fixed-point >= 0
 * Output: Q16.16 fixed-point sqrt
 */
PB_INLINE pb_fixed_t pb_fp_sqrt(pb_fixed_t x)
{
    if (x <= 0) return 0;

    /* Initial guess: x/2 (shift for fixed-point scaling) */
    pb_fixed_t guess = x;
    if (guess > PB_FIXED_ONE) {
        guess = PB_FIXED_ONE + ((x - PB_FIXED_ONE) >> 1);
    }

    /* Newton-Raphson iterations: guess = (guess + x/guess) / 2 */
    for (int i = 0; i < 8; i++) {
        if (guess == 0) break;
        pb_fixed_t next = (guess + pb_fp_div(x, guess)) >> 1;
        if (next == guess) break;
        guess = next;
    }

    return guess;
}

/*
 * Sine lookup table (256 entries, Q15 format).
 * sin(i * 2*PI / 256) * 32767
 */
static const int16_t pb_sin_table[256] = {
         0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
      6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
     12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
     18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
     23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
     27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
     30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
     32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
     32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
     32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
     30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
     27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
     23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
     18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
     12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,
      6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
         0,  -804, -1608, -2410, -3212, -4011, -4808, -5602,
     -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
    -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,
    -18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
    -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
    -27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
    -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,
    -32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
    -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
    -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
    -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,
    -27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
    -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,
    -18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
    -12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179,
     -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804
};

/* PI in Q16.16: 205887 = 3.14159... * 65536 */
#define PB_FP_PI      205887
#define PB_FP_2PI     411775
#define PB_FP_HALF_PI 102944

/*============================================================================
 * Game-Specific Angle Tables (HP48-inspired)
 *
 * Uses 79-entry lookup tables covering only firing angles (12°-168°).
 * This saves 70% memory compared to full 256-entry tables.
 * Values extracted from hp48-puzzle-bobble implementation.
 *
 * Format: Q12 fixed-point (value * 4096)
 * Index 0 = 12°, Index 78 = 168°, Step = 2°
 *============================================================================*/

#if PB_USE_GAME_ANGLE_TABLE

/*
 * Sine lookup for firing angles only (12° to 168° in 2° steps)
 * Q12 format: sin(angle) * 4096
 * Index N corresponds to angle (12 + N*2) degrees
 */
static const int16_t pb_game_sin_table[79] = {
    /* 12°-26° */   852,  991, 1129, 1266, 1401, 1534, 1666, 1796,
    /* 28°-42° */  1923, 2048, 2171, 2290, 2407, 2521, 2631, 2739,
    /* 44°-58° */  2842, 2942, 3039, 3131, 3219, 3303, 3383, 3458,
    /* 60°-74° */  3528, 3593, 3653, 3708, 3758, 3802, 3841, 3875,
    /* 76°-90° */  3902, 3924, 3940, 3951, 3956, 3956, 3951, 4096,
    /* 92°-106° */ 4094, 4086, 4074, 4056, 4034, 4006, 3974, 3937,
    /* 108°-122° */3895, 3848, 3797, 3742, 3682, 3618, 3550, 3478,
    /* 124°-138° */3401, 3321, 3237, 3149, 3058, 2963, 2865, 2763,
    /* 140°-154° */2658, 2550, 2439, 2325, 2209, 2089, 1967, 1843,
    /* 156°-168° */1716, 1588, 1457, 1325, 1191, 1056,  920
};

/*
 * Cosine lookup for firing angles only (12° to 168° in 2° steps)
 * Q12 format: cos(angle) * 4096
 * Note: Cosine values go negative after 90°
 */
static const int16_t pb_game_cos_table[79] = {
    /* 12°-26° */  4006, 3974, 3937, 3895, 3849, 3798, 3742, 3681,
    /* 28°-42° */  3617, 3547, 3474, 3396, 3314, 3228, 3138, 3044,
    /* 44°-58° */  2946, 2845, 2740, 2632, 2521, 2407, 2290, 2171,
    /* 60°-74° */  2048, 1923, 1796, 1666, 1534, 1401, 1266, 1129,
    /* 76°-90° */   991,  852,  711,  570,  428,  286,  143,    0,
    /* 92°-106° */ -143, -286, -428, -570, -711, -852, -991,-1129,
    /* 108°-122° */-1266,-1401,-1534,-1666,-1796,-1923,-2048,-2170,
    /* 124°-138° */-2290,-2407,-2521,-2632,-2740,-2845,-2946,-3044,
    /* 140°-154° */-3138,-3228,-3314,-3396,-3474,-3547,-3617,-3681,
    /* 156°-168° */-3742,-3798,-3849,-3895,-3937,-3974,-4006
};

/*
 * Game-specific trig functions using direction index.
 * Input: direction index 0-78 (maps to 12°-168°)
 * Output: Q12 or Q16 fixed-point value
 *
 * Note: Uses uint8_t instead of pb_direction_index because this header
 * is included before pb_types.h defines that type.
 */
PB_INLINE int16_t pb_game_sin(uint8_t idx) {
    if (idx > 78) idx = 78;
    return pb_game_sin_table[idx];
}

PB_INLINE int16_t pb_game_cos(uint8_t idx) {
    if (idx > 78) idx = 78;
    return pb_game_cos_table[idx];
}

#endif /* PB_USE_GAME_ANGLE_TABLE */

/*
 * Fixed-point sine.
 * Input: angle in Q16.16 radians
 * Output: sin(angle) in Q16.16 (-65536 to +65536)
 */
PB_INLINE pb_fixed_t pb_fp_sin(pb_fixed_t angle)
{
    /* Normalize angle to [0, 2*PI) */
    while (angle < 0) angle += PB_FP_2PI;
    while (angle >= PB_FP_2PI) angle -= PB_FP_2PI;

    /* Convert to table index (0-255) */
    /* index = angle * 256 / (2*PI) = angle * 256 / 411775 */
    int idx = (int)((int64_t)angle * 256 / PB_FP_2PI) & 0xFF;

    /* Look up and scale from Q15 to Q16 */
    return ((pb_fixed_t)pb_sin_table[idx]) << 1;
}

/* Fixed-point cosine: cos(x) = sin(x + PI/2) */
PB_INLINE pb_fixed_t pb_fp_cos(pb_fixed_t angle)
{
    return pb_fp_sin(angle + PB_FP_HALF_PI);
}

/* Fixed-point absolute value */
PB_INLINE pb_fixed_t pb_fp_abs(pb_fixed_t x)
{
    return (x < 0) ? -x : x;
}

/* Fixed-point floor (truncate toward negative infinity) */
PB_INLINE pb_fixed_t pb_fp_floor(pb_fixed_t x)
{
    if (x >= 0) {
        return x & ~(PB_FIXED_ONE - 1);
    } else {
        return (x - PB_FIXED_ONE + 1) & ~(PB_FIXED_ONE - 1);
    }
}

/*
 * Float-style wrappers (convert float args to fixed, call fixed version)
 * These are for compatibility - prefer pb_fp_* directly for efficiency.
 */
#define pb_sqrt(x)   PB_FP_TO_FLOAT(pb_fp_sqrt(PB_FP_FROM_FLOAT(x)))
#define pb_sqrtf(x)  ((float)pb_sqrt(x))
#define pb_sin(x)    PB_FP_TO_FLOAT(pb_fp_sin(PB_FP_FROM_FLOAT(x)))
#define pb_sinf(x)   ((float)pb_sin(x))
#define pb_cos(x)    PB_FP_TO_FLOAT(pb_fp_cos(PB_FP_FROM_FLOAT(x)))
#define pb_cosf(x)   ((float)pb_cos(x))
#define pb_floor(x)  PB_FP_TO_FLOAT(pb_fp_floor(PB_FP_FROM_FLOAT(x)))
#define pb_floorf(x) ((float)pb_floor(x))
#define pb_fabs(x)   PB_FP_TO_FLOAT(pb_fp_abs(PB_FP_FROM_FLOAT(x)))
#define pb_fabsf(x)  ((float)pb_fabs(x))

/* Round: floor(x + 0.5) */
#define pb_round(x)  pb_floor((x) + 0.5)
#define pb_roundf(x) ((float)pb_round(x))

/*============================================================================
 * Additional Math Functions (for color/CVD - less critical on embedded)
 *============================================================================*/

/*
 * Fixed-point power function approximation.
 * For freestanding, we use a simple integer power loop for integer exponents,
 * or return 1.0 for fractional exponents (not accurate but prevents linker errors).
 */
PB_INLINE float pb_powf_approx(float base, float exp)
{
    /* Handle common cases */
    if (exp == 0.0f) return 1.0f;
    if (exp == 1.0f) return base;
    if (exp == 2.0f) return base * base;
    if (exp == 0.5f) return (float)pb_sqrtf(base);

    /* For gamma correction (2.4), use sqrt approximation: x^2.4 ≈ x^2 * sqrt(x) */
    if (exp > 2.0f && exp < 3.0f) {
        return base * base * (float)pb_sqrtf(base);
    }

    /* Fallback: linear approximation (not accurate!) */
    return base;
}

/*
 * Cube root approximation using Newton-Raphson.
 * cbrt(x) = x^(1/3)
 */
PB_INLINE float pb_cbrtf_approx(float x)
{
    if (x == 0.0f) return 0.0f;

    float sign = (x < 0.0f) ? -1.0f : 1.0f;
    x = (x < 0.0f) ? -x : x;

    /* Initial guess using sqrt: cbrt(x) ≈ sqrt(x) for x near 1 */
    float guess = (float)pb_sqrtf(x);
    if (guess < 0.1f) guess = 0.5f;

    /* Newton-Raphson: y = (2*y + x/(y*y)) / 3 */
    for (int i = 0; i < 6; i++) {
        float y2 = guess * guess;
        if (y2 < 0.0001f) break;
        guess = (2.0f * guess + x / y2) / 3.0f;
    }

    return sign * guess;
}

/*
 * atan2 approximation using polynomial.
 * Returns angle in radians.
 */
PB_INLINE float pb_atan2f_approx(float y, float x)
{
    const float PI = 3.14159265358979323846f;
    const float PI_2 = PI / 2.0f;

    if (x == 0.0f) {
        if (y > 0.0f) return PI_2;
        if (y < 0.0f) return -PI_2;
        return 0.0f;
    }

    float abs_y = (y < 0.0f) ? -y : y;
    float abs_x = (x < 0.0f) ? -x : x;
    float a = (abs_x < abs_y) ? abs_x / abs_y : abs_y / abs_x;
    float s = a * a;

    /* Polynomial approximation for atan */
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;

    if (abs_y > abs_x) r = PI_2 - r;
    if (x < 0.0f) r = PI - r;
    if (y < 0.0f) r = -r;

    return r;
}

/*
 * Floating-point modulo.
 */
PB_INLINE float pb_fmodf_approx(float x, float y)
{
    if (y == 0.0f) return 0.0f;
    return x - (float)((int)(x / y)) * y;
}

/* Wrappers for standard names */
#define pb_powf(b, e)   pb_powf_approx((b), (e))
#define pb_cbrtf(x)     pb_cbrtf_approx(x)
#define pb_atan2f(y, x) pb_atan2f_approx((y), (x))
#define pb_fmodf(x, y)  pb_fmodf_approx((x), (y))

/*============================================================================
 * Freestanding Standard Function Aliases
 * Map standard function names to pb_* versions for source compatibility
 *============================================================================*/

#define memset  pb_memset
#define memcpy  pb_memcpy
#define memcmp  pb_memcmp
#define strlen  pb_strlen
#define strncpy pb_strncpy
#define sqrtf   pb_sqrtf
#define sqrt    pb_sqrt
#define sinf    pb_sinf
#define sin     pb_sin
#define cosf    pb_cosf
#define cos     pb_cos
#define fabsf   pb_fabsf
#define fabs    pb_fabs
#define floorf  pb_floorf
#define floor   pb_floor
#define roundf  pb_roundf
#define round   pb_round
#define powf    pb_powf
#define cbrtf   pb_cbrtf
#define atan2f  pb_atan2f
#define fmodf   pb_fmodf

#endif /* PB_FREESTANDING */

#endif /* PB_FREESTANDING_H */

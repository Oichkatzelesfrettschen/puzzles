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
 * Freestanding Standard Function Aliases
 * Map standard function names to pb_* versions for source compatibility
 *============================================================================*/

#define memset  pb_memset
#define memcpy  pb_memcpy
#define memcmp  pb_memcmp
#define strlen  pb_strlen
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

#endif /* PB_FREESTANDING */

#endif /* PB_FREESTANDING_H */

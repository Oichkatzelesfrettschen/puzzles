/*
 * pb_compat.h - C Standard Compatibility Layer
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Provides compatibility shims for:
 *   C17 -> C11 -> C99 -> C89/ANSI -> K&R
 *
 * Include this AFTER pb_config.h and BEFORE any other pb headers.
 */

#ifndef PB_COMPAT_H
#define PB_COMPAT_H

#include "pb_config.h"

/*============================================================================
 * C89 Compatibility: bool type
 *============================================================================*/

#if defined(PB_C99) || defined(PB_C11) || defined(PB_C17)
    #include <stdbool.h>
#else
    /* C89: Define bool as int */
    #ifndef __bool_true_false_are_defined
        #define __bool_true_false_are_defined 1
        typedef int bool;
        #define true  1
        #define false 0
    #endif
#endif

/*============================================================================
 * C89 Compatibility: inline keyword
 *============================================================================*/

#if defined(PB_C99) || defined(PB_C11) || defined(PB_C17)
    #define PB_INLINE static inline
#elif defined(__GNUC__)
    /* GCC extension for C89 */
    #define PB_INLINE static __inline__
#elif defined(_MSC_VER)
    #define PB_INLINE static __inline
#else
    /* C89 fallback: no inline, just static */
    #define PB_INLINE static
#endif

/*============================================================================
 * C89 Compatibility: restrict keyword
 *============================================================================*/

#if defined(PB_C99) || defined(PB_C11) || defined(PB_C17)
    #define PB_RESTRICT restrict
#elif defined(__GNUC__)
    #define PB_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define PB_RESTRICT __restrict
#else
    #define PB_RESTRICT
#endif

/*============================================================================
 * C11 Compatibility: _Static_assert
 *============================================================================*/

#if defined(PB_C11) || defined(PB_C17)
    #define PB_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
    /* GCC extension */
    #define PB_STATIC_ASSERT(cond, msg) \
        typedef char pb_static_assert_##__LINE__[(cond) ? 1 : -1] PB_UNUSED
#else
    /* No static assert available */
    #define PB_STATIC_ASSERT(cond, msg) /* nothing */
#endif

/*============================================================================
 * C99 Compatibility: Designated Initializers
 *
 * For C89, we provide macros that expand to ordered initializer lists.
 * This requires careful ordering of struct members.
 *============================================================================*/

#if defined(PB_C99) || defined(PB_C11) || defined(PB_C17)
    /* C99+: Use native designated initializers */
    #define PB_DESIGNATED_INIT 1
    #define PB_INIT_FIELD(name, val) .name = val
#else
    /* C89: No designated initializers - must use positional */
    #define PB_DESIGNATED_INIT 0
    #define PB_INIT_FIELD(name, val) val
#endif

/*============================================================================
 * C99 Compatibility: Compound Literals
 *============================================================================*/

#if defined(PB_C99) || defined(PB_C11) || defined(PB_C17)
    #define PB_COMPOUND_LITERAL 1
    #define PB_LITERAL(type, ...) ((type){__VA_ARGS__})
#else
    /* C89: No compound literals - must use static variables */
    #define PB_COMPOUND_LITERAL 0
    /* Users must declare named variables instead */
#endif

/*============================================================================
 * C99 Compatibility: Variable-length arrays (VLAs)
 *
 * We intentionally AVOID VLAs for portability (optional in C11+).
 * This is just for detection/warning purposes.
 *============================================================================*/

#if defined(PB_C11) || defined(PB_C17)
    #ifdef __STDC_NO_VLA__
        #define PB_HAS_VLA 0
    #else
        #define PB_HAS_VLA 1
    #endif
#elif defined(PB_C99)
    #define PB_HAS_VLA 1
#else
    #define PB_HAS_VLA 0
#endif

/* We don't use VLAs - this is a policy check */
#define PB_VLA_POLICY_FORBID 1

/*============================================================================
 * Fixed-width Integer Types
 *============================================================================*/

#if defined(PB_C99) || defined(PB_C11) || defined(PB_C17)
    #include <stdint.h>
    #include <inttypes.h>
#else
    /* C89: Provide fallback definitions */
    #ifndef _STDINT_H
        typedef signed char        int8_t;
        typedef unsigned char      uint8_t;
        typedef short              int16_t;
        typedef unsigned short     uint16_t;
        typedef int                int32_t;
        typedef unsigned int       uint32_t;
        #if defined(__GNUC__) || defined(_MSC_VER)
            typedef long long          int64_t;
            typedef unsigned long long uint64_t;
        #endif

        #define INT8_MIN   (-128)
        #define INT8_MAX   127
        #define UINT8_MAX  255
        #define INT16_MIN  (-32768)
        #define INT16_MAX  32767
        #define UINT16_MAX 65535
        #define INT32_MIN  (-2147483647-1)
        #define INT32_MAX  2147483647
        #define UINT32_MAX 4294967295U
    #endif
#endif

/*============================================================================
 * Size Types
 *============================================================================*/

#include <stddef.h>  /* size_t, ptrdiff_t - available in all C standards */

/*============================================================================
 * Compiler Version Detection
 *============================================================================*/

#if defined(__GNUC__)
    #define PB_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #define PB_GCC_VERSION 0
#endif

#if defined(__clang__)
    #define PB_CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#else
    #define PB_CLANG_VERSION 0
#endif

/*============================================================================
 * Endianness Detection
 *============================================================================*/

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define PB_LITTLE_ENDIAN 1
        #define PB_BIG_ENDIAN    0
    #else
        #define PB_LITTLE_ENDIAN 0
        #define PB_BIG_ENDIAN    1
    #endif
#elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || \
      defined(__aarch64__) || defined(__arm__) || defined(__riscv)
    /* Common little-endian platforms */
    #define PB_LITTLE_ENDIAN 1
    #define PB_BIG_ENDIAN    0
#else
    /* Assume big-endian for unknown (safer for network/file I/O) */
    #define PB_LITTLE_ENDIAN 0
    #define PB_BIG_ENDIAN    1
#endif

/*============================================================================
 * Diagnostic Helpers
 *============================================================================*/

/* Stringify macro for error messages */
#define PB_STRINGIFY(x) PB_STRINGIFY_(x)
#define PB_STRINGIFY_(x) #x

/* Concatenation macro */
#define PB_CONCAT(a, b) PB_CONCAT_(a, b)
#define PB_CONCAT_(a, b) a##b

/* Compile-time info string */
#define PB_BUILD_INFO \
    "pb_core C" PB_STRINGIFY(PB_C_STD_VERSION) " " PB_SIZE_TIER

#endif /* PB_COMPAT_H */

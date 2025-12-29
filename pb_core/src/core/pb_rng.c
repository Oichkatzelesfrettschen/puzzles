/*
 * pb_rng.c - xoshiro128** implementation
 *
 * Based on the reference implementation by David Blackman and Sebastiano Vigna.
 * Public domain / CC0.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_rng.h"
#include "pb/pb_config.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static PB_INLINE uint32_t rotl(uint32_t x, int k)
{
    return (x << k) | (x >> (32 - k));
}

/*
 * SplitMix64 constants (C89-safe: avoid ULL suffix).
 * These are 64-bit hex values split for strict C89 compatibility.
 */
#define SM64_GAMMA   ((uint64_t)0x9e3779b9 << 32 | (uint64_t)0x7f4a7c15)
#define SM64_MIX1    ((uint64_t)0xbf58476d << 32 | (uint64_t)0x1ce4e5b9)
#define SM64_MIX2    ((uint64_t)0x94d049bb << 32 | (uint64_t)0x133111eb)

/* SplitMix64 for seed expansion */
static uint64_t splitmix64(uint64_t* state)
{
    uint64_t z = (*state += SM64_GAMMA);
    z = (z ^ (z >> 30)) * SM64_MIX1;
    z = (z ^ (z >> 27)) * SM64_MIX2;
    return z ^ (z >> 31);
}

/*============================================================================
 * RNG Initialization
 *============================================================================*/

void pb_rng_seed(pb_rng* rng, uint64_t seed)
{
    uint64_t s = seed;

    /* Use SplitMix64 to fill the state */
    uint64_t r0 = splitmix64(&s);
    uint64_t r1 = splitmix64(&s);

    rng->state[0] = (uint32_t)(r0);
    rng->state[1] = (uint32_t)(r0 >> 32);
    rng->state[2] = (uint32_t)(r1);
    rng->state[3] = (uint32_t)(r1 >> 32);

    /* Ensure state is never all zeros */
    if (rng->state[0] == 0 && rng->state[1] == 0 &&
        rng->state[2] == 0 && rng->state[3] == 0) {
        rng->state[0] = 1;
    }
}

void pb_rng_set_state(pb_rng* rng, const uint32_t state[4])
{
    memcpy(rng->state, state, sizeof(rng->state));
}

void pb_rng_get_state(const pb_rng* rng, uint32_t state[4])
{
    memcpy(state, rng->state, sizeof(rng->state));
}

/*============================================================================
 * xoshiro128** Core
 *============================================================================*/

uint32_t pb_rng_next(pb_rng* rng)
{
    uint32_t* s = rng->state;

    /* xoshiro128** scrambler */
    const uint32_t result = rotl(s[1] * 5, 7) * 9;

    /* State update */
    const uint32_t t = s[1] << 9;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rotl(s[3], 11);

    return result;
}

/*============================================================================
 * Range Generation
 *============================================================================*/

uint32_t pb_rng_range(pb_rng* rng, uint32_t max)
{
    if (max == 0) {
        return 0;
    }

    /* Rejection sampling to avoid modulo bias */
    uint32_t threshold = (uint32_t)(-(int32_t)max) % max;

    for (;;) {
        uint32_t r = pb_rng_next(rng);
        if (r >= threshold) {
            return r % max;
        }
    }
}

int pb_rng_range_int(pb_rng* rng, int min, int max)
{
    if (min >= max) {
        return min;
    }

    uint32_t range = (uint32_t)(max - min + 1);
    return min + (int)pb_rng_range(rng, range);
}

float pb_rng_float(pb_rng* rng)
{
    /* Generate float in [0, 1) with 24 bits of precision */
    return (float)(pb_rng_next(rng) >> 8) * (1.0f / 16777216.0f);
}

float pb_rng_float_range(pb_rng* rng, float min, float max)
{
    return min + pb_rng_float(rng) * (max - min);
}

/*============================================================================
 * Shuffle
 *============================================================================*/

void pb_rng_shuffle(pb_rng* rng, void* array, size_t count, size_t elem_size)
{
    if (count <= 1 || elem_size == 0) {
        return;
    }

    uint8_t* arr = (uint8_t*)array;
    uint8_t temp[256];  /* Stack buffer for small elements */

    for (size_t i = count - 1; i > 0; i--) {
        size_t j = pb_rng_range(rng, (uint32_t)(i + 1));
        if (i != j) {
            uint8_t* a = arr + i * elem_size;
            uint8_t* b = arr + j * elem_size;

            if (elem_size <= sizeof(temp)) {
                /* Use stack buffer for small elements */
                memcpy(temp, a, elem_size);
                memcpy(a, b, elem_size);
                memcpy(b, temp, elem_size);
            } else {
                /* Byte-by-byte swap for large elements */
                for (size_t k = 0; k < elem_size; k++) {
                    uint8_t t = a[k];
                    a[k] = b[k];
                    b[k] = t;
                }
            }
        }
    }
}

/*============================================================================
 * Game-Specific Helpers
 *============================================================================*/

int pb_rng_pick_color(pb_rng* rng, uint8_t allowed_mask)
{
    if (allowed_mask == 0) {
        return -1;
    }

    /* Count set bits (popcount) */
    int count = 0;
    uint8_t mask = allowed_mask;
    while (mask) {
        count += mask & 1;
        mask >>= 1;
    }

    /* Pick a random index among the set bits */
    int pick = (int)pb_rng_range(rng, (uint32_t)count);

    /* Find the pick-th set bit */
    int idx = 0;
    mask = allowed_mask;
    while (mask) {
        if (mask & 1) {
            if (pick == 0) {
                return idx;
            }
            pick--;
        }
        mask >>= 1;
        idx++;
    }

    return -1;  /* Should not reach here */
}

uint32_t pb_rng_checksum(const pb_rng* rng)
{
    /* Simple FNV-1a hash of state */
    uint32_t hash = 2166136261u;
    const uint8_t* bytes = (const uint8_t*)rng->state;

    for (size_t i = 0; i < sizeof(rng->state); i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

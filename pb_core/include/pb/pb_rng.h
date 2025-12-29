/*
 * pb_rng.h - Deterministic PRNG for pb_core
 *
 * Uses xoshiro128** - a fast, high-quality generator suitable for games.
 * State can be serialized for replay/multiplayer sync.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_RNG_H
#define PB_RNG_H

#include "pb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * RNG Initialization
 *============================================================================*/

/**
 * Initialize RNG with a 64-bit seed.
 * Uses SplitMix64 to expand seed into full state.
 */
void pb_rng_seed(pb_rng* rng, uint64_t seed);

/**
 * Initialize RNG from raw state (for replay/networking).
 */
void pb_rng_set_state(pb_rng* rng, const uint32_t state[4]);

/**
 * Get current state for serialization.
 */
void pb_rng_get_state(const pb_rng* rng, uint32_t state[4]);

/*============================================================================
 * Random Number Generation
 *============================================================================*/

/**
 * Generate a random 32-bit unsigned integer.
 */
uint32_t pb_rng_next(pb_rng* rng);

/**
 * Generate a random integer in [0, max).
 * Uses rejection sampling to avoid bias.
 */
uint32_t pb_rng_range(pb_rng* rng, uint32_t max);

/**
 * Generate a random integer in [min, max] (inclusive).
 */
int pb_rng_range_int(pb_rng* rng, int min, int max);

/**
 * Generate a random float in [0.0, 1.0).
 */
float pb_rng_float(pb_rng* rng);

/**
 * Generate a random float in [min, max).
 */
float pb_rng_float_range(pb_rng* rng, float min, float max);

/**
 * Shuffle an array using Fisher-Yates algorithm.
 *
 * @param array    Pointer to array elements
 * @param count    Number of elements
 * @param elem_size Size of each element in bytes
 */
void pb_rng_shuffle(pb_rng* rng, void* array, size_t count, size_t elem_size);

/*============================================================================
 * Game-Specific Helpers
 *============================================================================*/

/**
 * Pick a random color from the allowed colors bitmask.
 *
 * @param rng           RNG state
 * @param allowed_mask  Bitmask of allowed color indices (bit 0 = color 0, etc.)
 * @return              Selected color index, or -1 if no colors allowed
 */
int pb_rng_pick_color(pb_rng* rng, uint8_t allowed_mask);

/**
 * Generate deterministic checksum of RNG state.
 * Used for sync verification in multiplayer.
 */
uint32_t pb_rng_checksum(const pb_rng* rng);

/*============================================================================
 * Hardware Entropy Mixing (HP48-inspired)
 *
 * HP48 uses timer + CRC register for entropy.
 * We provide platform-agnostic entropy mixing that can incorporate:
 * - High-resolution timers (RDTSC, cycle counters)
 * - Hardware RNG (RDSEED, /dev/urandom)
 * - User input timing jitter
 *============================================================================*/

/**
 * Mix additional entropy into RNG state.
 * Does NOT reset the RNG - combines new entropy with existing state.
 *
 * @param rng     RNG state to mix into
 * @param entropy Raw entropy value to mix
 */
void pb_rng_mix_entropy(pb_rng* rng, uint64_t entropy);

/**
 * Collect platform-specific hardware entropy.
 * Falls back to time-based entropy if no hardware RNG available.
 *
 * @return  Raw entropy value suitable for pb_rng_mix_entropy()
 */
uint64_t pb_rng_collect_entropy(void);

/**
 * Reseed RNG by mixing in fresh hardware entropy.
 * Useful after long idle periods or for security-sensitive contexts.
 *
 * @param rng RNG state to reseed
 */
void pb_rng_reseed(pb_rng* rng);

/**
 * Create a non-deterministic RNG by seeding from hardware entropy.
 * Use this for single-player games where reproducibility isn't needed.
 *
 * @param rng RNG state to initialize
 */
void pb_rng_seed_random(pb_rng* rng);

#ifdef __cplusplus
}
#endif

#endif /* PB_RNG_H */

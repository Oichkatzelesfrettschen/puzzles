/**
 * @file pb_checksum.h
 * @brief CRC-32 checksumming for determinism verification
 *
 * Provides checksums for board state, game state, and incremental
 * updates during gameplay. Used for:
 * - Replay verification (detect desync)
 * - Multiplayer sync checking
 * - Save state validation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_CHECKSUM_H
#define PB_CHECKSUM_H

#include "pb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CRC-32 Primitives
 *
 * Uses CRC-32/ISO-HDLC (polynomial 0xEDB88320, same as zlib).
 * Table-driven for performance.
 *============================================================================*/

/**
 * Initialize CRC-32 lookup table.
 * Called automatically on first use, but can be called explicitly
 * to ensure table is ready before hot path.
 */
void pb_crc32_init(void);

/**
 * Compute CRC-32 of data buffer.
 * @param data Input data
 * @param len Length in bytes
 * @return CRC-32 value
 */
uint32_t pb_crc32(const void* data, size_t len);

/**
 * Update running CRC-32 with additional data.
 * @param crc Previous CRC value (use 0 for first call)
 * @param data Additional data
 * @param len Length in bytes
 * @return Updated CRC-32 value
 */
uint32_t pb_crc32_update(uint32_t crc, const void* data, size_t len);

/**
 * Finalize CRC-32 (XOR with 0xFFFFFFFF).
 * Call this after the last pb_crc32_update().
 */
uint32_t pb_crc32_finalize(uint32_t crc);

/*============================================================================
 * Game State Checksums
 *
 * High-level functions for checksumming game structures.
 * These serialize data in a consistent order for cross-platform determinism.
 *============================================================================*/

/* Note: pb_board_checksum() is declared in pb_board.h */

/**
 * Compute checksum of bubble structure.
 */
uint32_t pb_bubble_checksum(const pb_bubble* bubble);

/**
 * Compute checksum of RNG state.
 * Useful for verifying RNG hasn't diverged.
 */
uint32_t pb_rng_state_checksum(const pb_rng* rng);

/**
 * Compute full game state checksum.
 * Includes: board, RNG, score, phase, cannon angle, current/preview bubbles.
 * This is the primary checksum for desync detection.
 */
uint32_t pb_state_checksum(const pb_game_state* state);

/**
 * Compute minimal checksum for per-frame tracking.
 * Lighter-weight than full state checksum.
 * Covers: board checksum XOR with RNG checksum, score, frame number.
 */
uint32_t pb_frame_checksum(const pb_game_state* state);

/*============================================================================
 * Desync Detection
 *============================================================================*/

/** Desync detection result */
typedef struct pb_desync_info {
    bool detected;              /* True if desync found */
    uint32_t frame;             /* Frame where desync occurred */
    uint32_t expected;          /* Expected checksum */
    uint32_t actual;            /* Actual checksum */
    const char* component;      /* Which component diverged (board/rng/etc) */
} pb_desync_info;

/**
 * Compare two game states and identify divergence.
 * @param expected Expected state
 * @param actual Actual state
 * @param info Output: desync information
 * @return true if states match
 */
bool pb_state_compare(const pb_game_state* expected,
                      const pb_game_state* actual,
                      pb_desync_info* info);

/**
 * Rolling checksum buffer for efficient desync detection.
 */
#define PB_CHECKSUM_BUFFER_SIZE 64

typedef struct pb_checksum_buffer {
    uint32_t checksums[PB_CHECKSUM_BUFFER_SIZE];
    uint32_t frames[PB_CHECKSUM_BUFFER_SIZE];
    int head;                   /* Next write position */
    int count;                  /* Number of valid entries */
} pb_checksum_buffer;

/**
 * Initialize checksum buffer.
 */
void pb_checksum_buffer_init(pb_checksum_buffer* buf);

/**
 * Record frame checksum.
 */
void pb_checksum_buffer_record(pb_checksum_buffer* buf,
                               uint32_t frame, uint32_t checksum);

/**
 * Find checksum for specific frame.
 * @param buf Checksum buffer
 * @param frame Frame to look up
 * @param checksum Output: checksum if found
 * @return true if frame found in buffer
 */
bool pb_checksum_buffer_find(const pb_checksum_buffer* buf,
                             uint32_t frame, uint32_t* checksum);

/**
 * Verify checksum matches for given frame.
 */
bool pb_checksum_buffer_verify(const pb_checksum_buffer* buf,
                               uint32_t frame, uint32_t expected);

#ifdef __cplusplus
}
#endif

#endif /* PB_CHECKSUM_H */

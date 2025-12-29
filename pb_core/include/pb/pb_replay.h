/**
 * @file pb_replay.h
 * @brief Deterministic replay recording and playback system
 *
 * Binary event format with varint delta encoding for compact replays.
 * Supports recording, playback, verification, and checkpoint seeking.
 *
 * Design principles:
 * - Only INPUT events stored (fire, rotate, switch, pause)
 * - Derived events regenerated via deterministic simulation
 * - Varint frame deltas for ~50% compression
 * - Per-frame checksums for desync detection
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_REPLAY_H
#define PB_REPLAY_H

#include "pb_types.h"
#include "pb_rng.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Binary Format Constants
 *============================================================================*/

/** Magic bytes: "PBRP" (Puzzle Bobble RePlay) */
#define PB_REPLAY_MAGIC 0x50525042

/** Current binary format version */
#define PB_REPLAY_VERSION 1

/** Maximum events in a single replay */
#define PB_REPLAY_MAX_EVENTS 65536

/** Maximum checkpoints per replay */
#define PB_REPLAY_MAX_CHECKPOINTS 256

/** Frames between automatic checkpoints */
#define PB_REPLAY_CHECKPOINT_INTERVAL 300  /* ~5 seconds at 60fps */

/*============================================================================
 * Input Event Type (subset for replay storage)
 *
 * Only events that require recording - derived events are regenerated
 * from deterministic simulation during playback.
 *============================================================================*/

typedef enum pb_input_event_type {
    PB_INPUT_NONE = 0,          /* No-op / padding */
    PB_INPUT_ROTATE_LEFT,       /* CCW rotation start/hold */
    PB_INPUT_ROTATE_RIGHT,      /* CW rotation start/hold */
    PB_INPUT_FIRE,              /* Launch bubble at angle */
    PB_INPUT_SWITCH,            /* Swap current/preview bubble */
    PB_INPUT_PAUSE,             /* Game paused */
    PB_INPUT_UNPAUSE,           /* Game resumed */
    PB_INPUT_COUNT              /* Must be <= 16 for 4-bit encoding */
} pb_input_event_type;

/*============================================================================
 * Compact Binary Event Format
 *
 * Header byte layout:
 *   Bits 7-4: Event type (pb_input_event_type, 0-15)
 *   Bits 3-2: Reserved / flags
 *   Bits 1-0: Payload size indicator
 *             00 = no payload
 *             01 = 2-byte payload (angle as int16)
 *             10 = 4-byte payload (angle as int32/float)
 *             11 = extended (future use)
 *
 * Frame delta: LEB128 varint (1-5 bytes)
 *   Bit 7: continuation flag (1 = more bytes)
 *   Bits 6-0: 7 bits of data, LSB first
 *
 * Payload: Event-specific data (0-4 bytes)
 *   FIRE: angle in fixed-point or float format
 *   ROTATE: optional delta angle for analog input
 *   Others: no payload
 *============================================================================*/

/** Packed binary input event (for serialization) */
typedef struct pb_packed_event {
    uint8_t header;             /* Type + flags + payload indicator */
    uint32_t frame_delta;       /* Decoded varint delta from previous event */
    union {
        int16_t angle_i16;      /* 8.8 fixed-point angle (-pi to pi) */
        int32_t angle_i32;      /* 16.16 fixed-point angle */
        float angle_f32;        /* IEEE 754 float angle */
    } payload;
} pb_packed_event;

/** Runtime input event (for recording/playback) */
typedef struct pb_input_event {
    pb_input_event_type type;
    uint32_t frame;             /* Absolute frame number */
    pb_scalar angle;            /* For FIRE event (radians) */
} pb_input_event;

/*============================================================================
 * Checkpoint Structure
 *
 * Full state snapshot for fast seeking and verification.
 *============================================================================*/

typedef struct pb_checkpoint {
    uint32_t frame;             /* Frame number of this checkpoint */
    uint32_t event_index;       /* Index into event array */
    uint32_t state_checksum;    /* Full game state CRC-32 */
    uint32_t board_checksum;    /* Board-only checksum */
    uint32_t rng_state[4];      /* xoshiro128** state snapshot */
    uint32_t score;             /* Score at checkpoint */
    int32_t shots_fired;        /* Shot count at checkpoint */
} pb_checkpoint;

/*============================================================================
 * Replay Header
 *
 * File format:
 *   [4] Magic (PB_REPLAY_MAGIC)
 *   [1] Version
 *   [1] Flags
 *   [2] Reserved
 *   [8] Seed
 *   [64] Level ID
 *   [64] Ruleset ID
 *   [4] Event count
 *   [4] Checkpoint count
 *   [4] Duration (frames)
 *   [4] Final score
 *   [1] Outcome (0=incomplete, 1=won, 2=lost, 3=abandoned)
 *   [3] Reserved
 *   [checkpoints...]
 *   [events...]
 *============================================================================*/

/** Replay header flags */
typedef enum pb_replay_flags {
    PB_REPLAY_FLAG_NONE = 0,
    PB_REPLAY_FLAG_FIXED_POINT = (1 << 0),  /* Uses fixed-point angles */
    PB_REPLAY_FLAG_COMPRESSED = (1 << 1),   /* Events are zlib compressed */
    PB_REPLAY_FLAG_VERIFIED = (1 << 2)      /* Checkpoints have been verified */
} pb_replay_flags;

/** Game outcome */
typedef enum pb_outcome {
    PB_OUTCOME_INCOMPLETE = 0,
    PB_OUTCOME_WON,
    PB_OUTCOME_LOST,
    PB_OUTCOME_ABANDONED
} pb_outcome;

/** Binary replay header (on-disk format) */
typedef struct pb_replay_header {
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t reserved;
    uint64_t seed;
    char level_id[64];
    char ruleset_id[64];
    uint32_t event_count;
    uint32_t checkpoint_count;
    uint32_t duration_frames;
    uint32_t final_score;
    uint8_t outcome;
    uint8_t reserved2[3];
} pb_replay_header;

/*============================================================================
 * Replay Structure (runtime)
 *============================================================================*/

typedef struct pb_replay {
    pb_replay_header header;

    /* Event storage */
    pb_input_event* events;
    uint32_t event_count;
    uint32_t event_capacity;

    /* Checkpoint storage */
    pb_checkpoint* checkpoints;
    uint32_t checkpoint_count;
    uint32_t checkpoint_capacity;

    /* Recording state */
    uint32_t last_event_frame;  /* For delta calculation */
} pb_replay;

/*============================================================================
 * Varint Encoding/Decoding
 *============================================================================*/

/**
 * Encode uint32 as LEB128 varint.
 * @param value Value to encode
 * @param out Output buffer (must have 5 bytes)
 * @return Number of bytes written (1-5)
 */
int pb_varint_encode(uint32_t value, uint8_t* out);

/**
 * Decode LEB128 varint to uint32.
 * @param data Input buffer
 * @param len Available bytes
 * @param out_value Decoded value
 * @return Bytes consumed, or 0 on error
 */
int pb_varint_decode(const uint8_t* data, int len, uint32_t* out_value);

/**
 * Get encoded size of value without writing.
 */
int pb_varint_size(uint32_t value);

/*============================================================================
 * Event Packing/Unpacking
 *============================================================================*/

/**
 * Pack input event to compact binary format.
 * @param event Event to pack
 * @param prev_frame Previous event's frame (for delta)
 * @param out Output buffer (must have 9 bytes max)
 * @param use_fixed_point Use 16.16 fixed-point for angles
 * @return Bytes written
 */
int pb_event_pack(const pb_input_event* event, uint32_t prev_frame,
                  uint8_t* out, bool use_fixed_point);

/**
 * Unpack binary event to input event.
 * @param data Input buffer
 * @param len Available bytes
 * @param prev_frame Previous event's absolute frame
 * @param event Output event
 * @param use_fixed_point Angles are 16.16 fixed-point
 * @return Bytes consumed, or 0 on error
 */
int pb_event_unpack(const uint8_t* data, int len, uint32_t prev_frame,
                    pb_input_event* event, bool use_fixed_point);

/**
 * Map pb_event_type to pb_input_event_type (or PB_INPUT_NONE if not input).
 */
pb_input_event_type pb_event_to_input_type(pb_event_type type);

/**
 * Map pb_input_event_type back to pb_event_type.
 */
pb_event_type pb_input_to_event_type(pb_input_event_type type);

/*============================================================================
 * Replay Lifecycle
 *============================================================================*/

/**
 * Initialize empty replay for recording.
 * @param replay Replay to initialize
 * @param seed RNG seed
 * @param level_id Level identifier (may be NULL)
 * @param ruleset_id Ruleset identifier (may be NULL)
 */
void pb_replay_init(pb_replay* replay, uint64_t seed,
                    const char* level_id, const char* ruleset_id);

/**
 * Free replay resources.
 */
void pb_replay_free(pb_replay* replay);

/**
 * Clear replay (reset to empty, keep allocated memory).
 */
void pb_replay_clear(pb_replay* replay);

/*============================================================================
 * Recording
 *============================================================================*/

/**
 * Record an input event.
 * @param replay Replay to record to
 * @param event Event to record
 * @return PB_OK on success
 */
pb_result pb_replay_record_event(pb_replay* replay, const pb_input_event* event);

/**
 * Record from pb_event (converts to input event, ignores non-input events).
 * @return PB_OK, or PB_ERR_INVALID_ARG if not an input event
 */
pb_result pb_replay_record(pb_replay* replay, const pb_event* event);

/**
 * Add checkpoint (call periodically during recording).
 * @param replay Replay
 * @param frame Current frame
 * @param state_checksum Full state checksum
 * @param board_checksum Board-only checksum
 * @param rng Current RNG state
 * @param score Current score
 * @param shots_fired Shot count
 */
pb_result pb_replay_add_checkpoint(pb_replay* replay, uint32_t frame,
                                   uint32_t state_checksum, uint32_t board_checksum,
                                   const pb_rng* rng, uint32_t score, int shots_fired);

/**
 * Finalize recording (set duration, outcome).
 */
void pb_replay_finalize(pb_replay* replay, uint32_t duration_frames,
                        uint32_t final_score, pb_outcome outcome);

/*============================================================================
 * Serialization
 *============================================================================*/

/**
 * Calculate serialized size of replay.
 */
size_t pb_replay_serialized_size(const pb_replay* replay);

/**
 * Serialize replay to buffer.
 * @param replay Replay to serialize
 * @param buffer Output buffer (must be at least pb_replay_serialized_size bytes)
 * @param buffer_size Available buffer size
 * @return Bytes written, or 0 on error
 */
size_t pb_replay_serialize(const pb_replay* replay, uint8_t* buffer, size_t buffer_size);

/**
 * Deserialize replay from buffer.
 * @param buffer Input buffer
 * @param buffer_size Available bytes
 * @param replay Output replay (caller must free with pb_replay_free)
 * @return PB_OK on success
 */
pb_result pb_replay_deserialize(const uint8_t* buffer, size_t buffer_size,
                                pb_replay* replay);

/**
 * Save replay to file.
 */
pb_result pb_replay_save(const pb_replay* replay, const char* path);

/**
 * Load replay from file.
 */
pb_result pb_replay_load(const char* path, pb_replay* replay);

/*============================================================================
 * Playback
 *============================================================================*/

/** Playback state */
typedef struct pb_playback {
    const pb_replay* replay;    /* Source replay (not owned) */
    uint32_t event_cursor;      /* Next event index to apply */
    uint32_t checkpoint_cursor; /* Last passed checkpoint */
    uint32_t current_frame;     /* Current playback frame */
    bool finished;              /* All events applied */
    bool paused;                /* Playback paused */
    int speed_multiplier;       /* 1x=100, 2x=200, 0.5x=50, pause=0 */
} pb_playback;

/**
 * Initialize playback from replay.
 */
void pb_playback_init(pb_playback* playback, const pb_replay* replay);

/**
 * Get next event if it should fire on current frame.
 * @param playback Playback state
 * @param event Output event (if returns true)
 * @return true if event is ready
 */
bool pb_playback_get_event(pb_playback* playback, pb_input_event* event);

/**
 * Advance playback by one frame.
 */
void pb_playback_advance(pb_playback* playback);

/**
 * Seek to frame via checkpoint.
 * @param playback Playback state
 * @param target_frame Frame to seek to
 * @param checkpoint Output checkpoint to restore from (may be NULL)
 * @return Closest checkpoint frame <= target_frame
 */
uint32_t pb_playback_seek(pb_playback* playback, uint32_t target_frame,
                          pb_checkpoint* checkpoint);

/**
 * Set playback speed.
 * @param playback Playback state
 * @param speed_percent 100 = normal, 200 = 2x, 50 = 0.5x, 0 = pause
 */
void pb_playback_set_speed(pb_playback* playback, int speed_percent);

/*============================================================================
 * Verification
 *============================================================================*/

/**
 * Verify checkpoint matches current game state.
 * @param checkpoint Checkpoint to verify
 * @param state_checksum Current state checksum
 * @param board_checksum Current board checksum
 * @return true if checksums match
 */
bool pb_checkpoint_verify(const pb_checkpoint* checkpoint,
                          uint32_t state_checksum, uint32_t board_checksum);

#ifdef __cplusplus
}
#endif

#endif /* PB_REPLAY_H */

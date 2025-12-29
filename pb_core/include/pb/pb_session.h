/**
 * @file pb_session.h
 * @brief Game session with integrated replay recording/playback
 *
 * Wraps pb_game_state with automatic event capture, checksum
 * verification, and replay playback injection. Use this for:
 * - Recording gameplay for replays
 * - Playing back recorded replays
 * - Verifying determinism between sessions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_SESSION_H
#define PB_SESSION_H

#include "pb_types.h"
#include "pb_game.h"
#include "pb_replay.h"
#include "pb_checksum.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Session Mode
 *============================================================================*/

typedef enum pb_session_mode {
    PB_SESSION_LIVE = 0,        /* Normal gameplay, optional recording */
    PB_SESSION_RECORDING,       /* Recording all inputs to replay */
    PB_SESSION_PLAYBACK,        /* Playing back from replay */
    PB_SESSION_VERIFICATION,    /* Playback with checksum verification */
} pb_session_mode;

/*============================================================================
 * Session Callbacks
 *============================================================================*/

/** Called when desync is detected during verification */
typedef void (*pb_desync_callback)(const pb_desync_info* info, void* userdata);

/** Called when checkpoint is created */
typedef void (*pb_checkpoint_callback)(const pb_checkpoint* cp, void* userdata);

/*============================================================================
 * Session Configuration
 *============================================================================*/

typedef struct pb_session_config {
    pb_session_mode mode;

    /* Recording options */
    bool auto_checkpoint;           /* Create checkpoints automatically */
    int checkpoint_interval;        /* Frames between checkpoints (default: 300) */

    /* Playback options */
    int playback_speed;             /* Speed percent (100 = normal) */
    bool verify_checksums;          /* Verify against recorded checksums */

    /* Callbacks */
    pb_desync_callback on_desync;
    pb_checkpoint_callback on_checkpoint;
    void* callback_userdata;
} pb_session_config;

/*============================================================================
 * Session Structure
 *============================================================================*/

typedef struct pb_session {
    pb_game_state game;             /* Embedded game state */
    pb_session_mode mode;
    pb_session_config config;

    /* Recording state */
    pb_replay replay;               /* Current replay (recording or playback) */
    bool owns_replay;               /* True if we allocated the replay */

    /* Playback state */
    pb_playback playback;           /* Playback cursor */

    /* Checksum verification */
    pb_checksum_buffer checksum_buf;
    uint32_t frames_since_checkpoint;

    /* Session metadata */
    uint64_t seed;
    char level_id[64];
    char ruleset_id[64];

    /* Status */
    bool active;                    /* Session is running */
    bool finished;                  /* Session completed (game over or replay end) */
    pb_desync_info last_desync;     /* Last desync info if verification failed */
} pb_session;

/*============================================================================
 * Session Lifecycle
 *============================================================================*/

/**
 * Initialize session with default config.
 */
void pb_session_config_default(pb_session_config* config);

/**
 * Create a new live/recording session.
 *
 * @param session   Session to initialize
 * @param ruleset   Game rules (NULL for defaults)
 * @param seed      RNG seed
 * @param config    Session configuration
 * @return          PB_OK on success
 */
pb_result pb_session_create(pb_session* session, const pb_ruleset* ruleset,
                            uint64_t seed, const pb_session_config* config);

/**
 * Create a playback session from replay.
 *
 * @param session   Session to initialize
 * @param replay    Replay to play (session takes ownership if owns_replay)
 * @param ruleset   Ruleset override (NULL to use replay's ruleset)
 * @param config    Session configuration
 * @return          PB_OK on success
 */
pb_result pb_session_create_playback(pb_session* session, pb_replay* replay,
                                     const pb_ruleset* ruleset,
                                     const pb_session_config* config);

/**
 * Free session resources.
 */
void pb_session_destroy(pb_session* session);

/*============================================================================
 * Input (Recording Mode)
 *
 * These functions wrap pb_game_* and automatically record events.
 *============================================================================*/

/**
 * Set cannon angle (recorded if in recording mode).
 */
void pb_session_set_angle(pb_session* session, pb_scalar angle);

/**
 * Rotate cannon (recorded).
 */
void pb_session_rotate(pb_session* session, pb_scalar delta);

/**
 * Fire bubble (recorded).
 */
pb_result pb_session_fire(pb_session* session);

/**
 * Swap bubbles (recorded).
 */
pb_result pb_session_swap(pb_session* session);

/**
 * Pause/unpause (recorded).
 */
void pb_session_pause(pb_session* session, bool paused);

/*============================================================================
 * Game Loop Integration
 *============================================================================*/

/**
 * Advance session by one frame.
 *
 * In recording mode: calls pb_game_tick, records checksum
 * In playback mode: injects events, calls pb_game_tick, verifies checksum
 *
 * @param session Session to advance
 * @return        Number of game events this frame, or -1 on error
 */
int pb_session_tick(pb_session* session);

/**
 * Run session until game over or replay end.
 * Useful for batch verification.
 *
 * @param session     Session to run
 * @param max_frames  Maximum frames to run (0 = unlimited)
 * @return            Total frames run
 */
int pb_session_run(pb_session* session, int max_frames);

/*============================================================================
 * Playback Control
 *============================================================================*/

/**
 * Seek to specific frame (playback mode only).
 * Uses checkpoints for efficient seeking.
 *
 * @param session      Session
 * @param target_frame Frame to seek to
 * @return             Actual frame after seek
 */
uint32_t pb_session_seek(pb_session* session, uint32_t target_frame);

/**
 * Set playback speed.
 *
 * @param session       Session
 * @param speed_percent Speed (0=pause, 50=half, 100=normal, 200=double)
 */
void pb_session_set_speed(pb_session* session, int speed_percent);

/**
 * Get current playback progress.
 *
 * @param session       Session
 * @param current_frame Output: current frame
 * @param total_frames  Output: total frames in replay
 */
void pb_session_get_progress(const pb_session* session,
                             uint32_t* current_frame, uint32_t* total_frames);

/*============================================================================
 * Recording Finalization
 *============================================================================*/

/**
 * Finalize recording session.
 * Sets final score, duration, and outcome.
 *
 * @param session Session to finalize
 * @param outcome Game outcome (won/lost/abandoned)
 */
void pb_session_finalize(pb_session* session, pb_outcome outcome);

/**
 * Get the recorded replay.
 * Only valid after finalization in recording mode.
 */
const pb_replay* pb_session_get_replay(const pb_session* session);

/**
 * Extract replay from session (transfers ownership to caller).
 */
pb_result pb_session_extract_replay(pb_session* session, pb_replay* out_replay);

/*============================================================================
 * Verification
 *============================================================================*/

/**
 * Check if session has encountered a desync.
 */
bool pb_session_has_desync(const pb_session* session);

/**
 * Get last desync info.
 */
const pb_desync_info* pb_session_get_desync(const pb_session* session);

/**
 * Verify current session state against recorded checkpoint.
 */
bool pb_session_verify_checkpoint(pb_session* session, int checkpoint_idx);

/*============================================================================
 * Twin Simulation (Determinism Testing)
 *============================================================================*/

/**
 * Run twin simulation: execute same replay in two sessions, verify match.
 *
 * @param replay        Replay to execute
 * @param ruleset       Ruleset to use (NULL for defaults)
 * @param info          Output: desync info if mismatch found
 * @return              true if both runs produce identical checksums
 */
bool pb_twin_simulate(const pb_replay* replay, const pb_ruleset* ruleset,
                      pb_desync_info* info);

/**
 * Create golden checksum fixture from replay.
 * Runs the replay and captures frame checksums at intervals.
 *
 * @param replay        Replay to run
 * @param ruleset       Ruleset to use
 * @param interval      Frame interval between checksums
 * @param checksums     Output array (caller allocates)
 * @param max_checksums Maximum checksums to capture
 * @return              Number of checksums captured
 */
int pb_create_golden_checksums(const pb_replay* replay,
                               const pb_ruleset* ruleset,
                               int interval,
                               uint32_t* checksums,
                               int max_checksums);

#ifdef __cplusplus
}
#endif

#endif /* PB_SESSION_H */

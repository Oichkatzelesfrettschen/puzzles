/**
 * @file pb_session.c
 * @brief Game session with integrated replay recording/playback
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_session.h"
#include "pb/pb_board.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static void record_input_event(pb_session* session, pb_input_event_type type,
                               pb_scalar angle)
{
    if (session->mode != PB_SESSION_RECORDING) return;

    pb_input_event event = {
        .type = type,
        .frame = session->game.frame,
        .angle = angle
    };

    pb_replay_record_event(&session->replay, &event);
}

static void maybe_create_checkpoint(pb_session* session)
{
    if (!session->config.auto_checkpoint) return;
    if (session->mode != PB_SESSION_RECORDING) return;

    session->frames_since_checkpoint++;

    if (session->frames_since_checkpoint >= (uint32_t)session->config.checkpoint_interval) {
        uint32_t state_crc = pb_state_checksum(&session->game);
        uint32_t board_crc = pb_board_checksum(&session->game.board);

        pb_replay_add_checkpoint(&session->replay, session->game.frame,
                                 state_crc, board_crc,
                                 &session->game.rng,
                                 session->game.score,
                                 session->game.shots_fired);

        session->frames_since_checkpoint = 0;

        if (session->config.on_checkpoint) {
            const pb_checkpoint* cp = &session->replay.checkpoints[
                session->replay.checkpoint_count - 1];
            session->config.on_checkpoint(cp, session->config.callback_userdata);
        }
    }
}

static void record_frame_checksum(pb_session* session)
{
    uint32_t checksum = pb_frame_checksum(&session->game);
    pb_checksum_buffer_record(&session->checksum_buf,
                              session->game.frame, checksum);
}

static bool inject_playback_events(pb_session* session)
{
    if (session->mode != PB_SESSION_PLAYBACK &&
        session->mode != PB_SESSION_VERIFICATION) {
        return true;
    }

    pb_input_event event;
    while (pb_playback_get_event(&session->playback, &event)) {
        /* Convert to game action */
        switch (event.type) {
            case PB_INPUT_FIRE:
                pb_game_set_angle(&session->game, event.angle);
                pb_game_fire(&session->game);
                break;
            case PB_INPUT_ROTATE_LEFT:
                pb_game_rotate(&session->game, PB_FLOAT_TO_FIXED(-0.05f));
                break;
            case PB_INPUT_ROTATE_RIGHT:
                pb_game_rotate(&session->game, PB_FLOAT_TO_FIXED(0.05f));
                break;
            case PB_INPUT_SWITCH:
                pb_game_swap_bubbles(&session->game);
                break;
            case PB_INPUT_PAUSE:
                pb_game_pause(&session->game, true);
                break;
            case PB_INPUT_UNPAUSE:
                pb_game_pause(&session->game, false);
                break;
            default:
                break;
        }
    }

    /* Playback continues until we reach recorded duration */
    uint32_t duration = session->replay.header.duration_frames;
    return session->game.frame < duration;
}

static bool verify_playback_checksum(pb_session* session)
{
    if (session->mode != PB_SESSION_VERIFICATION) return true;
    if (!session->config.verify_checksums) return true;

    /* Find matching checkpoint */
    for (uint32_t i = 0; i < session->replay.checkpoint_count; i++) {
        const pb_checkpoint* cp = &session->replay.checkpoints[i];
        if (cp->frame == session->game.frame) {
            uint32_t state_crc = pb_state_checksum(&session->game);
            uint32_t board_crc = pb_board_checksum(&session->game.board);

            if (!pb_checkpoint_verify(cp, state_crc, board_crc)) {
                session->last_desync.detected = true;
                session->last_desync.frame = session->game.frame;
                session->last_desync.expected = cp->state_checksum;
                session->last_desync.actual = state_crc;
                session->last_desync.component = "state";

                if (session->config.on_desync) {
                    session->config.on_desync(&session->last_desync,
                                              session->config.callback_userdata);
                }
                return false;
            }
        }
    }

    return true;
}

/*============================================================================
 * Session Configuration
 *============================================================================*/

void pb_session_config_default(pb_session_config* config)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));

    config->mode = PB_SESSION_LIVE;
    config->auto_checkpoint = true;
    config->checkpoint_interval = PB_REPLAY_CHECKPOINT_INTERVAL;
    config->playback_speed = 100;
    config->verify_checksums = true;
}

/*============================================================================
 * Session Lifecycle
 *============================================================================*/

pb_result pb_session_create(pb_session* session, const pb_ruleset* ruleset,
                            uint64_t seed, const pb_session_config* config)
{
    if (!session) return PB_ERR_INVALID_ARG;

    memset(session, 0, sizeof(*session));

    /* Apply config */
    if (config) {
        session->config = *config;
    } else {
        pb_session_config_default(&session->config);
    }
    session->mode = session->config.mode;
    session->seed = seed;

    /* Initialize game */
    pb_result result = pb_game_init(&session->game, ruleset, seed);
    if (result != PB_OK) return result;

    /* Initialize recording if needed */
    if (session->mode == PB_SESSION_RECORDING) {
        pb_replay_init(&session->replay, seed,
                       session->level_id, session->ruleset_id);
        session->owns_replay = true;
    }

    /* Initialize checksum buffer */
    pb_checksum_buffer_init(&session->checksum_buf);

    session->active = true;
    session->finished = false;

    return PB_OK;
}

pb_result pb_session_create_playback(pb_session* session, pb_replay* replay,
                                     const pb_ruleset* ruleset,
                                     const pb_session_config* config)
{
    if (!session || !replay) return PB_ERR_INVALID_ARG;

    memset(session, 0, sizeof(*session));

    /* Apply config */
    if (config) {
        session->config = *config;
    } else {
        pb_session_config_default(&session->config);
        session->config.mode = PB_SESSION_PLAYBACK;
    }
    session->mode = session->config.mode;
    session->seed = replay->header.seed;

    /* Copy replay reference */
    session->replay = *replay;
    session->owns_replay = false;

    /* Initialize game with replay's seed */
    pb_result result = pb_game_init(&session->game, ruleset, replay->header.seed);
    if (result != PB_OK) return result;

    /* Initialize playback */
    pb_playback_init(&session->playback, &session->replay);
    pb_playback_set_speed(&session->playback, session->config.playback_speed);

    /* Initialize checksum buffer */
    pb_checksum_buffer_init(&session->checksum_buf);

    session->active = true;
    session->finished = false;

    return PB_OK;
}

void pb_session_destroy(pb_session* session)
{
    if (!session) return;

    if (session->owns_replay) {
        pb_replay_free(&session->replay);
    }

    session->active = false;
    session->finished = true;
}

/*============================================================================
 * Input (Recording Mode)
 *============================================================================*/

void pb_session_set_angle(pb_session* session, pb_scalar angle)
{
    if (!session || !session->active) return;
    pb_game_set_angle(&session->game, angle);
    /* Angle changes are not recorded as discrete events */
}

void pb_session_rotate(pb_session* session, pb_scalar delta)
{
    if (!session || !session->active) return;
    pb_game_rotate(&session->game, delta);

    if (delta < 0) {
        record_input_event(session, PB_INPUT_ROTATE_LEFT, 0);
    } else if (delta > 0) {
        record_input_event(session, PB_INPUT_ROTATE_RIGHT, 0);
    }
}

pb_result pb_session_fire(pb_session* session)
{
    if (!session || !session->active) return PB_ERR_INVALID_STATE;

    pb_scalar angle = session->game.cannon_angle;
    pb_result result = pb_game_fire(&session->game);

    if (result == PB_OK) {
        record_input_event(session, PB_INPUT_FIRE, angle);
    }

    return result;
}

pb_result pb_session_swap(pb_session* session)
{
    if (!session || !session->active) return PB_ERR_INVALID_STATE;

    pb_result result = pb_game_swap_bubbles(&session->game);

    if (result == PB_OK) {
        record_input_event(session, PB_INPUT_SWITCH, 0);
    }

    return result;
}

void pb_session_pause(pb_session* session, bool paused)
{
    if (!session || !session->active) return;

    pb_game_pause(&session->game, paused);
    record_input_event(session, paused ? PB_INPUT_PAUSE : PB_INPUT_UNPAUSE, 0);
}

/*============================================================================
 * Game Loop Integration
 *============================================================================*/

int pb_session_tick(pb_session* session)
{
    if (!session || !session->active || session->finished) return -1;

    /* In playback mode, inject events first */
    if (session->mode == PB_SESSION_PLAYBACK ||
        session->mode == PB_SESSION_VERIFICATION) {

        if (!inject_playback_events(session)) {
            session->finished = true;
        }

        pb_playback_advance(&session->playback);
    }

    /* Run game simulation */
    int events = pb_game_tick(&session->game);

    /* Record frame checksum */
    record_frame_checksum(session);

    /* Maybe create checkpoint */
    maybe_create_checkpoint(session);

    /* Verify checksum if in verification mode */
    if (!verify_playback_checksum(session)) {
        session->finished = true;
        return -1;
    }

    /* Check for game over */
    if (pb_game_is_over(&session->game)) {
        session->finished = true;
    }

    return events;
}

int pb_session_run(pb_session* session, int max_frames)
{
    if (!session || !session->active) return 0;

    int frames = 0;
    while (!session->finished) {
        if (max_frames > 0 && frames >= max_frames) break;

        int result = pb_session_tick(session);
        if (result < 0) break;

        frames++;
    }

    return frames;
}

/*============================================================================
 * Playback Control
 *============================================================================*/

uint32_t pb_session_seek(pb_session* session, uint32_t target_frame)
{
    if (!session) return 0;
    if (session->mode != PB_SESSION_PLAYBACK &&
        session->mode != PB_SESSION_VERIFICATION) {
        return session->game.frame;
    }

    /* Find checkpoint to restore from */
    pb_checkpoint checkpoint;
    uint32_t checkpoint_frame = pb_playback_seek(&session->playback,
                                                  target_frame, &checkpoint);

    if (checkpoint_frame > 0) {
        /* Restore game state from checkpoint */
        pb_game_reset(&session->game, session->seed);

        /* Restore RNG state */
        memcpy(session->game.rng.state, checkpoint.rng_state,
               sizeof(checkpoint.rng_state));

        /* Run from checkpoint to target */
        while (session->game.frame < target_frame && !session->finished) {
            pb_session_tick(session);
        }
    } else {
        /* No checkpoint, run from start */
        pb_game_reset(&session->game, session->seed);
        pb_playback_init(&session->playback, &session->replay);

        while (session->game.frame < target_frame && !session->finished) {
            pb_session_tick(session);
        }
    }

    return session->game.frame;
}

void pb_session_set_speed(pb_session* session, int speed_percent)
{
    if (!session) return;
    session->config.playback_speed = speed_percent;
    pb_playback_set_speed(&session->playback, speed_percent);
}

void pb_session_get_progress(const pb_session* session,
                             uint32_t* current_frame, uint32_t* total_frames)
{
    if (!session) return;

    if (current_frame) {
        *current_frame = session->game.frame;
    }
    if (total_frames) {
        *total_frames = session->replay.header.duration_frames;
    }
}

/*============================================================================
 * Recording Finalization
 *============================================================================*/

void pb_session_finalize(pb_session* session, pb_outcome outcome)
{
    if (!session) return;
    if (session->mode != PB_SESSION_RECORDING) return;

    pb_replay_finalize(&session->replay,
                       session->game.frame,
                       session->game.score,
                       outcome);

    session->finished = true;
}

const pb_replay* pb_session_get_replay(const pb_session* session)
{
    if (!session) return NULL;
    return &session->replay;
}

pb_result pb_session_extract_replay(pb_session* session, pb_replay* out_replay)
{
    if (!session || !out_replay) return PB_ERR_INVALID_ARG;
    if (session->mode != PB_SESSION_RECORDING) return PB_ERR_INVALID_STATE;

    *out_replay = session->replay;
    session->owns_replay = false;  /* Transfer ownership */

    return PB_OK;
}

/*============================================================================
 * Verification
 *============================================================================*/

bool pb_session_has_desync(const pb_session* session)
{
    if (!session) return false;
    return session->last_desync.detected;
}

const pb_desync_info* pb_session_get_desync(const pb_session* session)
{
    if (!session) return NULL;
    return &session->last_desync;
}

bool pb_session_verify_checkpoint(pb_session* session, int checkpoint_idx)
{
    if (!session) return false;
    if (checkpoint_idx < 0 || (uint32_t)checkpoint_idx >= session->replay.checkpoint_count) {
        return false;
    }

    const pb_checkpoint* cp = &session->replay.checkpoints[checkpoint_idx];
    uint32_t state_crc = pb_state_checksum(&session->game);
    uint32_t board_crc = pb_board_checksum(&session->game.board);

    return pb_checkpoint_verify(cp, state_crc, board_crc);
}

/*============================================================================
 * Twin Simulation
 *============================================================================*/

bool pb_twin_simulate(const pb_replay* replay, const pb_ruleset* ruleset,
                      pb_desync_info* info)
{
    if (!replay) return false;

    /* Run session A */
    pb_session session_a;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_VERIFICATION;
    config.verify_checksums = false;  /* We'll compare at end */

    pb_replay replay_copy_a = *replay;
    if (pb_session_create_playback(&session_a, &replay_copy_a, ruleset, &config) != PB_OK) {
        return false;
    }
    pb_session_run(&session_a, 0);
    uint32_t checksum_a = pb_state_checksum(&session_a.game);

    /* Run session B */
    pb_session session_b;
    pb_replay replay_copy_b = *replay;
    if (pb_session_create_playback(&session_b, &replay_copy_b, ruleset, &config) != PB_OK) {
        pb_session_destroy(&session_a);
        return false;
    }
    pb_session_run(&session_b, 0);
    uint32_t checksum_b = pb_state_checksum(&session_b.game);

    bool match = (checksum_a == checksum_b);

    if (!match && info) {
        info->detected = true;
        info->frame = session_a.game.frame;
        info->expected = checksum_a;
        info->actual = checksum_b;
        info->component = "twin_simulation";
    }

    pb_session_destroy(&session_a);
    pb_session_destroy(&session_b);

    return match;
}

int pb_create_golden_checksums(const pb_replay* replay,
                               const pb_ruleset* ruleset,
                               int interval,
                               uint32_t* checksums,
                               int max_checksums)
{
    if (!replay || !checksums || interval <= 0 || max_checksums <= 0) {
        return 0;
    }

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_PLAYBACK;
    config.auto_checkpoint = false;

    pb_replay replay_copy = *replay;
    if (pb_session_create_playback(&session, &replay_copy, ruleset, &config) != PB_OK) {
        return 0;
    }

    int count = 0;
    while (!session.finished && count < max_checksums) {
        pb_session_tick(&session);

        if (session.game.frame % (uint32_t)interval == 0) {
            checksums[count++] = pb_state_checksum(&session.game);
        }
    }

    pb_session_destroy(&session);
    return count;
}

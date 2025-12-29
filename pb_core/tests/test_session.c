/**
 * @file test_session.c
 * @brief Tests for pb_session game/replay integration layer
 */

#include <stdio.h>
#include <string.h>
#include "pb/pb_core.h"

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { \
    tests_total++; \
    printf("  %s... ", #name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("OK\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

/* ============================================================================
 * Session Lifecycle Tests
 * ============================================================================ */

static void test_session_config_default(void) {
    TEST(session_config_default);

    pb_session_config config;
    pb_session_config_default(&config);

    ASSERT(config.mode == PB_SESSION_LIVE, "default mode is LIVE");
    ASSERT(config.auto_checkpoint == true, "auto checkpoint enabled");
    ASSERT(config.checkpoint_interval == PB_REPLAY_CHECKPOINT_INTERVAL, "default interval");
    ASSERT(config.playback_speed == 100, "playback speed 100%");
    ASSERT(config.verify_checksums == true, "verify checksums enabled");

    PASS();
}

static void test_session_create_live(void) {
    TEST(session_create_live);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_LIVE;

    pb_result result = pb_session_create(&session, NULL, 12345, &config);
    ASSERT(result == PB_OK, "create succeeds");
    ASSERT(session.mode == PB_SESSION_LIVE, "mode is LIVE");
    ASSERT(session.active, "session is active");
    ASSERT(!session.finished, "session not finished");
    ASSERT(session.seed == 12345, "seed recorded");

    pb_session_destroy(&session);
    ASSERT(!session.active, "session inactive after destroy");

    PASS();
}

static void test_session_create_recording(void) {
    TEST(session_create_recording);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_result result = pb_session_create(&session, NULL, 42, &config);
    ASSERT(result == PB_OK, "create succeeds");
    ASSERT(session.mode == PB_SESSION_RECORDING, "mode is RECORDING");
    ASSERT(session.owns_replay, "owns replay");
    ASSERT(session.replay.header.seed == 42, "replay has seed");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_create_null_config(void) {
    TEST(session_create_null_config);

    pb_session session;
    pb_result result = pb_session_create(&session, NULL, 0, NULL);

    ASSERT(result == PB_OK, "create with NULL config succeeds");
    ASSERT(session.mode == PB_SESSION_LIVE, "default mode is LIVE");

    pb_session_destroy(&session);

    PASS();
}

/* ============================================================================
 * Recording Tests
 * ============================================================================ */

static void test_session_record_fire(void) {
    TEST(session_record_fire);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&session, NULL, 12345, &config);

    /* Fire - this should record an event */
    pb_result result = pb_session_fire(&session);
    ASSERT(result == PB_OK, "fire succeeds");
    ASSERT(session.replay.event_count == 1, "fire event recorded");

    /* Run until shot lands (more frames needed for fixed-point mode) */
    for (int i = 0; i < 500 && session.game.shot.phase != PB_SHOT_IDLE; i++) {
        pb_session_tick(&session);
    }
    ASSERT(session.game.shot.phase == PB_SHOT_IDLE, "shot landed");

    /* Fire again (only if game not over) */
    if (!pb_game_is_over(&session.game)) {
        result = pb_session_fire(&session);
        ASSERT(result == PB_OK, "second fire succeeds");
        ASSERT(session.replay.event_count == 2, "second fire recorded");
    }

    pb_session_finalize(&session, PB_OUTCOME_ABANDONED);
    ASSERT(session.finished, "session finished after finalize");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_record_swap(void) {
    TEST(session_record_swap);

    /* Use ARCADE mode which allows color switching */
    pb_ruleset ruleset;
    pb_ruleset_default(&ruleset, PB_MODE_ARCADE);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&session, &ruleset, 99, &config);

    pb_result result = pb_session_swap(&session);
    ASSERT(result == PB_OK, "swap succeeds in arcade mode");
    ASSERT(session.replay.event_count == 1, "swap recorded");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_record_pause(void) {
    TEST(session_record_pause);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&session, NULL, 123, &config);

    pb_session_pause(&session, true);
    pb_session_pause(&session, false);

    ASSERT(session.replay.event_count == 2, "pause/unpause recorded");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_record_rotate(void) {
    TEST(session_record_rotate);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&session, NULL, 456, &config);

    pb_session_rotate(&session, PB_FLOAT_TO_FIXED(0.1f));
    pb_session_rotate(&session, PB_FLOAT_TO_FIXED(-0.1f));

    ASSERT(session.replay.event_count == 2, "rotations recorded");

    pb_session_destroy(&session);

    PASS();
}

/* ============================================================================
 * Tick and Run Tests
 * ============================================================================ */

static void test_session_tick_increments_frame(void) {
    TEST(session_tick_increments_frame);

    pb_session session;
    pb_session_create(&session, NULL, 0, NULL);

    uint32_t frame0 = session.game.frame;
    pb_session_tick(&session);
    uint32_t frame1 = session.game.frame;
    pb_session_tick(&session);
    uint32_t frame2 = session.game.frame;

    ASSERT(frame1 == frame0 + 1, "first tick increments");
    ASSERT(frame2 == frame1 + 1, "second tick increments");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_run_limited(void) {
    TEST(session_run_limited);

    pb_session session;
    pb_session_create(&session, NULL, 0, NULL);

    int frames = pb_session_run(&session, 50);

    ASSERT(frames == 50, "ran exactly 50 frames");
    ASSERT(session.game.frame == 50, "game at frame 50");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_finalize_updates_replay(void) {
    TEST(session_finalize_updates_replay);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&session, NULL, 789, &config);

    /* Play some frames */
    pb_session_run(&session, 100);
    session.game.score = 5000;

    pb_session_finalize(&session, PB_OUTCOME_WON);

    const pb_replay* replay = pb_session_get_replay(&session);
    ASSERT(replay != NULL, "replay available");
    ASSERT(replay->header.duration_frames == 100, "duration recorded");
    ASSERT(replay->header.final_score == 5000, "score recorded");
    ASSERT(replay->header.outcome == PB_OUTCOME_WON, "outcome recorded");

    pb_session_destroy(&session);

    PASS();
}

/* ============================================================================
 * Playback Tests
 * ============================================================================ */

static void test_session_playback_create(void) {
    TEST(session_playback_create);

    /* First record a session */
    pb_session rec;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&rec, NULL, 12345, &config);
    pb_session_fire(&rec);
    pb_session_run(&rec, 50);
    pb_session_finalize(&rec, PB_OUTCOME_ABANDONED);

    /* Extract replay */
    pb_replay replay;
    pb_session_extract_replay(&rec, &replay);
    pb_session_destroy(&rec);

    /* Create playback session */
    pb_session play;
    config.mode = PB_SESSION_PLAYBACK;
    pb_result result = pb_session_create_playback(&play, &replay, NULL, &config);

    ASSERT(result == PB_OK, "playback create succeeds");
    ASSERT(play.mode == PB_SESSION_PLAYBACK, "mode is PLAYBACK");
    ASSERT(play.seed == 12345, "seed from replay");

    pb_session_destroy(&play);
    pb_replay_free(&replay);

    PASS();
}

static void test_session_playback_progress(void) {
    TEST(session_playback_progress);

    /* Record */
    pb_session rec;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&rec, NULL, 999, &config);
    int recorded = pb_session_run(&rec, 100);
    pb_session_finalize(&rec, PB_OUTCOME_ABANDONED);

    pb_replay replay;
    pb_session_extract_replay(&rec, &replay);
    pb_session_destroy(&rec);

    ASSERT(replay.header.duration_frames == (uint32_t)recorded, "duration matches recorded");

    /* Playback */
    pb_session play;
    config.mode = PB_SESSION_PLAYBACK;
    pb_session_create_playback(&play, &replay, NULL, &config);

    uint32_t current, total;
    pb_session_get_progress(&play, &current, &total);

    ASSERT(current == 0, "starts at frame 0");
    ASSERT(total == replay.header.duration_frames, "total matches replay");

    int played = pb_session_run(&play, 30);
    pb_session_get_progress(&play, &current, &total);
    ASSERT(current == (uint32_t)played, "frame matches played count");
    ASSERT(played <= 30, "played at most 30 frames");

    pb_session_destroy(&play);
    pb_replay_free(&replay);

    PASS();
}

static void test_session_set_speed(void) {
    TEST(session_set_speed);

    pb_session session;
    pb_session_create(&session, NULL, 0, NULL);

    pb_session_set_speed(&session, 200);
    ASSERT(session.config.playback_speed == 200, "speed updated");

    pb_session_set_speed(&session, 50);
    ASSERT(session.config.playback_speed == 50, "speed updated again");

    pb_session_destroy(&session);

    PASS();
}

/* ============================================================================
 * Checksum and Verification Tests
 * ============================================================================ */

static void test_session_records_frame_checksums(void) {
    TEST(session_records_frame_checksums);

    pb_session session;
    pb_session_create(&session, NULL, 12345, NULL);

    pb_session_run(&session, 10);

    /* Should have checksums in buffer */
    ASSERT(session.checksum_buf.count > 0, "checksums recorded");

    uint32_t checksum;
    bool found = pb_checksum_buffer_find(&session.checksum_buf, 5, &checksum);
    ASSERT(found, "can find frame 5 checksum");
    ASSERT(checksum != 0, "checksum is non-zero");

    pb_session_destroy(&session);

    PASS();
}

static void test_session_verification_no_desync(void) {
    TEST(session_verification_no_desync);

    /* Record with checkpoints */
    pb_session rec;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;
    config.checkpoint_interval = 50;

    pb_session_create(&rec, NULL, 12345, &config);
    pb_session_run(&rec, 200);
    pb_session_finalize(&rec, PB_OUTCOME_ABANDONED);

    pb_replay replay;
    pb_session_extract_replay(&rec, &replay);
    pb_session_destroy(&rec);

    /* Verify playback */
    pb_session verify;
    config.mode = PB_SESSION_VERIFICATION;
    config.verify_checksums = true;
    pb_session_create_playback(&verify, &replay, NULL, &config);

    pb_session_run(&verify, 0);

    ASSERT(!pb_session_has_desync(&verify), "no desync detected");

    pb_session_destroy(&verify);
    pb_replay_free(&replay);

    PASS();
}

/* ============================================================================
 * Twin Simulation Tests
 * ============================================================================ */

static void test_twin_simulate_determinism(void) {
    TEST(twin_simulate_determinism);

    /* Record a session */
    pb_session rec;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&rec, NULL, 42, &config);

    /* Simulate some gameplay */
    for (int i = 0; i < 5; i++) {
        pb_session_fire(&rec);
        pb_session_run(&rec, 30);
    }
    pb_session_finalize(&rec, PB_OUTCOME_ABANDONED);

    pb_replay replay;
    pb_session_extract_replay(&rec, &replay);
    pb_session_destroy(&rec);

    /* Twin simulation should match */
    pb_desync_info info;
    memset(&info, 0, sizeof(info));
    bool match = pb_twin_simulate(&replay, NULL, &info);

    ASSERT(match, "twin simulation matches");
    ASSERT(!info.detected, "no desync in twin sim");

    pb_replay_free(&replay);

    PASS();
}

static void test_golden_checksums(void) {
    TEST(golden_checksums);

    /* Record - run enough frames to get multiple checksums */
    pb_session rec;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&rec, NULL, 12345, &config);
    pb_session_run(&rec, 100);
    pb_session_finalize(&rec, PB_OUTCOME_ABANDONED);

    pb_replay replay;
    pb_session_extract_replay(&rec, &replay);
    pb_session_destroy(&rec);

    /* Generate golden checksums - use smaller interval for more checksums */
    uint32_t checksums[20];
    int count = pb_create_golden_checksums(&replay, NULL, 10, checksums, 20);

    /* Should get some checksums (at least 1 for every 10 frames recorded) */
    ASSERT(count > 0, "got at least one checksum");
    if (count > 0) {
        ASSERT(checksums[0] != 0, "first checksum non-zero");
    }

    /* Verify they're deterministic */
    uint32_t checksums2[20];
    int count2 = pb_create_golden_checksums(&replay, NULL, 10, checksums2, 20);

    ASSERT(count == count2, "same count");
    for (int i = 0; i < count; i++) {
        ASSERT(checksums[i] == checksums2[i], "checksums match");
    }

    pb_replay_free(&replay);

    PASS();
}

/* ============================================================================
 * Extract Replay Tests
 * ============================================================================ */

static void test_session_extract_replay_transfers_ownership(void) {
    TEST(session_extract_replay_transfers_ownership);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;

    pb_session_create(&session, NULL, 111, &config);
    pb_session_fire(&session);
    pb_session_run(&session, 10);

    pb_replay replay;
    pb_result result = pb_session_extract_replay(&session, &replay);

    ASSERT(result == PB_OK, "extract succeeds");
    ASSERT(!session.owns_replay, "session no longer owns replay");
    ASSERT(replay.header.seed == 111, "replay has correct seed");

    /* Session destroy should not free the replay now */
    pb_session_destroy(&session);

    /* We must free it */
    pb_replay_free(&replay);

    PASS();
}

static void test_session_extract_replay_wrong_mode(void) {
    TEST(session_extract_replay_wrong_mode);

    pb_session session;
    pb_session_create(&session, NULL, 0, NULL);  /* LIVE mode */

    pb_replay replay;
    pb_result result = pb_session_extract_replay(&session, &replay);

    ASSERT(result == PB_ERR_INVALID_STATE, "extract fails in LIVE mode");

    pb_session_destroy(&session);

    PASS();
}

/* ============================================================================
 * Checkpoint Tests
 * ============================================================================ */

static void test_session_auto_checkpoint(void) {
    TEST(session_auto_checkpoint);

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;
    config.auto_checkpoint = true;
    config.checkpoint_interval = 50;

    pb_session_create(&session, NULL, 12345, &config);
    pb_session_run(&session, 150);

    /* Should have created at least 2 checkpoints (at frames 50, 100) */
    ASSERT(session.replay.checkpoint_count >= 2, "checkpoints created");

    pb_session_destroy(&session);

    PASS();
}

static int checkpoint_callback_count = 0;

static void checkpoint_test_callback(const pb_checkpoint* cp, void* ud) {
    (void)cp; (void)ud;
    checkpoint_callback_count++;
}

static void test_session_checkpoint_callback(void) {
    TEST(session_checkpoint_callback);

    checkpoint_callback_count = 0;

    pb_session session;
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;
    config.checkpoint_interval = 25;
    config.on_checkpoint = checkpoint_test_callback;

    pb_session_create(&session, NULL, 42, &config);
    pb_session_run(&session, 100);

    ASSERT(session.replay.checkpoint_count >= 3, "checkpoints created");
    ASSERT(checkpoint_callback_count >= 3, "callback invoked");

    pb_session_destroy(&session);

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("pb_session test suite\n");
    printf("=====================\n\n");

    printf("Session lifecycle:\n");
    test_session_config_default();
    test_session_create_live();
    test_session_create_recording();
    test_session_create_null_config();

    printf("\nRecording:\n");
    test_session_record_fire();
    test_session_record_swap();
    test_session_record_pause();
    test_session_record_rotate();

    printf("\nTick and run:\n");
    test_session_tick_increments_frame();
    test_session_run_limited();
    test_session_finalize_updates_replay();

    printf("\nPlayback:\n");
    test_session_playback_create();
    test_session_playback_progress();
    test_session_set_speed();

    printf("\nChecksum and verification:\n");
    test_session_records_frame_checksums();
    test_session_verification_no_desync();

    printf("\nTwin simulation:\n");
    test_twin_simulate_determinism();
    test_golden_checksums();

    printf("\nExtract replay:\n");
    test_session_extract_replay_transfers_ownership();
    test_session_extract_replay_wrong_mode();

    printf("\nCheckpoints:\n");
    test_session_auto_checkpoint();
    test_session_checkpoint_callback();

    printf("\n=====================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

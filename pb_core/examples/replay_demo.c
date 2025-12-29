/*
 * replay_demo.c - Demonstrates replay recording and playback
 *
 * Shows:
 * - Session-based game management
 * - Automatic replay recording
 * - Saving replay to file
 * - Loading and playing back replay
 * - Checksum verification for determinism
 *
 * Build:
 *   make examples
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_SHOTS 10
#define REPLAY_FILE "/tmp/pb_demo.pbr"

/*
 * Play a short game with scripted inputs, recording to replay
 */
static void record_game(uint64_t seed)
{
    printf("=== RECORDING GAME ===\n");

    /* Create session configuration */
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_RECORDING;
    config.auto_checkpoint = true;
    config.checkpoint_interval = 60;  /* Checkpoint every second at 60fps */

    /* Create session with recording */
    pb_session session;
    pb_result res = pb_session_create(&session, NULL, seed, &config);
    if (res != PB_OK) {
        fprintf(stderr, "Failed to create session: %d\n", res);
        return;
    }

    /* Place some test bubbles */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    for (int col = 0; col < 5; col++) {
        pb_board_set(&session.game.board, (pb_offset){0, col}, b);
    }

    printf("Seed: %lu\n", (unsigned long)seed);
    printf("Initial checksum: 0x%08X\n", pb_game_checksum(&session.game));

    /* Fire several shots with varying angles */
    pb_scalar angles[] = {
        PB_FLOAT_TO_FIXED(1.57f),   /* Straight up */
        PB_FLOAT_TO_FIXED(1.2f),    /* Slight left */
        PB_FLOAT_TO_FIXED(1.9f),    /* Slight right */
        PB_FLOAT_TO_FIXED(1.57f),   /* Straight up */
        PB_FLOAT_TO_FIXED(1.0f),    /* More left */
    };

    for (int shot = 0; shot < 5 && !pb_game_is_over(&session.game); shot++) {
        /* Set angle and fire */
        pb_session_set_angle(&session, angles[shot]);
        pb_session_fire(&session);
        printf("Shot %d: angle=%.3f\n", shot + 1, PB_FIXED_TO_FLOAT(angles[shot]));

        /* Run until shot completes */
        int ticks = 0;
        while (session.game.shot.phase != PB_SHOT_IDLE && ticks < 200) {
            pb_session_tick(&session);
            ticks++;
        }
        printf("  Completed in %d ticks, score=%u\n", ticks, session.game.score);
    }

    /* Finalize recording */
    pb_session_finalize(&session, PB_OUTCOME_WON);
    printf("\nFinal checksum: 0x%08X\n", pb_game_checksum(&session.game));
    printf("Final score: %u\n", session.game.score);

    /* Get the recorded replay */
    const pb_replay* replay = pb_session_get_replay(&session);
    if (replay) {
        printf("Recorded %d events\n", replay->event_count);

        /* Save to file */
        res = pb_replay_save(replay, REPLAY_FILE);
        if (res == PB_OK) {
            printf("Saved replay to: %s\n", REPLAY_FILE);
        } else {
            fprintf(stderr, "Failed to save replay: %d\n", res);
        }
    }

    pb_session_destroy(&session);
}

/*
 * Load and playback a recorded replay
 */
static void playback_game(void)
{
    printf("\n=== PLAYING BACK REPLAY ===\n");

    /* Load replay from file */
    pb_replay replay;
    pb_result res = pb_replay_load(REPLAY_FILE, &replay);
    if (res != PB_OK) {
        fprintf(stderr, "Failed to load replay from: %s (error %d)\n", REPLAY_FILE, res);
        return;
    }

    printf("Loaded replay: %d events, seed=%lu\n",
           replay.event_count, (unsigned long)replay.header.seed);

    /* Create session config for playback */
    pb_session_config config;
    pb_session_config_default(&config);
    config.mode = PB_SESSION_PLAYBACK;
    config.verify_checksums = true;

    /* Create playback session */
    pb_session session;
    res = pb_session_create_playback(&session, &replay, NULL, &config);
    if (res != PB_OK) {
        fprintf(stderr, "Failed to create playback session: %d\n", res);
        pb_replay_free(&replay);
        return;
    }

    /* Same initial board setup as recording */
    pb_bubble b = {PB_KIND_COLORED, 0, 0, PB_SPECIAL_NONE, {0}};
    for (int col = 0; col < 5; col++) {
        pb_board_set(&session.game.board, (pb_offset){0, col}, b);
    }

    printf("Initial checksum: 0x%08X\n", pb_game_checksum(&session.game));

    /* Run playback */
    int frame = 0;
    int shots_seen = 0;
    while (!session.finished && frame < 2000) {
        int events = pb_session_tick(&session);

        /* Check for fire events */
        for (int i = 0; i < session.game.event_count; i++) {
            if (session.game.events[i].type == PB_EVENT_FIRE) {
                shots_seen++;
                printf("Playback shot %d at frame %d\n", shots_seen, frame);
            }
        }
        pb_game_clear_events(&session.game);

        /* Check for desync */
        if (pb_session_has_desync(&session)) {
            const pb_desync_info* desync = pb_session_get_desync(&session);
            printf("DESYNC at frame %u! Expected 0x%08X, got 0x%08X\n",
                   desync->frame, desync->expected, desync->actual);
            break;
        }

        frame++;
        (void)events;
    }

    printf("\nPlayback finished after %d frames\n", frame);
    printf("Final checksum: 0x%08X\n", pb_game_checksum(&session.game));
    printf("Final score: %u\n", session.game.score);

    /* Verify determinism */
    if (!pb_session_has_desync(&session)) {
        printf("\n*** CHECKSUM VERIFIED - Deterministic playback confirmed! ***\n");
    }

    pb_session_destroy(&session);
    /* Note: replay is freed by session_destroy if it owns it */
}

int main(void)
{
    printf("pb_core Replay Demo\n");
    printf("===================\n\n");

    /* Use fixed seed for reproducibility */
    uint64_t seed = 12345;

    record_game(seed);
    playback_game();

    /* Cleanup */
    remove(REPLAY_FILE);

    return 0;
}

/**
 * @file test_replay.c
 * @brief Tests for pb_replay binary format and serialization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
 * Varint Tests
 * ============================================================================ */

static void test_varint_small(void) {
    TEST(varint_small);

    uint8_t buf[5];
    uint32_t value;

    /* Single byte: values 0-127 */
    int len = pb_varint_encode(0, buf);
    ASSERT(len == 1, "0 should be 1 byte");
    ASSERT(buf[0] == 0, "0 encoding");

    len = pb_varint_encode(127, buf);
    ASSERT(len == 1, "127 should be 1 byte");
    ASSERT(buf[0] == 127, "127 encoding");

    /* Decode */
    len = pb_varint_decode(buf, 5, &value);
    ASSERT(len == 1, "127 decode len");
    ASSERT(value == 127, "127 decode value");

    PASS();
}

static void test_varint_medium(void) {
    TEST(varint_medium);

    uint8_t buf[5];
    uint32_t value;

    /* Two bytes: 128-16383 */
    int len = pb_varint_encode(128, buf);
    ASSERT(len == 2, "128 should be 2 bytes");
    ASSERT((buf[0] & 0x80) != 0, "128 continuation bit");

    len = pb_varint_decode(buf, 5, &value);
    ASSERT(len == 2, "128 decode len");
    ASSERT(value == 128, "128 decode value");

    /* 300 = 0b100101100 = 0x012C */
    len = pb_varint_encode(300, buf);
    ASSERT(len == 2, "300 should be 2 bytes");
    len = pb_varint_decode(buf, 5, &value);
    ASSERT(value == 300, "300 roundtrip");

    PASS();
}

static void test_varint_large(void) {
    TEST(varint_large);

    uint8_t buf[5];
    uint32_t value;

    /* Test various large values */
    uint32_t test_values[] = {16384, 65535, 100000, 1000000, 0xFFFFFFFF};
    int expected_sizes[] = {3, 3, 3, 3, 5};

    for (int i = 0; i < 5; i++) {
        int len = pb_varint_encode(test_values[i], buf);
        ASSERT(len == expected_sizes[i], "large value size");

        len = pb_varint_decode(buf, 5, &value);
        ASSERT(value == test_values[i], "large value roundtrip");
    }

    PASS();
}

static void test_varint_size(void) {
    TEST(varint_size);

    ASSERT(pb_varint_size(0) == 1, "size(0)");
    ASSERT(pb_varint_size(127) == 1, "size(127)");
    ASSERT(pb_varint_size(128) == 2, "size(128)");
    ASSERT(pb_varint_size(16383) == 2, "size(16383)");
    ASSERT(pb_varint_size(16384) == 3, "size(16384)");
    ASSERT(pb_varint_size(2097151) == 3, "size(2097151)");
    ASSERT(pb_varint_size(2097152) == 4, "size(2097152)");
    ASSERT(pb_varint_size(268435455) == 4, "size(268435455)");
    ASSERT(pb_varint_size(268435456) == 5, "size(268435456)");

    PASS();
}

/* ============================================================================
 * Event Type Mapping Tests
 * ============================================================================ */

static void test_event_type_mapping(void) {
    TEST(event_type_mapping);

    /* Input events should round-trip */
    ASSERT(pb_event_to_input_type(PB_EVENT_FIRE) == PB_INPUT_FIRE, "fire map");
    ASSERT(pb_input_to_event_type(PB_INPUT_FIRE) == PB_EVENT_FIRE, "fire unmap");

    ASSERT(pb_event_to_input_type(PB_EVENT_ROTATE_LEFT) == PB_INPUT_ROTATE_LEFT, "rotate_left");
    ASSERT(pb_event_to_input_type(PB_EVENT_SWITCH_BUBBLE) == PB_INPUT_SWITCH, "switch");

    /* Non-input events should map to NONE */
    ASSERT(pb_event_to_input_type(PB_EVENT_BUBBLE_PLACED) == PB_INPUT_NONE, "placed -> none");
    ASSERT(pb_event_to_input_type(PB_EVENT_BUBBLES_POPPED) == PB_INPUT_NONE, "popped -> none");
    ASSERT(pb_event_to_input_type(PB_EVENT_GAME_OVER) == PB_INPUT_NONE, "gameover -> none");

    PASS();
}

/* ============================================================================
 * Event Packing Tests
 * ============================================================================ */

static void test_event_pack_simple(void) {
    TEST(event_pack_simple);

    uint8_t buf[16];
    pb_input_event event = {
        .type = PB_INPUT_ROTATE_LEFT,
        .frame = 100,
        .angle = 0
    };

    /* Pack with delta from frame 0 */
    int len = pb_event_pack(&event, 0, buf, false);
    ASSERT(len >= 2, "rotate should pack to 2+ bytes");

    /* Unpack */
    pb_input_event unpacked;
    int consumed = pb_event_unpack(buf, len, 0, &unpacked, false);
    ASSERT(consumed == len, "consumed matches");
    ASSERT(unpacked.type == PB_INPUT_ROTATE_LEFT, "type matches");
    ASSERT(unpacked.frame == 100, "frame matches");

    PASS();
}

static void test_event_pack_fire(void) {
    TEST(event_pack_fire);

    uint8_t buf[16];
    float test_angle = 1.5708f;  /* ~90 degrees */

    pb_input_event event = {
        .type = PB_INPUT_FIRE,
        .frame = 250,
        .angle = PB_FLOAT_TO_FIXED(test_angle)
    };

    /* Pack (float mode) */
    int len = pb_event_pack(&event, 200, buf, false);
    ASSERT(len >= 5, "fire should pack to 5+ bytes");

    /* Unpack */
    pb_input_event unpacked;
    int consumed = pb_event_unpack(buf, len, 200, &unpacked, false);
    ASSERT(consumed == len, "consumed matches");
    ASSERT(unpacked.type == PB_INPUT_FIRE, "type is fire");
    ASSERT(unpacked.frame == 250, "frame is 250");

    float unpacked_angle = PB_FIXED_TO_FLOAT(unpacked.angle);
    ASSERT(fabsf(unpacked_angle - test_angle) < 0.001f, "angle matches");

    PASS();
}

static void test_event_pack_delta(void) {
    TEST(event_pack_delta);

    uint8_t buf[16];
    pb_input_event events[3] = {
        {.type = PB_INPUT_PAUSE, .frame = 60, .angle = 0},
        {.type = PB_INPUT_UNPAUSE, .frame = 180, .angle = 0},
        {.type = PB_INPUT_SWITCH, .frame = 181, .angle = 0}
    };

    /* Pack with increasing deltas */
    int total_len = 0;
    uint32_t prev = 0;
    for (int i = 0; i < 3; i++) {
        int len = pb_event_pack(&events[i], prev, buf + total_len, false);
        prev = events[i].frame;
        total_len += len;
    }

    /* Unpack all */
    int offset = 0;
    prev = 0;
    for (int i = 0; i < 3; i++) {
        pb_input_event unpacked;
        int consumed = pb_event_unpack(buf + offset, total_len - offset, prev, &unpacked, false);
        ASSERT(consumed > 0, "unpack succeeded");
        ASSERT(unpacked.type == events[i].type, "type matches");
        ASSERT(unpacked.frame == events[i].frame, "frame matches");
        prev = unpacked.frame;
        offset += consumed;
    }

    PASS();
}

/* ============================================================================
 * Replay Lifecycle Tests
 * ============================================================================ */

static void test_replay_init_free(void) {
    TEST(replay_init_free);

    pb_replay replay;
    pb_replay_init(&replay, 12345, "level1", "classic");

    ASSERT(replay.header.magic == PB_REPLAY_MAGIC, "magic set");
    ASSERT(replay.header.version == PB_REPLAY_VERSION, "version set");
    ASSERT(replay.header.seed == 12345, "seed set");
    ASSERT(strcmp(replay.header.level_id, "level1") == 0, "level_id set");
    ASSERT(replay.events != NULL, "events allocated");
    ASSERT(replay.checkpoints != NULL, "checkpoints allocated");

    pb_replay_free(&replay);
    ASSERT(replay.events == NULL, "events freed");
    ASSERT(replay.checkpoints == NULL, "checkpoints freed");

    PASS();
}

static void test_replay_record_events(void) {
    TEST(replay_record_events);

    pb_replay replay;
    pb_replay_init(&replay, 42, NULL, NULL);

    pb_input_event e1 = {.type = PB_INPUT_ROTATE_LEFT, .frame = 10};
    pb_input_event e2 = {.type = PB_INPUT_FIRE, .frame = 50, .angle = PB_FLOAT_TO_FIXED(1.0f)};
    pb_input_event e3 = {.type = PB_INPUT_SWITCH, .frame = 100};

    ASSERT(pb_replay_record_event(&replay, &e1) == PB_OK, "record e1");
    ASSERT(pb_replay_record_event(&replay, &e2) == PB_OK, "record e2");
    ASSERT(pb_replay_record_event(&replay, &e3) == PB_OK, "record e3");

    ASSERT(replay.event_count == 3, "3 events recorded");
    ASSERT(replay.events[1].type == PB_INPUT_FIRE, "e2 type");
    ASSERT(replay.events[1].frame == 50, "e2 frame");

    pb_replay_free(&replay);
    PASS();
}

static void test_replay_record_from_pb_event(void) {
    TEST(replay_record_from_pb_event);

    pb_replay replay;
    pb_replay_init(&replay, 42, NULL, NULL);

    /* Input event should be recorded */
    pb_event e1 = {.type = PB_EVENT_FIRE, .frame = 100, .data.fire.angle = PB_FLOAT_TO_FIXED(0.5f)};
    ASSERT(pb_replay_record(&replay, &e1) == PB_OK, "record fire");
    ASSERT(replay.event_count == 1, "1 event recorded");

    /* Non-input event should be silently ignored */
    pb_event e2 = {.type = PB_EVENT_BUBBLES_POPPED, .frame = 101};
    ASSERT(pb_replay_record(&replay, &e2) == PB_OK, "ignore non-input");
    ASSERT(replay.event_count == 1, "still 1 event");

    pb_replay_free(&replay);
    PASS();
}

/* ============================================================================
 * Serialization Tests
 * ============================================================================ */

static void test_replay_serialize_roundtrip(void) {
    TEST(replay_serialize_roundtrip);

    /* Create replay with events */
    pb_replay replay;
    pb_replay_init(&replay, 98765, "test_level", "test_rules");

    pb_input_event events[] = {
        {.type = PB_INPUT_ROTATE_LEFT, .frame = 30},
        {.type = PB_INPUT_FIRE, .frame = 60, .angle = PB_FLOAT_TO_FIXED(1.57f)},
        {.type = PB_INPUT_SWITCH, .frame = 120},
        {.type = PB_INPUT_FIRE, .frame = 180, .angle = PB_FLOAT_TO_FIXED(-0.5f)},
    };

    for (int i = 0; i < 4; i++) {
        pb_replay_record_event(&replay, &events[i]);
    }

    pb_replay_finalize(&replay, 200, 1500, PB_OUTCOME_WON);

    /* Serialize */
    size_t size = pb_replay_serialized_size(&replay);
    uint8_t* buffer = malloc(size);
    ASSERT(buffer != NULL, "buffer allocated");

    size_t written = pb_replay_serialize(&replay, buffer, size);
    ASSERT(written > 0, "serialization succeeded");

    /* Deserialize */
    pb_replay loaded;
    pb_result result = pb_replay_deserialize(buffer, written, &loaded);
    ASSERT(result == PB_OK, "deserialization succeeded");

    /* Verify */
    ASSERT(loaded.header.magic == PB_REPLAY_MAGIC, "magic matches");
    ASSERT(loaded.header.seed == 98765, "seed matches");
    ASSERT(strcmp(loaded.header.level_id, "test_level") == 0, "level_id matches");
    ASSERT(loaded.header.event_count == 4, "event count matches");
    ASSERT(loaded.header.final_score == 1500, "score matches");
    ASSERT(loaded.header.outcome == PB_OUTCOME_WON, "outcome matches");

    ASSERT(loaded.event_count == 4, "loaded event_count");
    ASSERT(loaded.events[0].type == PB_INPUT_ROTATE_LEFT, "event 0 type");
    ASSERT(loaded.events[0].frame == 30, "event 0 frame");
    ASSERT(loaded.events[1].type == PB_INPUT_FIRE, "event 1 type");
    ASSERT(loaded.events[2].frame == 120, "event 2 frame");

    float angle1 = PB_FIXED_TO_FLOAT(loaded.events[1].angle);
    ASSERT(fabsf(angle1 - 1.57f) < 0.01f, "event 1 angle");

    free(buffer);
    pb_replay_free(&replay);
    pb_replay_free(&loaded);
    PASS();
}

static void test_replay_file_roundtrip(void) {
    TEST(replay_file_roundtrip);

    pb_replay replay;
    pb_replay_init(&replay, 11111, "file_test", NULL);

    pb_input_event e = {.type = PB_INPUT_FIRE, .frame = 42, .angle = PB_FLOAT_TO_FIXED(0.0f)};
    pb_replay_record_event(&replay, &e);
    pb_replay_finalize(&replay, 100, 500, PB_OUTCOME_LOST);

    /* Save */
    const char* path = "/tmp/test_replay.pbrp";
    ASSERT(pb_replay_save(&replay, path) == PB_OK, "save succeeded");

    /* Load */
    pb_replay loaded;
    ASSERT(pb_replay_load(path, &loaded) == PB_OK, "load succeeded");

    ASSERT(loaded.header.seed == 11111, "seed matches after file roundtrip");
    ASSERT(loaded.event_count == 1, "event count matches");
    ASSERT(loaded.events[0].frame == 42, "event frame matches");

    pb_replay_free(&replay);
    pb_replay_free(&loaded);
    PASS();
}

/* ============================================================================
 * Playback Tests
 * ============================================================================ */

static void test_playback_basic(void) {
    TEST(playback_basic);

    pb_replay replay;
    pb_replay_init(&replay, 42, NULL, NULL);

    pb_input_event events[] = {
        {.type = PB_INPUT_ROTATE_LEFT, .frame = 10},
        {.type = PB_INPUT_FIRE, .frame = 30, .angle = PB_FLOAT_TO_FIXED(1.0f)},
        {.type = PB_INPUT_SWITCH, .frame = 60},
    };
    for (int i = 0; i < 3; i++) {
        pb_replay_record_event(&replay, &events[i]);
    }

    pb_playback playback;
    pb_playback_init(&playback, &replay);

    ASSERT(playback.current_frame == 0, "starts at frame 0");
    ASSERT(!playback.finished, "not finished");

    /* Advance to frame 10 - should get first event */
    pb_input_event got;
    for (int f = 0; f < 10; f++) {
        ASSERT(!pb_playback_get_event(&playback, &got), "no event before frame 10");
        pb_playback_advance(&playback);
    }

    ASSERT(pb_playback_get_event(&playback, &got), "event at frame 10");
    ASSERT(got.type == PB_INPUT_ROTATE_LEFT, "got rotate_left");
    ASSERT(got.frame == 10, "frame is 10");

    pb_replay_free(&replay);
    PASS();
}

static void test_playback_speed(void) {
    TEST(playback_speed);

    pb_replay replay;
    pb_replay_init(&replay, 42, NULL, NULL);

    pb_playback playback;
    pb_playback_init(&playback, &replay);

    ASSERT(playback.speed_multiplier == 100, "default 1x");
    ASSERT(!playback.paused, "not paused");

    pb_playback_set_speed(&playback, 200);
    ASSERT(playback.speed_multiplier == 200, "2x speed");

    pb_playback_set_speed(&playback, 0);
    ASSERT(playback.paused, "paused at 0");

    pb_playback_set_speed(&playback, 50);
    ASSERT(!playback.paused, "unpaused");
    ASSERT(playback.speed_multiplier == 50, "0.5x speed");

    pb_replay_free(&replay);
    PASS();
}

/* ============================================================================
 * Checkpoint Tests
 * ============================================================================ */

static void test_checkpoint_add(void) {
    TEST(checkpoint_add);

    pb_replay replay;
    pb_replay_init(&replay, 42, NULL, NULL);

    pb_rng rng;
    pb_rng_seed(&rng, 12345);

    pb_result r = pb_replay_add_checkpoint(&replay, 300, 0xAABBCCDD, 0x11223344,
                                           &rng, 1000, 5);
    ASSERT(r == PB_OK, "add checkpoint");
    ASSERT(replay.checkpoint_count == 1, "1 checkpoint");

    pb_checkpoint* cp = &replay.checkpoints[0];
    ASSERT(cp->frame == 300, "checkpoint frame");
    ASSERT(cp->state_checksum == 0xAABBCCDD, "state checksum");
    ASSERT(cp->score == 1000, "checkpoint score");

    pb_replay_free(&replay);
    PASS();
}

static void test_checkpoint_verify(void) {
    TEST(checkpoint_verify);

    pb_checkpoint cp = {
        .frame = 100,
        .state_checksum = 0x12345678,
        .board_checksum = 0x87654321
    };

    ASSERT(pb_checkpoint_verify(&cp, 0x12345678, 0x87654321), "verify match");
    ASSERT(!pb_checkpoint_verify(&cp, 0x12345678, 0x00000000), "verify mismatch board");
    ASSERT(!pb_checkpoint_verify(&cp, 0x00000000, 0x87654321), "verify mismatch state");

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("pb_replay test suite\n");
    printf("====================\n\n");

    printf("Varint encoding:\n");
    test_varint_small();
    test_varint_medium();
    test_varint_large();
    test_varint_size();

    printf("\nEvent type mapping:\n");
    test_event_type_mapping();

    printf("\nEvent packing:\n");
    test_event_pack_simple();
    test_event_pack_fire();
    test_event_pack_delta();

    printf("\nReplay lifecycle:\n");
    test_replay_init_free();
    test_replay_record_events();
    test_replay_record_from_pb_event();

    printf("\nSerialization:\n");
    test_replay_serialize_roundtrip();
    test_replay_file_roundtrip();

    printf("\nPlayback:\n");
    test_playback_basic();
    test_playback_speed();

    printf("\nCheckpoints:\n");
    test_checkpoint_add();
    test_checkpoint_verify();

    printf("\n====================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

/**
 * @file pb_replay.c
 * @brief Deterministic replay recording and playback implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_replay.h"
#include <stdlib.h>

#include <stdio.h>

/*============================================================================
 * Varint Encoding/Decoding (LEB128)
 *
 * LEB128 encodes integers in 7-bit chunks with a continuation bit.
 * Values < 128 fit in 1 byte, < 16384 in 2 bytes, etc.
 *============================================================================*/

int pb_varint_encode(uint32_t value, uint8_t* out)
{
    int bytes = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;  /* Set continuation bit */
        }
        out[bytes++] = byte;
    } while (value != 0);
    return bytes;
}

int pb_varint_decode(const uint8_t* data, int len, uint32_t* out_value)
{
    uint32_t result = 0;
    int shift = 0;
    int bytes = 0;

    while (bytes < len && bytes < 5) {
        uint8_t byte = data[bytes++];
        result |= (uint32_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            *out_value = result;
            return bytes;
        }
        shift += 7;
    }
    return 0;  /* Error: incomplete or overflow */
}

int pb_varint_size(uint32_t value)
{
    /* Use uint32_t casts to avoid shift overflow on 8/16-bit platforms */
    if (value < ((uint32_t)1 << 7)) return 1;
    if (value < ((uint32_t)1 << 14)) return 2;
    if (value < ((uint32_t)1 << 21)) return 3;
    if (value < ((uint32_t)1 << 28)) return 4;
    return 5;
}

/*============================================================================
 * Little-Endian Byte Ordering
 *
 * All multi-byte fields are stored in little-endian format for cross-platform
 * compatibility. These helpers ensure replays work on big-endian systems.
 *============================================================================*/

static void write_le16(uint8_t* buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static void write_le32(uint8_t* buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static void write_le64(uint8_t* buf, uint64_t val)
{
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
}

static uint16_t read_le16(const uint8_t* buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t read_le32(const uint8_t* buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static uint64_t read_le64(const uint8_t* buf)
{
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (uint64_t)buf[i] << (i * 8);
    }
    return val;
}

/*============================================================================
 * Event Type Mapping
 *============================================================================*/

pb_input_event_type pb_event_to_input_type(pb_event_type type)
{
    switch (type) {
        case PB_EVENT_ROTATE_LEFT:   return PB_INPUT_ROTATE_LEFT;
        case PB_EVENT_ROTATE_RIGHT:  return PB_INPUT_ROTATE_RIGHT;
        case PB_EVENT_FIRE:          return PB_INPUT_FIRE;
        case PB_EVENT_SWITCH_BUBBLE: return PB_INPUT_SWITCH;
        case PB_EVENT_PAUSE:         return PB_INPUT_PAUSE;
        case PB_EVENT_UNPAUSE:       return PB_INPUT_UNPAUSE;
        default:                     return PB_INPUT_NONE;
    }
}

pb_event_type pb_input_to_event_type(pb_input_event_type type)
{
    switch (type) {
        case PB_INPUT_ROTATE_LEFT:  return PB_EVENT_ROTATE_LEFT;
        case PB_INPUT_ROTATE_RIGHT: return PB_EVENT_ROTATE_RIGHT;
        case PB_INPUT_FIRE:         return PB_EVENT_FIRE;
        case PB_INPUT_SWITCH:       return PB_EVENT_SWITCH_BUBBLE;
        case PB_INPUT_PAUSE:        return PB_EVENT_PAUSE;
        case PB_INPUT_UNPAUSE:      return PB_EVENT_UNPAUSE;
        default:                    return PB_EVENT_NONE;
    }
}

/*============================================================================
 * Event Packing
 *
 * Header byte: [type:4][reserved:2][payload_size:2]
 * Payload size: 0=none, 1=2bytes(int16), 2=4bytes(int32/float), 3=reserved
 *============================================================================*/

/* Header byte construction */
#define PACK_HEADER(type, payload_size) \
    (((uint8_t)(type) << 4) | ((payload_size) & 0x03))

#define UNPACK_TYPE(header) \
    ((pb_input_event_type)((header) >> 4))

#define UNPACK_PAYLOAD_SIZE(header) \
    ((header) & 0x03)

int pb_event_pack(const pb_input_event* event, uint32_t prev_frame,
                  uint8_t* out, bool use_fixed_point)
{
    int written = 0;
    uint32_t delta = event->frame - prev_frame;
    int payload_size = 0;

    /* Determine payload size */
    if (event->type == PB_INPUT_FIRE) {
        /* Both fixed-point (int32) and float use 4 bytes = payload code 2 */
        payload_size = 2;
    }

    /* Write header */
    out[written++] = PACK_HEADER(event->type, payload_size);

    /* Write frame delta as varint */
    written += pb_varint_encode(delta, out + written);

    /* Write payload */
    if (event->type == PB_INPUT_FIRE) {
        if (use_fixed_point) {
            /* Store as Q16.16 fixed-point in little-endian */
#if PB_USE_FIXED_POINT
            /* Input already in fixed-point format */
            int32_t angle_fixed = event->angle;
#else
            /* Convert float input to Q16.16 for storage */
            int32_t angle_fixed = (int32_t)(event->angle * 65536.0f);
#endif
            write_le32(out + written, (uint32_t)angle_fixed);
            written += 4;
        } else {
            /* Store as IEEE 754 float in little-endian */
#if PB_USE_FIXED_POINT
            /* Convert fixed-point input to float for storage */
            float angle_f = (float)event->angle / 65536.0f;
#else
            /* Input already in float format */
            float angle_f = event->angle;
#endif
            uint32_t bits;
            memcpy(&bits, &angle_f, sizeof(bits));
            write_le32(out + written, bits);
            written += 4;
        }
    }

    return written;
}

int pb_event_unpack(const uint8_t* data, int len, uint32_t prev_frame,
                    pb_input_event* event, bool use_fixed_point)
{
    if (len < 2) return 0;

    int consumed = 0;

    /* Read header */
    uint8_t header = data[consumed++];
    event->type = UNPACK_TYPE(header);
    (void)UNPACK_PAYLOAD_SIZE(header);  /* Reserved for future format extensions */

    /* Read frame delta */
    uint32_t delta;
    int varint_bytes = pb_varint_decode(data + consumed, len - consumed, &delta);
    if (varint_bytes == 0) return 0;
    consumed += varint_bytes;
    event->frame = prev_frame + delta;

    /* Read payload */
    event->angle = 0;
    if (event->type == PB_INPUT_FIRE) {
        if (len - consumed < 4) return 0;

        uint32_t raw_bits = read_le32(data + consumed);
        consumed += 4;

        if (use_fixed_point) {
            /* Data is stored as Q16.16 fixed-point */
            int32_t angle_fixed = (int32_t)raw_bits;
#if PB_USE_FIXED_POINT
            /* Output is fixed-point, use directly */
            event->angle = angle_fixed;
#else
            /* Output is float, convert from Q16.16 */
            event->angle = (float)angle_fixed / 65536.0f;
#endif
        } else {
            /* Data is stored as IEEE 754 float */
            float angle_f;
            memcpy(&angle_f, &raw_bits, sizeof(angle_f));
#if PB_USE_FIXED_POINT
            /* Output is fixed-point, convert from float */
            event->angle = (int32_t)(angle_f * 65536.0f);
#else
            /* Output is float, use directly */
            event->angle = angle_f;
#endif
        }
    }

    return consumed;
}

/*============================================================================
 * Replay Lifecycle
 *============================================================================*/

void pb_replay_init(pb_replay* replay, uint64_t seed,
                    const char* level_id, const char* ruleset_id)
{
    memset(replay, 0, sizeof(*replay));

    replay->header.magic = PB_REPLAY_MAGIC;
    replay->header.version = PB_REPLAY_VERSION;
    replay->header.seed = seed;

#if PB_USE_FIXED_POINT
    replay->header.flags |= PB_REPLAY_FLAG_FIXED_POINT;
#endif

    if (level_id) {
        strncpy(replay->header.level_id, level_id, sizeof(replay->header.level_id) - 1);
    }
    if (ruleset_id) {
        strncpy(replay->header.ruleset_id, ruleset_id, sizeof(replay->header.ruleset_id) - 1);
    }

    /* Initial allocation */
    replay->event_capacity = 256;
    replay->events = calloc((size_t)replay->event_capacity, sizeof(pb_input_event));

    replay->checkpoint_capacity = 16;
    replay->checkpoints = calloc((size_t)replay->checkpoint_capacity, sizeof(pb_checkpoint));
}

void pb_replay_free(pb_replay* replay)
{
    if (replay->events) {
        free(replay->events);
        replay->events = NULL;
    }
    if (replay->checkpoints) {
        free(replay->checkpoints);
        replay->checkpoints = NULL;
    }
    replay->event_count = 0;
    replay->event_capacity = 0;
    replay->checkpoint_count = 0;
    replay->checkpoint_capacity = 0;
}

void pb_replay_clear(pb_replay* replay)
{
    replay->event_count = 0;
    replay->checkpoint_count = 0;
    replay->last_event_frame = 0;
    replay->header.event_count = 0;
    replay->header.checkpoint_count = 0;
    replay->header.duration_frames = 0;
    replay->header.final_score = 0;
    replay->header.outcome = PB_OUTCOME_INCOMPLETE;
}

/*============================================================================
 * Recording
 *============================================================================*/

static pb_result grow_events(pb_replay* replay)
{
    uint32_t new_capacity = replay->event_capacity * 2;
    if (new_capacity > PB_REPLAY_MAX_EVENTS) {
        new_capacity = PB_REPLAY_MAX_EVENTS;
    }
    if (replay->event_count >= new_capacity) {
        return PB_ERR_NO_MEMORY;
    }

    pb_input_event* new_events = realloc(replay->events,
                                         (size_t)new_capacity * sizeof(pb_input_event));
    if (!new_events) {
        return PB_ERR_NO_MEMORY;
    }

    replay->events = new_events;
    replay->event_capacity = new_capacity;
    return PB_OK;
}

static pb_result grow_checkpoints(pb_replay* replay)
{
    uint32_t new_capacity = replay->checkpoint_capacity * 2;
    if (new_capacity > PB_REPLAY_MAX_CHECKPOINTS) {
        new_capacity = PB_REPLAY_MAX_CHECKPOINTS;
    }
    if (replay->checkpoint_count >= new_capacity) {
        return PB_ERR_NO_MEMORY;
    }

    pb_checkpoint* new_checkpoints = realloc(replay->checkpoints,
                                             (size_t)new_capacity * sizeof(pb_checkpoint));
    if (!new_checkpoints) {
        return PB_ERR_NO_MEMORY;
    }

    replay->checkpoints = new_checkpoints;
    replay->checkpoint_capacity = new_capacity;
    return PB_OK;
}

pb_result pb_replay_record_event(pb_replay* replay, const pb_input_event* event)
{
    if (!replay || !event) {
        return PB_ERR_INVALID_ARG;
    }

    if (event->type == PB_INPUT_NONE) {
        return PB_OK;  /* Ignore no-op events */
    }

    if (replay->event_count >= replay->event_capacity) {
        pb_result result = grow_events(replay);
        if (result != PB_OK) return result;
    }

    replay->events[replay->event_count++] = *event;
    replay->last_event_frame = event->frame;
    replay->header.event_count = replay->event_count;

    return PB_OK;
}

pb_result pb_replay_record(pb_replay* replay, const pb_event* event)
{
    if (!event) return PB_ERR_INVALID_ARG;

    pb_input_event_type input_type = pb_event_to_input_type(event->type);
    if (input_type == PB_INPUT_NONE) {
        return PB_OK;  /* Not an input event, ignore */
    }

    pb_input_event input = {
        .type = input_type,
        .frame = event->frame,
        .angle = (event->type == PB_EVENT_FIRE) ? event->data.fire.angle : 0
    };

    return pb_replay_record_event(replay, &input);
}

pb_result pb_replay_add_checkpoint(pb_replay* replay, uint32_t frame,
                                   uint32_t state_checksum, uint32_t board_checksum,
                                   const pb_rng* rng, uint32_t score, int shots_fired)
{
    if (!replay || !rng) {
        return PB_ERR_INVALID_ARG;
    }

    if (replay->checkpoint_count >= replay->checkpoint_capacity) {
        pb_result result = grow_checkpoints(replay);
        if (result != PB_OK) return result;
    }

    pb_checkpoint* cp = &replay->checkpoints[replay->checkpoint_count++];
    cp->frame = frame;
    cp->event_index = replay->event_count;
    cp->state_checksum = state_checksum;
    cp->board_checksum = board_checksum;
    memcpy(cp->rng_state, rng->state, sizeof(cp->rng_state));
    cp->score = score;
    cp->shots_fired = shots_fired;

    replay->header.checkpoint_count = replay->checkpoint_count;

    return PB_OK;
}

void pb_replay_finalize(pb_replay* replay, uint32_t duration_frames,
                        uint32_t final_score, pb_outcome outcome)
{
    if (!replay) return;

    replay->header.duration_frames = duration_frames;
    replay->header.final_score = final_score;
    replay->header.outcome = (uint8_t)outcome;
}

/*============================================================================
 * Serialization
 *
 * Binary format uses little-endian byte order for all multi-byte fields.
 * This ensures replay files are portable across architectures.
 *============================================================================*/

/* Serialized sizes (explicit layout, not sizeof) */
#define PB_HEADER_SERIALIZED_SIZE 164
#define PB_CHECKPOINT_SERIALIZED_SIZE 40

static size_t serialize_header(const pb_replay_header* h, uint8_t* buf)
{
    size_t off = 0;
    write_le32(buf + off, h->magic); off += 4;
    buf[off++] = h->version;
    buf[off++] = h->flags;
    write_le16(buf + off, h->reserved); off += 2;
    write_le64(buf + off, h->seed); off += 8;
    memcpy(buf + off, h->level_id, 64); off += 64;
    memcpy(buf + off, h->ruleset_id, 64); off += 64;
    write_le32(buf + off, h->event_count); off += 4;
    write_le32(buf + off, h->checkpoint_count); off += 4;
    write_le32(buf + off, h->duration_frames); off += 4;
    write_le32(buf + off, h->final_score); off += 4;
    buf[off++] = h->outcome;
    buf[off++] = h->reserved2[0];
    buf[off++] = h->reserved2[1];
    buf[off++] = h->reserved2[2];
    return off;  /* Should be PB_HEADER_SERIALIZED_SIZE */
}

static size_t deserialize_header(const uint8_t* buf, pb_replay_header* h)
{
    size_t off = 0;
    h->magic = read_le32(buf + off); off += 4;
    h->version = buf[off++];
    h->flags = buf[off++];
    h->reserved = read_le16(buf + off); off += 2;
    h->seed = read_le64(buf + off); off += 8;
    memcpy(h->level_id, buf + off, 64); off += 64;
    memcpy(h->ruleset_id, buf + off, 64); off += 64;
    h->event_count = read_le32(buf + off); off += 4;
    h->checkpoint_count = read_le32(buf + off); off += 4;
    h->duration_frames = read_le32(buf + off); off += 4;
    h->final_score = read_le32(buf + off); off += 4;
    h->outcome = buf[off++];
    h->reserved2[0] = buf[off++];
    h->reserved2[1] = buf[off++];
    h->reserved2[2] = buf[off++];
    return off;  /* Should be PB_HEADER_SERIALIZED_SIZE */
}

static size_t serialize_checkpoint(const pb_checkpoint* cp, uint8_t* buf)
{
    size_t off = 0;
    write_le32(buf + off, cp->frame); off += 4;
    write_le32(buf + off, cp->event_index); off += 4;
    write_le32(buf + off, cp->state_checksum); off += 4;
    write_le32(buf + off, cp->board_checksum); off += 4;
    for (int i = 0; i < 4; i++) {
        write_le32(buf + off, cp->rng_state[i]); off += 4;
    }
    write_le32(buf + off, cp->score); off += 4;
    write_le32(buf + off, (uint32_t)cp->shots_fired); off += 4;
    return off;  /* Should be PB_CHECKPOINT_SERIALIZED_SIZE */
}

static size_t deserialize_checkpoint(const uint8_t* buf, pb_checkpoint* cp)
{
    size_t off = 0;
    cp->frame = read_le32(buf + off); off += 4;
    cp->event_index = read_le32(buf + off); off += 4;
    cp->state_checksum = read_le32(buf + off); off += 4;
    cp->board_checksum = read_le32(buf + off); off += 4;
    for (int i = 0; i < 4; i++) {
        cp->rng_state[i] = read_le32(buf + off); off += 4;
    }
    cp->score = read_le32(buf + off); off += 4;
    cp->shots_fired = (int32_t)read_le32(buf + off); off += 4;
    return off;  /* Should be PB_CHECKPOINT_SERIALIZED_SIZE */
}

size_t pb_replay_serialized_size(const pb_replay* replay)
{
    if (!replay) return 0;

    size_t size = PB_HEADER_SERIALIZED_SIZE;
    size += replay->checkpoint_count * PB_CHECKPOINT_SERIALIZED_SIZE;

    /* Max event size: 1 byte header + 5 byte varint + 4 byte payload = 10 */
    /* Use 10 to ensure buffer is always large enough */
    size += replay->event_count * 10;

    return size;
}

size_t pb_replay_serialize(const pb_replay* replay, uint8_t* buffer, size_t buffer_size)
{
    if (!replay || !buffer) return 0;

    size_t offset = 0;
    bool use_fixed_point = (replay->header.flags & PB_REPLAY_FLAG_FIXED_POINT) != 0;

    /* Header (endian-safe) */
    if (offset + PB_HEADER_SERIALIZED_SIZE > buffer_size) return 0;
    offset += serialize_header(&replay->header, buffer + offset);

    /* Checkpoints (endian-safe) */
    size_t checkpoint_size = (size_t)replay->checkpoint_count * PB_CHECKPOINT_SERIALIZED_SIZE;
    if (offset + checkpoint_size > buffer_size) return 0;
    for (uint32_t i = 0; i < replay->checkpoint_count; i++) {
        offset += serialize_checkpoint(&replay->checkpoints[i], buffer + offset);
    }

    /* Events (packed) */
    uint32_t prev_frame = 0;
    for (uint32_t i = 0; i < replay->event_count; i++) {
        if (offset + 9 > buffer_size) return 0;  /* Max event size */

        int written = pb_event_pack(&replay->events[i], prev_frame,
                                    buffer + offset, use_fixed_point);
        if (written == 0) return 0;

        prev_frame = replay->events[i].frame;
        offset += (size_t)written;
    }

    return offset;
}

pb_result pb_replay_deserialize(const uint8_t* buffer, size_t buffer_size,
                                pb_replay* replay)
{
    if (!buffer || !replay || buffer_size < PB_HEADER_SERIALIZED_SIZE) {
        return PB_ERR_INVALID_ARG;
    }

    memset(replay, 0, sizeof(*replay));
    size_t offset = 0;

    /* Header (endian-safe) */
    offset += deserialize_header(buffer, &replay->header);

    if (replay->header.magic != PB_REPLAY_MAGIC) {
        return PB_ERR_INVALID_ARG;
    }

    if (replay->header.version > PB_REPLAY_VERSION) {
        return PB_ERR_NOT_IMPLEMENTED;
    }

    bool use_fixed_point = (replay->header.flags & PB_REPLAY_FLAG_FIXED_POINT) != 0;

    /* Checkpoints (endian-safe) */
    size_t checkpoint_size = replay->header.checkpoint_count * PB_CHECKPOINT_SERIALIZED_SIZE;
    if (offset + checkpoint_size > buffer_size) {
        return PB_ERR_INVALID_ARG;
    }

    replay->checkpoint_capacity = replay->header.checkpoint_count;
    replay->checkpoint_count = replay->header.checkpoint_count;
    if (replay->checkpoint_count > 0) {
        replay->checkpoints = calloc(replay->checkpoint_count, sizeof(pb_checkpoint));
        if (!replay->checkpoints) return PB_ERR_NO_MEMORY;
        for (uint32_t i = 0; i < replay->header.checkpoint_count; i++) {
            offset += deserialize_checkpoint(buffer + offset, &replay->checkpoints[i]);
        }
    }

    /* Events */
    replay->event_capacity = replay->header.event_count;
    replay->event_count = 0;
    if (replay->event_capacity > 0) {
        replay->events = calloc(replay->event_capacity, sizeof(pb_input_event));
        if (!replay->events) {
            pb_replay_free(replay);
            return PB_ERR_NO_MEMORY;
        }
    }

    uint32_t prev_frame = 0;
    for (uint32_t i = 0; i < replay->header.event_count; i++) {
        pb_input_event event;
        int consumed = pb_event_unpack(buffer + offset, (int)(buffer_size - offset),
                                       prev_frame, &event, use_fixed_point);
        if (consumed == 0) {
            pb_replay_free(replay);
            return PB_ERR_INVALID_ARG;
        }

        replay->events[replay->event_count++] = event;
        prev_frame = event.frame;
        offset += (size_t)consumed;
    }

    return PB_OK;
}

pb_result pb_replay_save(const pb_replay* replay, const char* path)
{
    if (!replay || !path) return PB_ERR_INVALID_ARG;

    size_t size = pb_replay_serialized_size(replay);
    uint8_t* buffer = malloc(size);
    if (!buffer) return PB_ERR_NO_MEMORY;

    size_t written = pb_replay_serialize(replay, buffer, size);
    if (written == 0) {
        free(buffer);
        return PB_ERR_INVALID_ARG;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        free(buffer);
        return PB_ERR_INVALID_ARG;
    }

    size_t file_written = fwrite(buffer, 1, written, f);
    fclose(f);
    free(buffer);

    return (file_written == written) ? PB_OK : PB_ERR_INVALID_ARG;
}

pb_result pb_replay_load(const char* path, pb_replay* replay)
{
    if (!path || !replay) return PB_ERR_INVALID_ARG;

    FILE* f = fopen(path, "rb");
    if (!f) return PB_ERR_INVALID_ARG;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > (long)(PB_REPLAY_MAX_EVENTS * 9 + 65536)) {
        fclose(f);
        return PB_ERR_INVALID_ARG;
    }

    uint8_t* buffer = malloc((size_t)size);
    if (!buffer) {
        fclose(f);
        return PB_ERR_NO_MEMORY;
    }

    size_t read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(buffer);
        return PB_ERR_INVALID_ARG;
    }

    pb_result result = pb_replay_deserialize(buffer, (size_t)size, replay);
    free(buffer);

    return result;
}

/*============================================================================
 * Playback
 *============================================================================*/

void pb_playback_init(pb_playback* playback, const pb_replay* replay)
{
    if (!playback || !replay) return;

    memset(playback, 0, sizeof(*playback));
    playback->replay = replay;
    playback->speed_multiplier = 100;  /* 1x */
}

bool pb_playback_get_event(pb_playback* playback, pb_input_event* event)
{
    if (!playback || !playback->replay || !event) return false;
    if (playback->finished || playback->paused) return false;

    /* No more events to return (but playback may continue) */
    if (playback->event_cursor >= playback->replay->event_count) {
        return false;
    }

    const pb_input_event* next = &playback->replay->events[playback->event_cursor];
    if (next->frame <= playback->current_frame) {
        *event = *next;
        playback->event_cursor++;
        return true;
    }

    return false;
}

void pb_playback_advance(pb_playback* playback)
{
    if (!playback || playback->paused) return;
    playback->current_frame++;

    /* Update checkpoint cursor (avoid unsigned underflow when count==0) */
    const pb_replay* r = playback->replay;
    while (playback->checkpoint_cursor + 1 < r->checkpoint_count &&
           r->checkpoints[playback->checkpoint_cursor + 1].frame <= playback->current_frame) {
        playback->checkpoint_cursor++;
    }
}

uint32_t pb_playback_seek(pb_playback* playback, uint32_t target_frame,
                          pb_checkpoint* checkpoint)
{
    if (!playback || !playback->replay) return 0;

    const pb_replay* r = playback->replay;

    /* Find best checkpoint <= target_frame */
    uint32_t best_cp = 0;
    bool found = false;
    for (uint32_t i = 0; i < r->checkpoint_count; i++) {
        if (r->checkpoints[i].frame <= target_frame) {
            best_cp = i;
            found = true;
        } else {
            break;
        }
    }

    if (found) {
        if (checkpoint) {
            *checkpoint = r->checkpoints[best_cp];
        }
        playback->checkpoint_cursor = best_cp;
        playback->current_frame = r->checkpoints[best_cp].frame;
        playback->event_cursor = r->checkpoints[best_cp].event_index;
        playback->finished = false;
        return r->checkpoints[best_cp].frame;
    }

    /* No checkpoint, start from beginning */
    playback->current_frame = 0;
    playback->event_cursor = 0;
    playback->checkpoint_cursor = 0;
    playback->finished = false;
    return 0;
}

void pb_playback_set_speed(pb_playback* playback, int speed_percent)
{
    if (!playback) return;
    playback->speed_multiplier = speed_percent;
    playback->paused = (speed_percent == 0);
}

/*============================================================================
 * Verification
 *============================================================================*/

bool pb_checkpoint_verify(const pb_checkpoint* checkpoint,
                          uint32_t state_checksum, uint32_t board_checksum)
{
    if (!checkpoint) return false;
    return checkpoint->state_checksum == state_checksum &&
           checkpoint->board_checksum == board_checksum;
}

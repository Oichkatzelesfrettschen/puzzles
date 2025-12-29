/**
 * @file pb_checksum.c
 * @brief CRC-32 checksumming implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_checksum.h"
#include "pb/pb_board.h"


/*============================================================================
 * CRC-32 Table (ISO-HDLC polynomial 0xEDB88320)
 *============================================================================*/

static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

void pb_crc32_init(void)
{
    if (crc32_table_initialized) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }

    crc32_table_initialized = true;
}

uint32_t pb_crc32_update(uint32_t crc, const void* data, size_t len)
{
    if (!crc32_table_initialized) {
        pb_crc32_init();
    }

    const uint8_t* buf = (const uint8_t*)data;
    crc ^= 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ buf[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF;
}

uint32_t pb_crc32(const void* data, size_t len)
{
    return pb_crc32_update(0, data, len);
}

uint32_t pb_crc32_finalize(uint32_t crc)
{
    /* For streaming use: caller accumulates with XOR'd value,
       this finalizes. Since update already XORs, this is identity. */
    return crc;
}

/*============================================================================
 * Helper: Feed values in consistent byte order
 *============================================================================*/

static uint32_t crc_feed_u8(uint32_t crc, uint8_t val)
{
    return pb_crc32_update(crc, &val, 1);
}

static uint32_t crc_feed_u32(uint32_t crc, uint32_t val)
{
    /* Little-endian byte order for consistency */
    uint8_t bytes[4] = {
        (uint8_t)(val & 0xFF),
        (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF),
        (uint8_t)((val >> 24) & 0xFF)
    };
    return pb_crc32_update(crc, bytes, 4);
}

static uint32_t crc_feed_i32(uint32_t crc, int32_t val)
{
    return crc_feed_u32(crc, (uint32_t)val);
}

static uint32_t crc_feed_scalar(uint32_t crc, pb_scalar val)
{
#if PB_USE_FIXED_POINT
    return crc_feed_i32(crc, val);
#else
    /* IEEE 754 float as raw bytes */
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    return crc_feed_u32(crc, bits);
#endif
}

/*============================================================================
 * Game State Checksums
 *============================================================================*/

uint32_t pb_bubble_checksum(const pb_bubble* bubble)
{
    if (!bubble) return 0;

    uint32_t crc = 0;
    crc = crc_feed_u8(crc, (uint8_t)bubble->kind);
    crc = crc_feed_u8(crc, bubble->color_id);
    crc = crc_feed_u8(crc, bubble->flags);
    crc = crc_feed_u8(crc, (uint8_t)bubble->special);
    crc = crc_feed_u8(crc, bubble->payload.timer);

    return crc;
}

/* pb_board_checksum is in pb_board.c */

uint32_t pb_rng_state_checksum(const pb_rng* rng)
{
    if (!rng) return 0;

    uint32_t crc = 0;
    for (int i = 0; i < 4; i++) {
        crc = crc_feed_u32(crc, rng->state[i]);
    }
    return crc;
}

uint32_t pb_state_checksum(const pb_game_state* state)
{
    if (!state) return 0;

    uint32_t crc = 0;

    /* Phase and frame */
    crc = crc_feed_u8(crc, (uint8_t)state->phase);
    crc = crc_feed_u32(crc, state->frame);

    /* Board (full checksum) */
    uint32_t board_crc = pb_board_checksum(&state->board);
    crc = crc_feed_u32(crc, board_crc);

    /* RNG state */
    uint32_t rng_crc = pb_rng_state_checksum(&state->rng);
    crc = crc_feed_u32(crc, rng_crc);

    /* Score and shots */
    crc = crc_feed_u32(crc, state->score);
    crc = crc_feed_i32(crc, state->shots_fired);
    crc = crc_feed_i32(crc, state->shots_until_row);
    crc = crc_feed_i32(crc, state->combo_multiplier);

    /* Cannon angle */
    crc = crc_feed_scalar(crc, state->cannon_angle);

    /* Current and preview bubbles */
    uint32_t cur_crc = pb_bubble_checksum(&state->current_bubble);
    uint32_t prev_crc = pb_bubble_checksum(&state->preview_bubble);
    crc = crc_feed_u32(crc, cur_crc);
    crc = crc_feed_u32(crc, prev_crc);

    /* Shot state (if in motion) */
    if (state->shot.phase != PB_SHOT_IDLE) {
        crc = crc_feed_u8(crc, (uint8_t)state->shot.phase);
        crc = crc_feed_scalar(crc, state->shot.pos.x);
        crc = crc_feed_scalar(crc, state->shot.pos.y);
        crc = crc_feed_scalar(crc, state->shot.velocity.x);
        crc = crc_feed_scalar(crc, state->shot.velocity.y);
        crc = crc_feed_i32(crc, state->shot.bounces);
    }

    return crc;
}

uint32_t pb_frame_checksum(const pb_game_state* state)
{
    if (!state) return 0;

    /* Lightweight checksum for per-frame tracking */
    uint32_t board_crc = pb_board_checksum(&state->board);
    uint32_t rng_crc = pb_rng_state_checksum(&state->rng);

    uint32_t crc = 0;
    crc = crc_feed_u32(crc, state->frame);
    crc = crc_feed_u32(crc, board_crc ^ rng_crc);
    crc = crc_feed_u32(crc, state->score);

    return crc;
}

/*============================================================================
 * State Comparison
 *============================================================================*/

bool pb_state_compare(const pb_game_state* expected,
                      const pb_game_state* actual,
                      pb_desync_info* info)
{
    if (!expected || !actual) {
        if (info) {
            info->detected = true;
            info->component = "null pointer";
        }
        return false;
    }

    if (info) {
        memset(info, 0, sizeof(*info));
    }

    /* Check components in order of likely divergence */

    /* RNG (most likely to diverge due to call ordering) */
    uint32_t exp_rng = pb_rng_state_checksum(&expected->rng);
    uint32_t act_rng = pb_rng_state_checksum(&actual->rng);
    if (exp_rng != act_rng) {
        if (info) {
            info->detected = true;
            info->frame = actual->frame;
            info->expected = exp_rng;
            info->actual = act_rng;
            info->component = "rng";
        }
        return false;
    }

    /* Board state */
    uint32_t exp_board = pb_board_checksum(&expected->board);
    uint32_t act_board = pb_board_checksum(&actual->board);
    if (exp_board != act_board) {
        if (info) {
            info->detected = true;
            info->frame = actual->frame;
            info->expected = exp_board;
            info->actual = act_board;
            info->component = "board";
        }
        return false;
    }

    /* Score */
    if (expected->score != actual->score) {
        if (info) {
            info->detected = true;
            info->frame = actual->frame;
            info->expected = expected->score;
            info->actual = actual->score;
            info->component = "score";
        }
        return false;
    }

    /* Phase */
    if (expected->phase != actual->phase) {
        if (info) {
            info->detected = true;
            info->frame = actual->frame;
            info->expected = (uint32_t)expected->phase;
            info->actual = (uint32_t)actual->phase;
            info->component = "phase";
        }
        return false;
    }

    /* Frame number */
    if (expected->frame != actual->frame) {
        if (info) {
            info->detected = true;
            info->frame = actual->frame;
            info->expected = expected->frame;
            info->actual = actual->frame;
            info->component = "frame";
        }
        return false;
    }

    return true;
}

/*============================================================================
 * Checksum Buffer
 *============================================================================*/

void pb_checksum_buffer_init(pb_checksum_buffer* buf)
{
    if (!buf) return;
    memset(buf, 0, sizeof(*buf));
}

void pb_checksum_buffer_record(pb_checksum_buffer* buf,
                               uint32_t frame, uint32_t checksum)
{
    if (!buf) return;

    buf->checksums[buf->head] = checksum;
    buf->frames[buf->head] = frame;

    buf->head = (buf->head + 1) % PB_CHECKSUM_BUFFER_SIZE;
    if (buf->count < PB_CHECKSUM_BUFFER_SIZE) {
        buf->count++;
    }
}

bool pb_checksum_buffer_find(const pb_checksum_buffer* buf,
                             uint32_t frame, uint32_t* checksum)
{
    if (!buf || buf->count == 0) return false;

    /* Search backwards from most recent */
    for (int i = 0; i < buf->count; i++) {
        int idx = (buf->head - 1 - i + PB_CHECKSUM_BUFFER_SIZE) % PB_CHECKSUM_BUFFER_SIZE;
        if (buf->frames[idx] == frame) {
            if (checksum) *checksum = buf->checksums[idx];
            return true;
        }
    }

    return false;
}

bool pb_checksum_buffer_verify(const pb_checksum_buffer* buf,
                               uint32_t frame, uint32_t expected)
{
    uint32_t actual;
    if (!pb_checksum_buffer_find(buf, frame, &actual)) {
        return false;  /* Frame not in buffer */
    }
    return actual == expected;
}

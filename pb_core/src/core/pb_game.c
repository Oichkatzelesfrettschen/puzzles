/*
 * pb_game.c - Game state controller implementation
 *
 * Orchestrates the main game loop: shot physics, collision detection,
 * match resolution, orphan dropping, and win/lose conditions.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_game.h"
#include "pb/pb_rng.h"
#include "pb/pb_hex.h"

#include "pb/pb_freestanding.h"

/*============================================================================
 * Cell Index Conversion Helpers
 *============================================================================*/

/**
 * Convert pb_offset array to pb_cell_index array.
 * Used for compact event storage.
 */
static void offsets_to_indices(const pb_offset* offsets, pb_cell_index* indices, int count)
{
    int limit = (count > PB_EVENT_CELL_MAX) ? PB_EVENT_CELL_MAX : count;
    for (int i = 0; i < limit; i++) {
        indices[i] = PB_CELL_TO_INDEX(offsets[i].row, offsets[i].col);
    }
}

/*============================================================================
 * Default Rulesets
 *============================================================================*/

static const pb_ruleset ruleset_defaults[PB_MODE_COUNT] = {
    [PB_MODE_PUZZLE] = {
        .mode = PB_MODE_PUZZLE,
        .match_threshold = 3,
        .cols_even = 8, .cols_odd = 7, .rows = 12,
        .max_bounces = 2,
        .shots_per_row_insert = 0,  /* No pressure */
        .initial_rows = 5,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_OVERFLOW,
        .allow_color_switch = false,
        .restrict_colors_to_board = true,
        .allowed_colors = 0xFF,
        .allowed_specials = 0
    },
    [PB_MODE_ARCADE] = {
        .mode = PB_MODE_ARCADE,
        .match_threshold = 3,
        .cols_even = 8, .cols_odd = 7, .rows = 12,
        .max_bounces = 2,
        .shots_per_row_insert = 8,  /* New row every 8 shots */
        .initial_rows = 4,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_OVERFLOW,
        .allow_color_switch = true,
        .restrict_colors_to_board = true,
        .allowed_colors = 0xFF,
        .allowed_specials = 0x07  /* Bomb, Lightning, Star */
    },
    [PB_MODE_SURVIVAL] = {
        .mode = PB_MODE_SURVIVAL,
        .match_threshold = 3,
        .cols_even = 8, .cols_odd = 7, .rows = 14,
        .max_bounces = 2,
        .shots_per_row_insert = 5,
        .initial_rows = 3,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_OVERFLOW,
        .allow_color_switch = true,
        .restrict_colors_to_board = false,
        .allowed_colors = 0x3F,  /* 6 colors */
        .allowed_specials = 0x0F
    },
    [PB_MODE_TIME_ATTACK] = {
        .mode = PB_MODE_TIME_ATTACK,
        .match_threshold = 3,
        .cols_even = 8, .cols_odd = 7, .rows = 12,
        .max_bounces = 2,
        .shots_per_row_insert = 0,
        .initial_rows = 6,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_TIMEOUT,
        .allow_color_switch = true,
        .restrict_colors_to_board = true,
        .allowed_colors = 0xFF,
        .allowed_specials = 0x1F
    },
    [PB_MODE_VERSUS] = {
        .mode = PB_MODE_VERSUS,
        .match_threshold = 3,
        .cols_even = 8, .cols_odd = 7, .rows = 12,
        .max_bounces = 2,
        .shots_per_row_insert = 10,
        .initial_rows = 4,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_OVERFLOW,
        .allow_color_switch = true,
        .restrict_colors_to_board = true,
        .allowed_colors = 0xFF,
        .allowed_specials = 0x03
    },
    [PB_MODE_COOP] = {
        .mode = PB_MODE_COOP,
        .match_threshold = 3,
        .cols_even = 10, .cols_odd = 9, .rows = 14,  /* Wider board */
        .max_bounces = 2,
        .shots_per_row_insert = 12,
        .initial_rows = 5,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_OVERFLOW,
        .allow_color_switch = true,
        .restrict_colors_to_board = true,
        .allowed_colors = 0xFF,
        .allowed_specials = 0x0F
    },
    [PB_MODE_ZEN] = {
        .mode = PB_MODE_ZEN,
        .match_threshold = 3,
        .cols_even = 8, .cols_odd = 7, .rows = 12,
        .max_bounces = 99,  /* Unlimited bounces */
        .shots_per_row_insert = 0,
        .initial_rows = 4,
        .bubble_radius = PB_INT_TO_FIXED(16),
        .lose_on = PB_LOSE_OVERFLOW,
        .allow_color_switch = true,
        .restrict_colors_to_board = false,
        .allowed_colors = 0xFF,
        .allowed_specials = 0xFF
    }
};

void pb_ruleset_default(pb_ruleset* ruleset, pb_mode_type mode)
{
    if (mode < PB_MODE_COUNT) {
        *ruleset = ruleset_defaults[mode];
    } else {
        *ruleset = ruleset_defaults[PB_MODE_PUZZLE];
    }
}

pb_result pb_ruleset_validate(const pb_ruleset* ruleset)
{
    if (ruleset->cols_even < 1 || ruleset->cols_even > PB_MAX_COLS) {
        return PB_ERR_INVALID_ARG;
    }
    if (ruleset->cols_odd < 1 || ruleset->cols_odd > PB_MAX_COLS) {
        return PB_ERR_INVALID_ARG;
    }
    if (ruleset->rows < 1 || ruleset->rows > PB_MAX_ROWS) {
        return PB_ERR_INVALID_ARG;
    }
    if (ruleset->match_threshold < 2) {
        return PB_ERR_INVALID_ARG;
    }
    return PB_OK;
}

/*============================================================================
 * Playfield Geometry
 *============================================================================*/

void pb_playfield_calc(pb_playfield* field, const pb_board* board,
                       pb_scalar bubble_radius)
{
    /* Calculate playfield from board dimensions */
    int max_cols = (board->cols_even > board->cols_odd) ?
                   board->cols_even : board->cols_odd;

    field->bubble_radius = bubble_radius;
    field->left_wall = 0;
    field->right_wall = PB_FIXED_MUL(PB_FIXED_MUL(bubble_radius, PB_INT_TO_FIXED(2 * max_cols)), PB_0_75)
                        + bubble_radius;
    field->ceiling = 0;
    field->floor = PB_FIXED_MUL(PB_FIXED_MUL(bubble_radius, PB_SQRT3), PB_INT_TO_FIXED(board->rows))
                   + PB_FIXED_MUL(bubble_radius, PB_INT_TO_FIXED(2));

    /* Cannon at bottom center */
    field->cannon_pos.x = field->right_wall / 2;
    field->cannon_pos.y = field->floor - bubble_radius;
}

/*============================================================================
 * Game Initialization
 *============================================================================*/

static pb_bubble generate_bubble(pb_game_state* state)
{
    pb_bubble bubble = {0};
    bubble.kind = PB_KIND_COLORED;

    uint8_t allowed = state->ruleset.allowed_colors;

    /* Restrict to colors on board if configured */
    if (state->ruleset.restrict_colors_to_board) {
        uint8_t board_colors = pb_board_color_mask(&state->board);
        if (board_colors != 0) {
            allowed &= board_colors;
        }
    }

    bubble.color_id = (uint8_t)pb_rng_pick_color(&state->rng, allowed);

    return bubble;
}

pb_result pb_game_init(pb_game_state* state, const pb_ruleset* ruleset,
                       uint64_t seed)
{
    memset(state, 0, sizeof(*state));

    /* Set ruleset */
    if (ruleset != NULL) {
        pb_result r = pb_ruleset_validate(ruleset);
        if (r != PB_OK) return r;
        state->ruleset = *ruleset;
    } else {
        pb_ruleset_default(&state->ruleset, PB_MODE_PUZZLE);
    }

    /* Initialize board */
    pb_board_init_custom(&state->board, state->ruleset.rows,
                         state->ruleset.cols_even, state->ruleset.cols_odd);

    /* Initialize RNG */
    pb_rng_seed(&state->rng, seed);

    /* Initialize shot */
    state->shot.phase = PB_SHOT_IDLE;
    state->shot.max_bounces = state->ruleset.max_bounces;

    /* Generate initial bubbles */
    state->current_bubble = generate_bubble(state);
    state->preview_bubble = generate_bubble(state);

    /* Initial cannon angle (straight up = π/2) */
    state->cannon_angle = PB_FLOAT_TO_FIXED(3.14159265f / 2.0f);

    /* Mode-specific initialization */
    state->shots_until_row = state->ruleset.shots_per_row_insert;

    /* Start in PLAYING phase - READY is a UI concept for countdown display */
    state->phase = PB_PHASE_PLAYING;

    return PB_OK;
}

void pb_game_reset(pb_game_state* state, uint64_t seed)
{
    pb_ruleset saved_ruleset = state->ruleset;
    pb_game_init(state, &saved_ruleset, seed);
}

pb_result pb_game_load_board(pb_game_state* state, const pb_bubble* bubbles,
                             int rows)
{
    if (rows > state->board.rows) {
        return PB_ERR_OUT_OF_BOUNDS;
    }

    pb_board_clear(&state->board);

    int idx = 0;
    for (int row = 0; row < rows; row++) {
        int cols = pb_row_cols(row, state->board.cols_even, state->board.cols_odd);
        for (int col = 0; col < cols; col++) {
            if (bubbles[idx].kind != PB_KIND_NONE) {
                pb_board_set(&state->board, (pb_offset){row, col}, bubbles[idx]);
            }
            idx++;
        }
    }

    return PB_OK;
}

/*============================================================================
 * Input Handling
 *============================================================================*/

void pb_game_set_angle(pb_game_state* state, pb_scalar angle)
{
    /* Clamp to valid range */
    if (angle < PB_MIN_ANGLE) angle = PB_MIN_ANGLE;
    if (angle > PB_MAX_ANGLE) angle = PB_MAX_ANGLE;
    state->cannon_angle = angle;
}

void pb_game_rotate(pb_game_state* state, pb_scalar delta)
{
    pb_game_set_angle(state, state->cannon_angle + delta);
}

pb_result pb_game_fire(pb_game_state* state)
{
    if (state->phase != PB_PHASE_READY && state->phase != PB_PHASE_PLAYING &&
        state->phase != PB_PHASE_HURRY) {
        return PB_ERR_INVALID_STATE;
    }

    if (state->shot.phase != PB_SHOT_IDLE) {
        return PB_ERR_INVALID_STATE;
    }

    /* Calculate cannon position */
    pb_playfield field;
    pb_playfield_calc(&field, &state->board, state->ruleset.bubble_radius);

    /* Initialize shot */
    pb_shot_init(&state->shot, state->current_bubble, field.cannon_pos,
                 state->cannon_angle, PB_DEFAULT_SHOT_SPEED);
    state->shot.max_bounces = state->ruleset.max_bounces;

    state->phase = PB_PHASE_PLAYING;
    state->shots_fired++;

    /* Track last shot frame for hurry-up system */
    state->last_shot_frame = state->frame;
    state->hurry_active = false;

    /* Log event */
    pb_event event = {
        .type = PB_EVENT_FIRE,
        .frame = state->frame,
        .data.fire.angle = state->cannon_angle
    };
    pb_game_add_event(state, &event);

    return PB_OK;
}

pb_result pb_game_swap_bubbles(pb_game_state* state)
{
    if (!state->ruleset.allow_color_switch) {
        return PB_ERR_INVALID_STATE;
    }

    if (state->shot.phase != PB_SHOT_IDLE) {
        return PB_ERR_INVALID_STATE;
    }

    pb_bubble temp = state->current_bubble;
    state->current_bubble = state->preview_bubble;
    state->preview_bubble = temp;

    pb_event event = {.type = PB_EVENT_SWITCH_BUBBLE, .frame = state->frame};
    pb_game_add_event(state, &event);

    return PB_OK;
}

void pb_game_pause(pb_game_state* state, bool paused)
{
    if (paused && state->phase == PB_PHASE_PLAYING) {
        state->phase = PB_PHASE_PAUSED;
        pb_event event = {.type = PB_EVENT_PAUSE, .frame = state->frame};
        pb_game_add_event(state, &event);
    } else if (!paused && state->phase == PB_PHASE_PAUSED) {
        state->phase = PB_PHASE_PLAYING;
        pb_event event = {.type = PB_EVENT_UNPAUSE, .frame = state->frame};
        pb_game_add_event(state, &event);
    }
}

/*============================================================================
 * Game Loop
 *============================================================================*/

int pb_game_tick(pb_game_state* state)
{
    int events_generated = 0;

    if (state->phase != PB_PHASE_PLAYING &&
        state->phase != PB_PHASE_ANIMATING &&
        state->phase != PB_PHASE_HURRY) {
        return 0;
    }

    state->frame++;

    /* Auto-fire / hurry-up system check (only when shot is idle) */
    if (state->shot.phase == PB_SHOT_IDLE) {
        uint32_t frames_since_shot = state->frame - state->last_shot_frame;

        /* Check for hurry warning */
        if (!state->hurry_active && frames_since_shot >= PB_HURRY_WARNING_FRAMES) {
            state->hurry_active = true;
            state->phase = PB_PHASE_HURRY;
        }

        /* Check for auto-fire trigger */
        if (frames_since_shot >= PB_HURRY_AUTOFIRE_FRAMES) {
            pb_game_fire(state);
            events_generated++;
        }
    }

    /* Process shot */
    if (state->shot.phase == PB_SHOT_MOVING) {
        pb_playfield field;
        pb_playfield_calc(&field, &state->board, state->ruleset.bubble_radius);

        pb_collision collision = pb_shot_step(&state->shot, &state->board,
                                              field.bubble_radius,
                                              field.left_wall, field.right_wall,
                                              field.ceiling, field.floor);

        if (collision.type == PB_COLLISION_BUBBLE ||
            collision.type == PB_COLLISION_CEILING) {
            /* Place bubble */
            pb_offset snap = pb_find_snap_cell(&state->board, collision.hit_point,
                                                field.bubble_radius);

            if (snap.row >= 0 && snap.col >= 0) {
                pb_board_set(&state->board, snap, state->shot.bubble);

                /* Log placement event */
                pb_event event = {
                    .type = PB_EVENT_BUBBLE_PLACED,
                    .frame = state->frame,
                    .data.placed = {PB_CELL_TO_INDEX(snap.row, snap.col), state->shot.bubble}
                };
                pb_game_add_event(state, &event);
                events_generated++;

                /* Process matches */
                int popped = pb_game_process_matches(state, snap);
                if (popped > 0) events_generated++;

                /* Process orphans */
                int dropped = pb_game_process_orphans(state);
                if (dropped > 0) events_generated++;

                /* Check win/lose */
                if (pb_board_is_clear(&state->board)) {
                    state->phase = PB_PHASE_WON;
                    pb_event win_event = {
                        .type = PB_EVENT_LEVEL_CLEAR,
                        .frame = state->frame
                    };
                    pb_game_add_event(state, &win_event);
                } else if (snap.row >= state->board.rows - 1) {
                    /* Bubble placed in danger zone */
                    state->phase = PB_PHASE_LOST;
                    pb_event lose_event = {
                        .type = PB_EVENT_GAME_OVER,
                        .frame = state->frame
                    };
                    pb_game_add_event(state, &lose_event);
                }

                /* Next bubble */
                pb_game_next_bubble(state);

                /* Row insertion for survival/arcade modes */
                if (state->ruleset.shots_per_row_insert > 0) {
                    state->shots_until_row--;
                    if (state->shots_until_row <= 0) {
                        /* Insert new row at top */
                        uint8_t colors = state->ruleset.restrict_colors_to_board
                                         ? pb_board_color_mask(&state->board)
                                         : state->ruleset.allowed_colors;
                        if (colors == 0) colors = state->ruleset.allowed_colors;

                        if (!pb_board_insert_row(&state->board, &state->rng, colors)) {
                            /* Row insertion caused overflow - game over */
                            state->phase = PB_PHASE_LOST;
                            pb_event lose_event = {
                                .type = PB_EVENT_GAME_OVER,
                                .frame = state->frame
                            };
                            pb_game_add_event(state, &lose_event);
                        } else {
                            /* Log row insert event */
                            pb_event row_event = {
                                .type = PB_EVENT_ROW_INSERTED,
                                .frame = state->frame
                            };
                            pb_game_add_event(state, &row_event);
                        }
                        state->shots_until_row = state->ruleset.shots_per_row_insert;
                    }
                }
            }

            state->shot.phase = PB_SHOT_IDLE;

        } else if (collision.type == PB_COLLISION_FLOOR) {
            /* Lose condition in some modes */
            state->phase = PB_PHASE_LOST;
            state->shot.phase = PB_SHOT_IDLE;
        }
    }

    return events_generated;
}

int pb_game_process_matches(pb_game_state* state, pb_offset placed_cell)
{
    pb_visit_result matches;
    int count = pb_find_matches(&state->board, placed_cell, &matches);

    if (count >= state->ruleset.match_threshold) {
        /* Pop matched bubbles */
        pb_board_remove_cells(&state->board, &matches);

        /* Enhanced scoring system (km-bubble-shooter quantifier style):
         * Score per bubble = BASE * (floor(quantifier / DIV) + 1)
         * Quantifier increments per bubble destroyed */
        int score = 0;
        for (int i = 0; i < count; i++) {
            int tier = (state->score_quantifier / PB_SCORE_QUANTIFIER_DIV) + 1;
            score += PB_SCORE_BASE_MATCH * tier;
            state->score_quantifier++;
        }

        /* Apply chain multiplier */
        state->combo_multiplier++;
        if (state->combo_multiplier > PB_MAX_CHAIN_MULTIPLIER) {
            state->combo_multiplier = PB_MAX_CHAIN_MULTIPLIER;
        }
        score *= state->combo_multiplier;
        state->score += (uint32_t)score;

        /* Calculate garbage to send (versus mode: matched + orphans - 2) */
        if (state->ruleset.mode == PB_MODE_VERSUS) {
            int garbage = count - 2;
            if (garbage > 0) {
                state->pending_garbage_send += garbage;
            }
        }

        /* Log event with compact cell indices */
        pb_event event = {.type = PB_EVENT_BUBBLES_POPPED, .frame = state->frame};
        int event_count = (count > PB_EVENT_CELL_MAX) ? PB_EVENT_CELL_MAX : count;
        event.data.popped.count = (uint8_t)event_count;
        offsets_to_indices(matches.cells, event.data.popped.cells, count);
        pb_game_add_event(state, &event);

        return count;
    }

    /* No match - reset combo but keep quantifier */
    state->combo_multiplier = 0;
    return 0;
}

int pb_game_process_orphans(pb_game_state* state)
{
    pb_visit_result orphans;
    int count = pb_find_orphans(&state->board, &orphans);

    if (count > 0) {
        pb_board_remove_cells(&state->board, &orphans);

        /* Enhanced orphan scoring:
         * Orphans are worth more (PB_SCORE_BASE_ORPHAN vs PB_SCORE_BASE_MATCH)
         * Also contribute to score quantifier */
        int score = 0;
        for (int i = 0; i < count; i++) {
            int tier = (state->score_quantifier / PB_SCORE_QUANTIFIER_DIV) + 1;
            score += PB_SCORE_BASE_ORPHAN * tier;
            state->score_quantifier++;
        }

        /* Apply chain multiplier (cascading orphans are very valuable) */
        if (state->combo_multiplier > 0) {
            score *= state->combo_multiplier;
        }
        state->score += (uint32_t)score;

        /* Orphans also contribute to garbage in versus mode */
        if (state->ruleset.mode == PB_MODE_VERSUS) {
            state->pending_garbage_send += count;
        }

        /* Log event with compact cell indices */
        pb_event event = {.type = PB_EVENT_BUBBLES_DROPPED, .frame = state->frame};
        int event_count = (count > PB_EVENT_CELL_MAX) ? PB_EVENT_CELL_MAX : count;
        event.data.dropped.count = (uint8_t)event_count;
        offsets_to_indices(orphans.cells, event.data.dropped.cells, count);
        pb_game_add_event(state, &event);
    }

    return count;
}

void pb_game_next_bubble(pb_game_state* state)
{
    state->current_bubble = state->preview_bubble;
    state->preview_bubble = generate_bubble(state);
}

/*============================================================================
 * Garbage Exchange (Versus Mode)
 *============================================================================*/

int pb_game_get_garbage_to_send(pb_game_state* state)
{
    int count = state->pending_garbage_send;
    state->pending_garbage_send = 0;
    return count;
}

pb_result pb_game_receive_garbage(pb_game_state* state, int count)
{
    if (count <= 0) {
        return PB_OK;
    }

    /* Insert garbage as new rows at top */
    int rows_to_add = (count + state->board.cols_even - 1) / state->board.cols_even;

    for (int i = 0; i < rows_to_add; i++) {
        /* Use random colors from current board */
        uint8_t colors = pb_board_color_mask(&state->board);
        if (colors == 0) {
            colors = state->ruleset.allowed_colors;
        }

        if (!pb_board_insert_row(&state->board, &state->rng, colors)) {
            /* Row insertion caused overflow - game over */
            state->phase = PB_PHASE_LOST;
            pb_event lose_event = {
                .type = PB_EVENT_GAME_OVER,
                .frame = state->frame
            };
            pb_game_add_event(state, &lose_event);
            return PB_ERR_INVALID_STATE;
        }
    }

    /* Log garbage event */
    pb_event event = {
        .type = PB_EVENT_GARBAGE_RECEIVED,
        .frame = state->frame,
        .data.garbage = {0, (uint8_t)count}
    };
    pb_game_add_event(state, &event);

    return PB_OK;
}

bool pb_game_is_hurry(const pb_game_state* state)
{
    return state->hurry_active || state->phase == PB_PHASE_HURRY;
}

uint32_t pb_game_get_hurry_countdown(const pb_game_state* state)
{
    if (state->shot.phase != PB_SHOT_IDLE) {
        return 0;
    }

    uint32_t frames_since_shot = state->frame - state->last_shot_frame;
    if (frames_since_shot >= PB_HURRY_AUTOFIRE_FRAMES) {
        return 0;
    }

    return PB_HURRY_AUTOFIRE_FRAMES - frames_since_shot;
}

/*============================================================================
 * Game Queries
 *============================================================================*/

bool pb_game_is_over(const pb_game_state* state)
{
    return state->phase == PB_PHASE_WON || state->phase == PB_PHASE_LOST;
}

bool pb_game_is_won(const pb_game_state* state)
{
    return state->phase == PB_PHASE_WON;
}

bool pb_game_is_lost(const pb_game_state* state)
{
    return state->phase == PB_PHASE_LOST;
}

uint32_t pb_game_checksum(const pb_game_state* state)
{
    uint32_t hash = 2166136261u;

    /* Board state */
    hash ^= pb_board_checksum(&state->board);
    hash *= 16777619u;

    /* RNG state */
    hash ^= pb_rng_checksum(&state->rng);
    hash *= 16777619u;

    /* Game state */
    hash ^= (uint32_t)state->frame;
    hash *= 16777619u;
    hash ^= state->score;
    hash *= 16777619u;
    hash ^= (uint32_t)state->shots_fired;
    hash *= 16777619u;

    return hash;
}

/*============================================================================
 * Event Log
 *============================================================================*/

int pb_game_get_events(const pb_game_state* state, int start_idx,
                       pb_event* events, int max_events)
{
    int count = 0;
    for (int i = start_idx; i < state->event_count && count < max_events; i++) {
        events[count++] = state->events[i];
    }
    return count;
}

void pb_game_clear_events(pb_game_state* state)
{
    state->event_count = 0;
}

void pb_game_add_event(pb_game_state* state, const pb_event* event)
{
    if (state->event_count < 256) {
        state->events[state->event_count++] = *event;
    }
}

/*============================================================================
 * Trajectory Preview
 *============================================================================*/

int pb_game_get_trajectory(const pb_game_state* state, pb_scalar angle,
                           pb_point* points, int max_points)
{
    pb_playfield field;
    pb_playfield_calc(&field, &state->board, state->ruleset.bubble_radius);

    pb_vec2 velocity;
    velocity.x = PB_FIXED_MUL(PB_SCALAR_COS(angle), PB_DEFAULT_SHOT_SPEED);
    velocity.y = -PB_FIXED_MUL(PB_SCALAR_SIN(angle), PB_DEFAULT_SHOT_SPEED);

    int count = 0;
    pb_shot_simulate(field.cannon_pos, velocity, &state->board,
                     field.bubble_radius, field.left_wall, field.right_wall,
                     field.ceiling, state->ruleset.max_bounces,
                     points, &count, max_points);

    return count;
}

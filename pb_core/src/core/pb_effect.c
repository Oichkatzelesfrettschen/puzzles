/*
 * pb_effect.c - Bubble effect system implementation
 *
 * Provides a data-driven framework for special bubble effects.
 * Effects are triggered by game events and execute actions on targets.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_effect.h"
#include "pb/pb_hex.h"


/*============================================================================
 * Default Effect Definitions
 *============================================================================*/

/* Bomb: destroys neighbors (radius 1) */
static const pb_effect_def effect_bomb = {
    .trigger = PB_TRIGGER_ON_POP,
    .action = PB_ACTION_DESTROY,
    .target = {PB_TARGET_NEIGHBORS, {0}},
    .value = 1,
    .delay_frames = 0,
    .chain_allowed = true
};

/* Lightning: destroys entire row */
static const pb_effect_def effect_lightning = {
    .trigger = PB_TRIGGER_ON_POP,
    .action = PB_ACTION_DESTROY,
    .target = {PB_TARGET_ROW, {0}},
    .value = 1,
    .delay_frames = 0,
    .chain_allowed = true
};

/* Star: destroys all of the hit color */
static const pb_effect_def effect_star = {
    .trigger = PB_TRIGGER_ON_HIT,
    .action = PB_ACTION_DESTROY,
    .target = {PB_TARGET_COLOR, {0}},  /* Color set at runtime */
    .value = 1,
    .delay_frames = 0,
    .chain_allowed = true
};

/* Rainbow: matches any color (no active effect, handled in match logic) */
static const pb_effect_def effect_rainbow = {
    .trigger = PB_TRIGGER_NONE,
    .action = PB_ACTION_NONE,
    .target = {PB_TARGET_SELF, {0}},
    .value = 0,
    .delay_frames = 0,
    .chain_allowed = false
};

/* Shifter: changes color each turn */
static const pb_effect_def effect_shifter = {
    .trigger = PB_TRIGGER_ON_TURN_END,
    .action = PB_ACTION_CHANGE_COLOR,
    .target = {PB_TARGET_SELF, {0}},
    .value = 1,  /* Increment color by 1 */
    .delay_frames = 0,
    .chain_allowed = false
};

/* Ice: prevents falling (handled in orphan logic) */
static const pb_effect_def effect_ice = {
    .trigger = PB_TRIGGER_ON_ADJACENT_POP,
    .action = PB_ACTION_CONVERT,  /* Convert to normal bubble */
    .target = {PB_TARGET_SELF, {0}},
    .value = 0,
    .delay_frames = 30,  /* Half second at 60fps */
    .chain_allowed = false
};

/* Virus: spreads color to neighbors */
static const pb_effect_def effect_virus = {
    .trigger = PB_TRIGGER_ON_TURN_END,
    .action = PB_ACTION_CHANGE_COLOR,
    .target = {PB_TARGET_NEIGHBORS, {0}},
    .value = 0,  /* Set to own color */
    .delay_frames = 0,
    .chain_allowed = false
};

/* Magnetic: attracts projectile (handled in shot physics) */
static const pb_effect_def effect_magnetic = {
    .trigger = PB_TRIGGER_NONE,
    .action = PB_ACTION_NONE,
    .target = {PB_TARGET_SELF, {0}},
    .value = 0,
    .delay_frames = 0,
    .chain_allowed = false
};

/* Split: divides into two colors when hit */
static const pb_effect_def effect_split = {
    .trigger = PB_TRIGGER_ON_HIT,
    .action = PB_ACTION_SPAWN,
    .target = {PB_TARGET_NEIGHBORS, {0}},
    .value = 2,  /* Spawn 2 new bubbles */
    .delay_frames = 0,
    .chain_allowed = true
};

/* Effect lookup table */
static const pb_effect_def* default_effects[PB_SPECIAL_COUNT] = {
    [PB_SPECIAL_NONE] = NULL,
    [PB_SPECIAL_BOMB] = &effect_bomb,
    [PB_SPECIAL_LIGHTNING] = &effect_lightning,
    [PB_SPECIAL_STAR] = &effect_star,
    [PB_SPECIAL_MAGNETIC] = &effect_magnetic,
    [PB_SPECIAL_RAINBOW] = &effect_rainbow,
    [PB_SPECIAL_SHIFTER] = &effect_shifter,
    [PB_SPECIAL_PORTAL] = NULL,  /* Handled in shot physics */
    [PB_SPECIAL_ICE] = &effect_ice,
    [PB_SPECIAL_SPLIT] = &effect_split,
    [PB_SPECIAL_VIRUS] = &effect_virus,
    [PB_SPECIAL_KEY] = NULL,     /* Handled in match logic */
    [PB_SPECIAL_LOCK] = NULL,    /* Handled in match logic */
};

/* Custom effect overrides */
static pb_effect_def custom_effects[PB_SPECIAL_COUNT];
static bool custom_effect_set[PB_SPECIAL_COUNT] = {false};

/*============================================================================
 * Effect Registration
 *============================================================================*/

const pb_effect_def* pb_get_special_effect(pb_special_type special)
{
    if (special >= PB_SPECIAL_COUNT) {
        return NULL;
    }

    if (custom_effect_set[special]) {
        return &custom_effects[special];
    }

    return default_effects[special];
}

const pb_effect_def* pb_register_effect(pb_special_type special,
                                        const pb_effect_def* effect)
{
    if (special >= PB_SPECIAL_COUNT) {
        return NULL;
    }

    const pb_effect_def* previous = pb_get_special_effect(special);

    if (effect == NULL) {
        custom_effect_set[special] = false;
    } else {
        custom_effects[special] = *effect;
        custom_effect_set[special] = true;
    }

    return previous;
}

/*============================================================================
 * Target Finding
 *============================================================================*/

static int find_radius_targets(const pb_board* board, pb_offset origin,
                               int radius, pb_visit_result* result)
{
    result->count = 0;

    /* Scan all cells and check distance */
    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};

            if (pb_board_is_empty(board, pos)) {
                continue;
            }

            if (pb_hex_distance_offset(origin, pos) <= radius) {
                result->cells[result->count++] = pos;
            }
        }
    }

    return result->count;
}

static int find_row_targets(const pb_board* board, int row,
                            pb_visit_result* result)
{
    result->count = 0;

    if (row < 0 || row >= board->rows) {
        return 0;
    }

    int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
    for (int col = 0; col < cols; col++) {
        pb_offset pos = {row, col};

        if (!pb_board_is_empty(board, pos)) {
            result->cells[result->count++] = pos;
        }
    }

    return result->count;
}

static int find_color_targets(const pb_board* board, uint8_t color_id,
                              pb_visit_result* result)
{
    result->count = 0;

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            const pb_bubble* b = pb_board_get_const(board, pos);

            if (b == NULL || b->kind == PB_KIND_NONE) {
                continue;
            }

            if (b->kind == PB_KIND_COLORED && b->color_id == color_id) {
                result->cells[result->count++] = pos;
            }
        }
    }

    return result->count;
}

int pb_find_targets(const pb_board* board, pb_offset origin,
                    const pb_target* target, pb_visit_result* result)
{
    result->count = 0;

    switch (target->type) {
    case PB_TARGET_SELF:
        if (!pb_board_is_empty(board, origin)) {
            result->cells[0] = origin;
            result->count = 1;
        }
        break;

    case PB_TARGET_NEIGHBORS:
        {
            pb_offset neighbors[6];
            pb_hex_neighbors_offset(origin, neighbors);

            for (int i = 0; i < 6; i++) {
                if (pb_board_in_bounds(board, neighbors[i]) &&
                    !pb_board_is_empty(board, neighbors[i])) {
                    result->cells[result->count++] = neighbors[i];
                }
            }
        }
        break;

    case PB_TARGET_RADIUS:
        find_radius_targets(board, origin, target->param.radius, result);
        break;

    case PB_TARGET_ROW:
        find_row_targets(board, origin.row, result);
        break;

    case PB_TARGET_COLOR:
        find_color_targets(board, target->param.color_id, result);
        break;

    case PB_TARGET_CONNECTED:
        /* Use find_matches for connected targets */
        pb_find_matches(board, origin, result);
        break;

    case PB_TARGET_ALL:
        for (int row = 0; row < board->rows; row++) {
            int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
            for (int col = 0; col < cols; col++) {
                pb_offset pos = {row, col};
                if (!pb_board_is_empty(board, pos)) {
                    result->cells[result->count++] = pos;
                }
            }
        }
        break;

    default:
        break;
    }

    return result->count;
}

/*============================================================================
 * Effect Execution
 *============================================================================*/

pb_result pb_execute_effect(pb_board* board, pb_offset origin,
                            const pb_effect_def* effect,
                            pb_effect_result* result)
{
    if (effect == NULL || result == NULL) {
        return PB_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));

    /* Find targets */
    pb_find_targets(board, origin, &effect->target, &result->affected);

    if (result->affected.count == 0) {
        return PB_OK;  /* No targets, nothing to do */
    }

    /* Execute action */
    switch (effect->action) {
    case PB_ACTION_NONE:
        break;

    case PB_ACTION_DESTROY:
        pb_board_remove_cells(board, &result->affected);
        result->board_changed = true;
        /* Base score: 10 per bubble destroyed */
        result->score_bonus = result->affected.count * 10 * effect->value;
        break;

    case PB_ACTION_CHANGE_COLOR:
        for (int i = 0; i < result->affected.count; i++) {
            pb_bubble* b = pb_board_get(board, result->affected.cells[i]);
            if (b && b->kind == PB_KIND_COLORED) {
                /* Cycle color */
                b->color_id = (uint8_t)((b->color_id + effect->value) % PB_MAX_COLORS);
                result->board_changed = true;
            }
        }
        break;

    case PB_ACTION_CONVERT:
        for (int i = 0; i < result->affected.count; i++) {
            pb_bubble* b = pb_board_get(board, result->affected.cells[i]);
            if (b && b->kind == PB_KIND_SPECIAL) {
                /* Convert to regular colored bubble */
                b->kind = PB_KIND_COLORED;
                b->special = PB_SPECIAL_NONE;
                b->flags = 0;
                result->board_changed = true;
            }
        }
        break;

    case PB_ACTION_SPAWN:
        /* Spawn is complex - typically handled by game state */
        /* Mark as needing spawn processing */
        result->board_changed = true;
        break;

    case PB_ACTION_SCORE:
        result->score_bonus = effect->value;
        break;

    case PB_ACTION_SPREAD:
    case PB_ACTION_CHAIN:
        /* These require game state context */
        break;

    case PB_ACTION_COUNT:
        break;

    default:
        break;
    }

    return PB_OK;
}

/*============================================================================
 * Effect Queue
 *============================================================================*/

void pb_effect_queue_init(pb_effect_queue* queue)
{
    memset(queue, 0, sizeof(*queue));
}

bool pb_effect_queue_add(pb_effect_queue* queue, pb_offset origin,
                         const pb_effect_def* effect, int delay_frames)
{
    if (queue->count >= PB_MAX_PENDING_EFFECTS) {
        return false;
    }

    pb_pending_effect* pending = &queue->effects[queue->count++];
    pending->origin = origin;
    pending->effect = *effect;
    pending->trigger_frame = queue->current_frame + delay_frames;

    return true;
}

int pb_effect_queue_process(pb_effect_queue* queue, pb_board* board,
                            pb_effect_result* results, int max_results)
{
    int executed = 0;

    /* Process effects ready at current frame */
    int i = 0;
    while (i < queue->count && executed < max_results) {
        pb_pending_effect* pending = &queue->effects[i];

        if (pending->trigger_frame <= queue->current_frame) {
            /* Execute effect */
            pb_execute_effect(board, pending->origin, &pending->effect,
                              &results[executed++]);

            /* Remove from queue (swap with last) */
            queue->effects[i] = queue->effects[--queue->count];
            /* Don't increment i - check swapped element */
        } else {
            i++;
        }
    }

    return executed;
}

void pb_effect_queue_tick(pb_effect_queue* queue)
{
    queue->current_frame++;
}

bool pb_effect_queue_has_pending(const pb_effect_queue* queue)
{
    return queue->count > 0;
}

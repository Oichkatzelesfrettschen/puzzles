/*
 * pb_effect.h - Bubble effect system
 *
 * Data-driven trigger/effect framework for special bubbles.
 * Effects are triggered by game events and produce actions.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_EFFECT_H
#define PB_EFFECT_H

#include "pb_types.h"
#include "pb_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Effect Trigger Events
 *============================================================================*/

typedef enum pb_trigger_type {
    PB_TRIGGER_NONE = 0,
    PB_TRIGGER_ON_HIT,          /* Shot hits this bubble */
    PB_TRIGGER_ON_POP,          /* This bubble is popped */
    PB_TRIGGER_ON_DROP,         /* This bubble falls (orphaned) */
    PB_TRIGGER_ON_PLACE,        /* Shot is placed next to this bubble */
    PB_TRIGGER_ON_TURN_END,     /* After all matches resolved */
    PB_TRIGGER_ON_ADJACENT_POP, /* Neighbor bubble popped */
    PB_TRIGGER_COUNT
} pb_trigger_type;

/*============================================================================
 * Effect Action Types
 *============================================================================*/

typedef enum pb_action_type {
    PB_ACTION_NONE = 0,
    PB_ACTION_DESTROY,          /* Remove bubbles matching criteria */
    PB_ACTION_CHANGE_COLOR,     /* Change bubble colors */
    PB_ACTION_SPREAD,           /* Spread to adjacent cells */
    PB_ACTION_CONVERT,          /* Convert bubbles to different type */
    PB_ACTION_SPAWN,            /* Create new bubbles */
    PB_ACTION_SCORE,            /* Award bonus score */
    PB_ACTION_CHAIN,            /* Trigger another effect */
    PB_ACTION_COUNT
} pb_action_type;

/*============================================================================
 * Target Selection
 *============================================================================*/

typedef enum pb_target_type {
    PB_TARGET_SELF = 0,         /* The bubble itself */
    PB_TARGET_NEIGHBORS,        /* Adjacent cells (radius 1) */
    PB_TARGET_RADIUS,           /* Cells within N hexes */
    PB_TARGET_ROW,              /* All cells in same row */
    PB_TARGET_COLOR,            /* All bubbles of a color */
    PB_TARGET_CONNECTED,        /* All connected bubbles */
    PB_TARGET_ALL               /* All bubbles on board */
} pb_target_type;

typedef struct pb_target {
    pb_target_type type;
    union {
        int radius;             /* For PB_TARGET_RADIUS */
        uint8_t color_id;       /* For PB_TARGET_COLOR */
    } param;
} pb_target;

/*============================================================================
 * Effect Definition
 *============================================================================*/

typedef struct pb_effect_def {
    pb_trigger_type trigger;
    pb_action_type action;
    pb_target target;
    int value;                  /* Action-specific value (damage, score, etc.) */
    int delay_frames;           /* Frames before effect executes */
    bool chain_allowed;         /* Can trigger other effects */
} pb_effect_def;

/*============================================================================
 * Effect Result (returned by effect execution)
 *============================================================================*/

typedef struct pb_effect_result {
    pb_visit_result affected;   /* Cells affected by this effect */
    int score_bonus;            /* Score awarded */
    int chain_level;            /* Chain combo level */
    bool board_changed;         /* True if board state modified */
} pb_effect_result;

/*============================================================================
 * Effect Registration
 *============================================================================*/

/**
 * Get the default effect for a special bubble type.
 */
const pb_effect_def* pb_get_special_effect(pb_special_type special);

/**
 * Register a custom effect for a special type.
 * Returns the previous effect definition (for restoration).
 */
const pb_effect_def* pb_register_effect(pb_special_type special,
                                        const pb_effect_def* effect);

/*============================================================================
 * Effect Execution
 *============================================================================*/

/**
 * Execute an effect triggered at a specific cell.
 *
 * @param board     Board to modify
 * @param origin    Cell where effect originates
 * @param effect    Effect definition to execute
 * @param result    Output: effect results
 * @return          PB_OK on success
 */
pb_result pb_execute_effect(pb_board* board, pb_offset origin,
                            const pb_effect_def* effect,
                            pb_effect_result* result);

/**
 * Find all cells matching a target specification.
 *
 * @param board     Board to search
 * @param origin    Origin cell for relative targets
 * @param target    Target specification
 * @param result    Output: matching cells
 * @return          Number of cells matched
 */
int pb_find_targets(const pb_board* board, pb_offset origin,
                    const pb_target* target, pb_visit_result* result);

/*============================================================================
 * Effect Processing Pipeline
 *============================================================================*/

/**
 * Pending effect in the execution queue.
 */
typedef struct pb_pending_effect {
    pb_offset origin;
    pb_effect_def effect;
    int trigger_frame;          /* Frame when effect should execute */
} pb_pending_effect;

#define PB_MAX_PENDING_EFFECTS 64

/**
 * Effect queue for managing delayed and chained effects.
 */
typedef struct pb_effect_queue {
    pb_pending_effect effects[PB_MAX_PENDING_EFFECTS];
    int count;
    int current_frame;
} pb_effect_queue;

/**
 * Initialize effect queue.
 */
void pb_effect_queue_init(pb_effect_queue* queue);

/**
 * Add an effect to the queue (immediate or delayed).
 */
bool pb_effect_queue_add(pb_effect_queue* queue, pb_offset origin,
                         const pb_effect_def* effect, int delay_frames);

/**
 * Process all effects ready to execute at the current frame.
 *
 * @param queue     Effect queue
 * @param board     Board to modify
 * @param results   Output array for effect results
 * @param max_results Maximum results to return
 * @return          Number of effects executed
 */
int pb_effect_queue_process(pb_effect_queue* queue, pb_board* board,
                            pb_effect_result* results, int max_results);

/**
 * Advance queue frame counter.
 */
void pb_effect_queue_tick(pb_effect_queue* queue);

/**
 * Check if queue has pending effects.
 */
bool pb_effect_queue_has_pending(const pb_effect_queue* queue);

#ifdef __cplusplus
}
#endif

#endif /* PB_EFFECT_H */

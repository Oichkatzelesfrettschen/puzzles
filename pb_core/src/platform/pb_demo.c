/*
 * pb_demo.c - Reference demo using pb_core with SDL2 backend
 *
 * Demonstrates a minimal playable Puzzle Bobble implementation.
 *
 * Controls:
 *   Left/Right - Aim cannon
 *   Space/Z    - Fire
 *   X          - Swap bubbles
 *   P/Escape   - Pause
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_core.h"
#include "pb/pb_platform.h"
#include <stdio.h>
#include <math.h>

/*============================================================================
 * Constants
 *============================================================================*/

/* Logical screen (NES-like resolution) */
#define SCREEN_W 256
#define SCREEN_H 224

/* Bubble size and layout */
#define BUBBLE_RADIUS 8
#define BOARD_OFFSET_X 64
#define BOARD_OFFSET_Y 16

/* Cannon position */
#define CANNON_X 128
#define CANNON_Y 200
#define CANNON_LENGTH 20

/* Aim speed (radians per frame) */
#define AIM_SPEED 0.03f

/*============================================================================
 * Colors
 *============================================================================*/

static const pb_color_srgb8 COLORS[] = {
    {255, 0, 0, 255},       /* Red */
    {0, 128, 255, 255},     /* Blue */
    {0, 200, 0, 255},       /* Green */
    {255, 255, 0, 255},     /* Yellow */
    {255, 128, 0, 255},     /* Orange */
    {200, 0, 255, 255},     /* Purple */
    {255, 128, 192, 255},   /* Pink */
    {128, 64, 0, 255},      /* Brown */
};

static const pb_color_srgb8 BG_COLOR = {32, 32, 48, 255};
static const pb_color_srgb8 WALL_COLOR = {64, 64, 80, 255};
static const pb_color_srgb8 AIM_COLOR = {255, 255, 255, 128};

/*============================================================================
 * Demo State
 *============================================================================*/

typedef struct demo_state {
    pb_platform* platform;
    pb_session session;
    pb_scalar aim_angle;
    bool running;
    bool paused;
} demo_state;

/*============================================================================
 * Rendering
 *============================================================================*/

static void draw_bubble(pb_platform* p, int cx, int cy, const pb_bubble* bubble)
{
    if (bubble->kind == PB_KIND_NONE) return;

    pb_color_srgb8 color = {128, 128, 128, 255};
    if (bubble->kind == PB_KIND_COLORED && bubble->color_id < 8) {
        color = COLORS[bubble->color_id];
    }

    /* Filled circle */
    p->draw_circle(p, cx, cy, BUBBLE_RADIUS - 1, color);

    /* Highlight */
    pb_color_srgb8 highlight = {
        (uint8_t)(color.r + (255 - color.r) / 2),
        (uint8_t)(color.g + (255 - color.g) / 2),
        (uint8_t)(color.b + (255 - color.b) / 2),
        255
    };
    p->draw_rect(p, cx - 2, cy - 3, 3, 2, highlight);
}

static void draw_board(demo_state* state)
{
    pb_platform* p = state->platform;
    const pb_board* board = &state->session.game.board;

    for (int row = 0; row < board->rows; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            const pb_bubble* bubble = pb_board_get_const(board, pos);

            if (bubble->kind == PB_KIND_NONE) continue;

            /* Convert to pixel coords */
            pb_point px = pb_offset_to_pixel(pos, PB_FLOAT_TO_FIXED(BUBBLE_RADIUS));
            int cx = BOARD_OFFSET_X + PB_FIXED_TO_INT(px.x);
            int cy = BOARD_OFFSET_Y + PB_FIXED_TO_INT(px.y);

            draw_bubble(p, cx, cy, bubble);
        }
    }
}

static void draw_shot(demo_state* state)
{
    pb_platform* p = state->platform;
    const pb_shot* shot = &state->session.game.shot;

    if (shot->phase != PB_SHOT_MOVING) return;

    int cx = BOARD_OFFSET_X + PB_FIXED_TO_INT(shot->pos.x);
    int cy = BOARD_OFFSET_Y + PB_FIXED_TO_INT(shot->pos.y);

    draw_bubble(p, cx, cy, &shot->bubble);
}

static void draw_cannon(demo_state* state)
{
    pb_platform* p = state->platform;

    /* Base position */
    int bx = BOARD_OFFSET_X + CANNON_X;
    int by = BOARD_OFFSET_Y + CANNON_Y;

    /* Draw base */
    pb_color_srgb8 base_color = {100, 100, 120, 255};
    p->draw_rect(p, bx - 12, by, 24, 12, base_color);

    /* Draw cannon barrel */
    float angle = PB_FIXED_TO_FLOAT(state->aim_angle);
    int tx = bx + (int)(cosf(angle) * CANNON_LENGTH);
    int ty = by - (int)(sinf(angle) * CANNON_LENGTH);
    p->draw_line(p, bx, by, tx, ty, (pb_color_srgb8){200, 200, 220, 255});
    p->draw_line(p, bx - 1, by, tx - 1, ty, (pb_color_srgb8){200, 200, 220, 255});
    p->draw_line(p, bx + 1, by, tx + 1, ty, (pb_color_srgb8){200, 200, 220, 255});

    /* Draw current bubble */
    draw_bubble(p, bx, by, &state->session.game.current_bubble);

    /* Draw preview bubble */
    draw_bubble(p, bx + 30, by + 8, &state->session.game.preview_bubble);
}

static void draw_aim_line(demo_state* state)
{
    pb_platform* p = state->platform;

    float angle = PB_FIXED_TO_FLOAT(state->aim_angle);
    int bx = BOARD_OFFSET_X + CANNON_X;
    int by = BOARD_OFFSET_Y + CANNON_Y;

    /* Draw dotted aim line */
    for (int i = 0; i < 10; i++) {
        int dist = CANNON_LENGTH + 10 + i * 12;
        int x = bx + (int)(cosf(angle) * dist);
        int y = by - (int)(sinf(angle) * dist);

        if (y < BOARD_OFFSET_Y) break;
        if (x < BOARD_OFFSET_X || x > BOARD_OFFSET_X + SCREEN_W - 2 * BOARD_OFFSET_X) break;

        p->draw_rect(p, x - 1, y - 1, 2, 2, AIM_COLOR);
    }
}

static void draw_walls(demo_state* state)
{
    pb_platform* p = state->platform;

    /* Left wall */
    p->draw_rect(p, BOARD_OFFSET_X - 4, BOARD_OFFSET_Y, 4, CANNON_Y, WALL_COLOR);

    /* Right wall */
    int right_x = BOARD_OFFSET_X + SCREEN_W - 2 * BOARD_OFFSET_X;
    p->draw_rect(p, right_x, BOARD_OFFSET_Y, 4, CANNON_Y, WALL_COLOR);

    /* Ceiling */
    p->draw_rect(p, BOARD_OFFSET_X - 4, BOARD_OFFSET_Y - 4,
                 SCREEN_W - 2 * BOARD_OFFSET_X + 8, 4, WALL_COLOR);
}

static void draw_ui(demo_state* state)
{
    pb_platform* p = state->platform;
    (void)p; /* Would draw score, frame count, etc. here */

    /* For now, just indicate pause state */
    if (state->paused) {
        pb_color_srgb8 overlay = {0, 0, 0, 128};
        p->draw_rect(p, 0, 0, SCREEN_W, SCREEN_H, overlay);

        /* Simple "PAUSED" indicator */
        pb_color_srgb8 white = {255, 255, 255, 255};
        p->draw_rect(p, SCREEN_W / 2 - 30, SCREEN_H / 2 - 5, 60, 10, white);
    }
}

static void render(demo_state* state)
{
    pb_platform* p = state->platform;

    pb_clear(p, BG_COLOR);

    draw_walls(state);
    draw_board(state);
    draw_shot(state);
    draw_aim_line(state);
    draw_cannon(state);
    draw_ui(state);

    pb_present(p);
}

/*============================================================================
 * Game Logic
 *============================================================================*/

static void handle_input(demo_state* state, const pb_input_state* input)
{
    /* Pause toggle */
    if (input->keys_pressed[PB_KEY_PAUSE]) {
        state->paused = !state->paused;
    }

    if (state->paused) return;

    /* Aiming */
    if (input->keys[PB_KEY_LEFT]) {
        state->aim_angle += PB_FLOAT_TO_FIXED(AIM_SPEED);
        if (state->aim_angle > PB_MAX_ANGLE) {
            state->aim_angle = PB_MAX_ANGLE;
        }
    }
    if (input->keys[PB_KEY_RIGHT]) {
        state->aim_angle -= PB_FLOAT_TO_FIXED(AIM_SPEED);
        if (state->aim_angle < PB_MIN_ANGLE) {
            state->aim_angle = PB_MIN_ANGLE;
        }
    }

    /* Update session with current aim angle */
    pb_session_set_angle(&state->session, state->aim_angle);

    /* Fire */
    if (input->keys_pressed[PB_KEY_FIRE]) {
        if (state->session.game.shot.phase == PB_SHOT_IDLE) {
            pb_session_fire(&state->session);
        }
    }

    /* Swap bubbles */
    if (input->keys_pressed[PB_KEY_SWAP]) {
        pb_session_swap(&state->session);
    }
}

static void update(demo_state* state)
{
    if (state->paused) return;

    pb_session_tick(&state->session);

    /* Check game state */
    if (state->session.game.phase == PB_PHASE_WON) {
        printf("You win! Score: %u\n", state->session.game.score);
        state->running = false;
    } else if (state->session.game.phase == PB_PHASE_LOST) {
        printf("Game over! Score: %u\n", state->session.game.score);
        state->running = false;
    }
}

/*============================================================================
 * Initialization
 *============================================================================*/

static void setup_test_level(pb_board* board)
{
    pb_board_init(board);

    /* Create a simple test pattern - 4 rows of colored bubbles */
    pb_rng rng;
    pb_rng_seed(&rng, 12345);

    /* Allow 5 colors (bits 0-4) */
    uint8_t allowed_colors = 0x1F;

    for (int row = 0; row < 4; row++) {
        int cols = pb_row_cols(row, board->cols_even, board->cols_odd);
        for (int col = 0; col < cols; col++) {
            pb_offset pos = {row, col};
            int color = pb_rng_pick_color(&rng, allowed_colors);
            pb_bubble bubble = {
                .kind = PB_KIND_COLORED,
                .color_id = (uint8_t)(color >= 0 ? color : 0),
                .flags = 0,
                .special = PB_SPECIAL_NONE,
                .payload = {0}
            };
            pb_board_set(board, pos, bubble);
        }
    }
}

/*============================================================================
 * Main
 *============================================================================*/

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    /* Create platform */
    pb_platform* platform = pb_platform_sdl2_create();
    if (!platform) {
        fprintf(stderr, "Failed to create platform\n");
        return 1;
    }

    /* Initialize platform */
    pb_platform_config config = {
        .title = "Puzzle Bobble - pb_core demo",
        .window_width = 768,
        .window_height = 672,
        .logical_width = SCREEN_W,
        .logical_height = SCREEN_H,
        .fullscreen = false,
        .vsync = true,
        .target_fps = 60
    };

    if (!pb_init(platform, &config)) {
        fprintf(stderr, "Failed to initialize platform\n");
        pb_platform_free(platform);
        return 1;
    }

    /* Initialize demo state */
    demo_state state = {0};
    state.platform = platform;
    state.running = true;
    state.paused = false;
    state.aim_angle = PB_FLOAT_TO_FIXED(3.14159265f / 2.0f);  /* Straight up */

    /* Initialize session (live mode with recording) */
    pb_session_config sess_config;
    pb_session_config_default(&sess_config);
    sess_config.mode = PB_SESSION_RECORDING;
    sess_config.auto_checkpoint = false;

    pb_session_create(&state.session, NULL, 42, &sess_config);
    setup_test_level(&state.session.game.board);

    /* Main loop */
    pb_input_state input;
    while (state.running && !pb_should_quit(platform)) {
        pb_begin_frame(platform);

        pb_poll_input(platform, &input);
        handle_input(&state, &input);
        update(&state);
        render(&state);

        pb_end_frame(platform);
    }

    /* Cleanup */
    pb_session_destroy(&state.session);
    pb_shutdown(platform);
    pb_platform_free(platform);

    printf("Demo finished.\n");
    return 0;
}

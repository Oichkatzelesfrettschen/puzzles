/*
 * pb_platform.h - Platform abstraction interface
 *
 * Defines interfaces for rendering, input, audio, and timing.
 * Platform backends (SDL2, raylib, etc.) implement these interfaces.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_PLATFORM_H
#define PB_PLATFORM_H

#include "pb_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

typedef struct pb_platform_config {
    const char* title;
    int window_width;
    int window_height;
    int logical_width;      /* Internal resolution (for scaling) */
    int logical_height;
    bool fullscreen;
    bool vsync;
    int target_fps;
} pb_platform_config;

/* Default configuration */
#define PB_PLATFORM_CONFIG_DEFAULT { \
    .title = "Puzzle Bobble", \
    .window_width = 640, \
    .window_height = 480, \
    .logical_width = 256, \
    .logical_height = 224, \
    .fullscreen = false, \
    .vsync = true, \
    .target_fps = 60 \
}

/*============================================================================
 * Input State
 *============================================================================*/

typedef enum pb_key {
    PB_KEY_NONE = 0,
    PB_KEY_LEFT,
    PB_KEY_RIGHT,
    PB_KEY_UP,
    PB_KEY_DOWN,
    PB_KEY_FIRE,        /* Space/Z/Button A */
    PB_KEY_SWAP,        /* X/Button B */
    PB_KEY_PAUSE,       /* Escape/Start */
    PB_KEY_QUIT,
    PB_KEY_COUNT
} pb_key;

typedef struct pb_input_state {
    bool keys[PB_KEY_COUNT];            /* Currently held */
    bool keys_pressed[PB_KEY_COUNT];    /* Just pressed this frame */
    bool keys_released[PB_KEY_COUNT];   /* Just released this frame */
    int mouse_x;
    int mouse_y;
    bool mouse_left;
    bool mouse_right;
    bool mouse_left_pressed;
    bool mouse_right_pressed;
} pb_input_state;

/*============================================================================
 * Render Interface
 *============================================================================*/

/* Texture handle (opaque) */
typedef struct pb_texture* pb_texture;

/* Sprite batch for efficient rendering */
typedef struct pb_sprite {
    pb_texture texture;
    int src_x, src_y, src_w, src_h;      /* Source rectangle */
    float dst_x, dst_y, dst_w, dst_h;    /* Destination rectangle */
    float rotation;                       /* Radians */
    float origin_x, origin_y;            /* Rotation origin */
    pb_color_srgb8 tint;                 /* Color modulation */
    uint8_t alpha;
} pb_sprite;

/*============================================================================
 * Audio Interface
 *============================================================================*/

/* Sound handle (opaque) */
typedef struct pb_sound* pb_sound;

/* Music handle (opaque) */
typedef struct pb_music* pb_music;

/*============================================================================
 * Platform Interface (vtable)
 *============================================================================*/

typedef struct pb_platform {
    void* impl;  /* Platform-specific data */

    /* Lifecycle */
    bool (*init)(struct pb_platform* p, const pb_platform_config* config);
    void (*shutdown)(struct pb_platform* p);
    bool (*should_quit)(struct pb_platform* p);

    /* Frame timing */
    void (*begin_frame)(struct pb_platform* p);
    void (*end_frame)(struct pb_platform* p);
    uint64_t (*get_ticks_ms)(struct pb_platform* p);
    void (*delay)(struct pb_platform* p, uint32_t ms);

    /* Input */
    void (*poll_input)(struct pb_platform* p, pb_input_state* state);

    /* Rendering */
    void (*clear)(struct pb_platform* p, pb_color_srgb8 color);
    void (*draw_rect)(struct pb_platform* p, int x, int y, int w, int h,
                      pb_color_srgb8 color);
    void (*draw_rect_outline)(struct pb_platform* p, int x, int y, int w, int h,
                              pb_color_srgb8 color);
    void (*draw_circle)(struct pb_platform* p, int cx, int cy, int r,
                        pb_color_srgb8 color);
    void (*draw_circle_outline)(struct pb_platform* p, int cx, int cy, int r,
                                pb_color_srgb8 color);
    void (*draw_line)(struct pb_platform* p, int x1, int y1, int x2, int y2,
                      pb_color_srgb8 color);
    void (*present)(struct pb_platform* p);

    /* Texture management */
    pb_texture (*texture_load)(struct pb_platform* p, const char* path);
    pb_texture (*texture_create)(struct pb_platform* p, int w, int h);
    void (*texture_free)(struct pb_platform* p, pb_texture tex);
    void (*texture_draw)(struct pb_platform* p, pb_texture tex,
                         int src_x, int src_y, int src_w, int src_h,
                         int dst_x, int dst_y, int dst_w, int dst_h);
    void (*texture_draw_ex)(struct pb_platform* p, const pb_sprite* sprite);

    /* Audio (optional) */
    pb_sound (*sound_load)(struct pb_platform* p, const char* path);
    void (*sound_free)(struct pb_platform* p, pb_sound snd);
    void (*sound_play)(struct pb_platform* p, pb_sound snd, float volume);
    pb_music (*music_load)(struct pb_platform* p, const char* path);
    void (*music_free)(struct pb_platform* p, pb_music mus);
    void (*music_play)(struct pb_platform* p, pb_music mus, bool loop);
    void (*music_stop)(struct pb_platform* p);
    void (*music_set_volume)(struct pb_platform* p, float volume);

} pb_platform;

/*============================================================================
 * Platform Backends
 *============================================================================*/

/**
 * Create SDL2 platform backend.
 * Caller must call init() after creation, shutdown() before free.
 */
pb_platform* pb_platform_sdl2_create(void);

/**
 * Free platform backend.
 */
void pb_platform_free(pb_platform* p);

/*============================================================================
 * Convenience Wrappers
 *============================================================================*/

static inline bool pb_init(pb_platform* p, const pb_platform_config* cfg)
{
    return p && p->init && p->init(p, cfg);
}

static inline void pb_shutdown(pb_platform* p)
{
    if (p && p->shutdown) p->shutdown(p);
}

static inline bool pb_should_quit(pb_platform* p)
{
    return p && p->should_quit && p->should_quit(p);
}

static inline void pb_begin_frame(pb_platform* p)
{
    if (p && p->begin_frame) p->begin_frame(p);
}

static inline void pb_end_frame(pb_platform* p)
{
    if (p && p->end_frame) p->end_frame(p);
}

static inline void pb_poll_input(pb_platform* p, pb_input_state* s)
{
    if (p && p->poll_input) p->poll_input(p, s);
}

static inline void pb_clear(pb_platform* p, pb_color_srgb8 c)
{
    if (p && p->clear) p->clear(p, c);
}

static inline void pb_present(pb_platform* p)
{
    if (p && p->present) p->present(p);
}

#ifdef __cplusplus
}
#endif

#endif /* PB_PLATFORM_H */

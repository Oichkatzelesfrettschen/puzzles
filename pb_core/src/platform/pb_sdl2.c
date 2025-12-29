/*
 * pb_sdl2.c - SDL2 platform backend implementation
 *
 * Implements pb_platform interface using SDL2 for rendering,
 * input handling, audio, and timing.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pb/pb_platform.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * SDL2 Implementation Data
 *============================================================================*/

typedef struct sdl2_impl {
    SDL_Window* window;
    SDL_Renderer* renderer;
    pb_platform_config config;
    bool should_quit;
    uint64_t frame_start;
    uint32_t frame_time_ms;
    pb_input_state prev_input;
} sdl2_impl;

/* Texture wrapper */
struct pb_texture {
    SDL_Texture* sdl_tex;
    int width;
    int height;
};

/* Sound wrapper */
struct pb_sound {
    Mix_Chunk* chunk;
};

/* Music wrapper */
struct pb_music {
    Mix_Music* music;
};

/*============================================================================
 * Lifecycle
 *============================================================================*/

static bool sdl2_init(pb_platform* p, const pb_platform_config* config)
{
    sdl2_impl* impl = p->impl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    impl->config = *config;

    /* Create window */
    Uint32 flags = SDL_WINDOW_SHOWN;
    if (config->fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    impl->window = SDL_CreateWindow(
        config->title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config->window_width, config->window_height,
        flags
    );
    if (!impl->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    /* Create renderer */
    Uint32 render_flags = SDL_RENDERER_ACCELERATED;
    if (config->vsync) {
        render_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    impl->renderer = SDL_CreateRenderer(impl->window, -1, render_flags);
    if (!impl->renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(impl->window);
        SDL_Quit();
        return false;
    }

    /* Set logical resolution for scaling */
    if (config->logical_width > 0 && config->logical_height > 0) {
        SDL_RenderSetLogicalSize(impl->renderer,
                                  config->logical_width,
                                  config->logical_height);
    }

    /* Initialize SDL_image */
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
    }

    /* Initialize SDL_mixer */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    }

    impl->should_quit = false;
    impl->frame_time_ms = config->target_fps > 0 ? 1000 / config->target_fps : 16;
    memset(&impl->prev_input, 0, sizeof(impl->prev_input));

    return true;
}

static void sdl2_shutdown(pb_platform* p)
{
    sdl2_impl* impl = p->impl;

    if (impl->renderer) {
        SDL_DestroyRenderer(impl->renderer);
        impl->renderer = NULL;
    }
    if (impl->window) {
        SDL_DestroyWindow(impl->window);
        impl->window = NULL;
    }

    Mix_CloseAudio();
    IMG_Quit();
    SDL_Quit();
}

static bool sdl2_should_quit(pb_platform* p)
{
    sdl2_impl* impl = p->impl;
    return impl->should_quit;
}

/*============================================================================
 * Frame Timing
 *============================================================================*/

static void sdl2_begin_frame(pb_platform* p)
{
    sdl2_impl* impl = p->impl;
    impl->frame_start = SDL_GetTicks64();
}

static void sdl2_end_frame(pb_platform* p)
{
    sdl2_impl* impl = p->impl;
    uint64_t elapsed = SDL_GetTicks64() - impl->frame_start;

    /* Frame pacing (if not using vsync) */
    if (!impl->config.vsync && elapsed < impl->frame_time_ms) {
        SDL_Delay((uint32_t)(impl->frame_time_ms - elapsed));
    }
}

static uint64_t sdl2_get_ticks_ms(pb_platform* p)
{
    (void)p;
    return SDL_GetTicks64();
}

static void sdl2_delay(pb_platform* p, uint32_t ms)
{
    (void)p;
    SDL_Delay(ms);
}

/*============================================================================
 * Input
 *============================================================================*/

static void sdl2_poll_input(pb_platform* p, pb_input_state* state)
{
    sdl2_impl* impl = p->impl;

    /* Save previous state for edge detection */
    pb_input_state prev = impl->prev_input;
    memset(state, 0, sizeof(*state));

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            impl->should_quit = true;
            state->keys[PB_KEY_QUIT] = true;
        }
    }

    /* Keyboard state */
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    state->keys[PB_KEY_LEFT] = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
    state->keys[PB_KEY_RIGHT] = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
    state->keys[PB_KEY_UP] = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
    state->keys[PB_KEY_DOWN] = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
    state->keys[PB_KEY_FIRE] = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z];
    state->keys[PB_KEY_SWAP] = keys[SDL_SCANCODE_X];
    state->keys[PB_KEY_PAUSE] = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_P];
    state->keys[PB_KEY_QUIT] = impl->should_quit;

    /* Edge detection */
    for (int i = 0; i < PB_KEY_COUNT; i++) {
        state->keys_pressed[i] = state->keys[i] && !prev.keys[i];
        state->keys_released[i] = !state->keys[i] && prev.keys[i];
    }

    /* Mouse state */
    int mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    /* Convert to logical coordinates */
    float lx, ly;
    SDL_RenderWindowToLogical(impl->renderer, mx, my, &lx, &ly);
    state->mouse_x = (int)lx;
    state->mouse_y = (int)ly;

    state->mouse_left = buttons & SDL_BUTTON_LMASK;
    state->mouse_right = buttons & SDL_BUTTON_RMASK;
    state->mouse_left_pressed = state->mouse_left && !prev.mouse_left;
    state->mouse_right_pressed = state->mouse_right && !prev.mouse_right;

    impl->prev_input = *state;
}

/*============================================================================
 * Rendering
 *============================================================================*/

static void sdl2_clear(pb_platform* p, pb_color_srgb8 color)
{
    sdl2_impl* impl = p->impl;
    SDL_SetRenderDrawColor(impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(impl->renderer);
}

static void sdl2_draw_rect(pb_platform* p, int x, int y, int w, int h,
                            pb_color_srgb8 color)
{
    sdl2_impl* impl = p->impl;
    SDL_SetRenderDrawColor(impl->renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(impl->renderer, &rect);
}

static void sdl2_draw_rect_outline(pb_platform* p, int x, int y, int w, int h,
                                    pb_color_srgb8 color)
{
    sdl2_impl* impl = p->impl;
    SDL_SetRenderDrawColor(impl->renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(impl->renderer, &rect);
}

/* Midpoint circle algorithm for filled circle */
static void sdl2_draw_circle(pb_platform* p, int cx, int cy, int r,
                              pb_color_srgb8 color)
{
    sdl2_impl* impl = p->impl;
    SDL_SetRenderDrawColor(impl->renderer, color.r, color.g, color.b, color.a);

    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)SDL_sqrtf((float)(r * r - dy * dy));
        SDL_RenderDrawLine(impl->renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

/* Midpoint circle algorithm for outline */
static void sdl2_draw_circle_outline(pb_platform* p, int cx, int cy, int r,
                                      pb_color_srgb8 color)
{
    sdl2_impl* impl = p->impl;
    SDL_SetRenderDrawColor(impl->renderer, color.r, color.g, color.b, color.a);

    int x = r;
    int y = 0;
    int err = 1 - r;

    while (x >= y) {
        SDL_RenderDrawPoint(impl->renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(impl->renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(impl->renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(impl->renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(impl->renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(impl->renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(impl->renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(impl->renderer, cx + x, cy - y);

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x + 1);
        }
    }
}

static void sdl2_draw_line(pb_platform* p, int x1, int y1, int x2, int y2,
                            pb_color_srgb8 color)
{
    sdl2_impl* impl = p->impl;
    SDL_SetRenderDrawColor(impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(impl->renderer, x1, y1, x2, y2);
}

static void sdl2_present(pb_platform* p)
{
    sdl2_impl* impl = p->impl;
    SDL_RenderPresent(impl->renderer);
}

/*============================================================================
 * Texture Management
 *============================================================================*/

static pb_texture sdl2_texture_load(pb_platform* p, const char* path)
{
    sdl2_impl* impl = p->impl;

    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        SDL_Log("IMG_Load failed for %s: %s", path, IMG_GetError());
        return NULL;
    }

    SDL_Texture* sdl_tex = SDL_CreateTextureFromSurface(impl->renderer, surface);
    int w = surface->w;
    int h = surface->h;
    SDL_FreeSurface(surface);

    if (!sdl_tex) {
        SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        return NULL;
    }

    pb_texture tex = malloc(sizeof(*tex));
    if (!tex) {
        SDL_DestroyTexture(sdl_tex);
        return NULL;
    }

    tex->sdl_tex = sdl_tex;
    tex->width = w;
    tex->height = h;
    return tex;
}

static pb_texture sdl2_texture_create(pb_platform* p, int w, int h)
{
    sdl2_impl* impl = p->impl;

    SDL_Texture* sdl_tex = SDL_CreateTexture(
        impl->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        w, h
    );
    if (!sdl_tex) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return NULL;
    }

    pb_texture tex = malloc(sizeof(*tex));
    if (!tex) {
        SDL_DestroyTexture(sdl_tex);
        return NULL;
    }

    tex->sdl_tex = sdl_tex;
    tex->width = w;
    tex->height = h;
    return tex;
}

static void sdl2_texture_free(pb_platform* p, pb_texture tex)
{
    (void)p;
    if (tex) {
        if (tex->sdl_tex) {
            SDL_DestroyTexture(tex->sdl_tex);
        }
        free(tex);
    }
}

static void sdl2_texture_draw(pb_platform* p, pb_texture tex,
                               int src_x, int src_y, int src_w, int src_h,
                               int dst_x, int dst_y, int dst_w, int dst_h)
{
    if (!tex || !tex->sdl_tex) return;
    sdl2_impl* impl = p->impl;

    SDL_Rect src = {src_x, src_y, src_w, src_h};
    SDL_Rect dst = {dst_x, dst_y, dst_w, dst_h};

    /* Use full texture if src is zeroed */
    SDL_Rect* src_ptr = (src_w > 0 && src_h > 0) ? &src : NULL;

    SDL_RenderCopy(impl->renderer, tex->sdl_tex, src_ptr, &dst);
}

static void sdl2_texture_draw_ex(pb_platform* p, const pb_sprite* sprite)
{
    if (!sprite || !sprite->texture || !sprite->texture->sdl_tex) return;
    sdl2_impl* impl = p->impl;

    SDL_Rect src = {sprite->src_x, sprite->src_y, sprite->src_w, sprite->src_h};
    SDL_FRect dst = {sprite->dst_x, sprite->dst_y, sprite->dst_w, sprite->dst_h};
    SDL_FPoint center = {sprite->origin_x, sprite->origin_y};

    SDL_SetTextureColorMod(sprite->texture->sdl_tex,
                           sprite->tint.r, sprite->tint.g, sprite->tint.b);
    SDL_SetTextureAlphaMod(sprite->texture->sdl_tex, sprite->alpha);

    double angle_deg = sprite->rotation * 180.0 / 3.14159265358979;

    SDL_RenderCopyExF(impl->renderer, sprite->texture->sdl_tex,
                      &src, &dst, angle_deg, &center, SDL_FLIP_NONE);
}

/*============================================================================
 * Audio
 *============================================================================*/

static pb_sound sdl2_sound_load(pb_platform* p, const char* path)
{
    (void)p;
    Mix_Chunk* chunk = Mix_LoadWAV(path);
    if (!chunk) {
        SDL_Log("Mix_LoadWAV failed for %s: %s", path, Mix_GetError());
        return NULL;
    }

    pb_sound snd = malloc(sizeof(*snd));
    if (!snd) {
        Mix_FreeChunk(chunk);
        return NULL;
    }

    snd->chunk = chunk;
    return snd;
}

static void sdl2_sound_free(pb_platform* p, pb_sound snd)
{
    (void)p;
    if (snd) {
        if (snd->chunk) {
            Mix_FreeChunk(snd->chunk);
        }
        free(snd);
    }
}

static void sdl2_sound_play(pb_platform* p, pb_sound snd, float volume)
{
    (void)p;
    if (!snd || !snd->chunk) return;
    Mix_VolumeChunk(snd->chunk, (int)(volume * MIX_MAX_VOLUME));
    Mix_PlayChannel(-1, snd->chunk, 0);
}

static pb_music sdl2_music_load(pb_platform* p, const char* path)
{
    (void)p;
    Mix_Music* music = Mix_LoadMUS(path);
    if (!music) {
        SDL_Log("Mix_LoadMUS failed for %s: %s", path, Mix_GetError());
        return NULL;
    }

    pb_music mus = malloc(sizeof(*mus));
    if (!mus) {
        Mix_FreeMusic(music);
        return NULL;
    }

    mus->music = music;
    return mus;
}

static void sdl2_music_free(pb_platform* p, pb_music mus)
{
    (void)p;
    if (mus) {
        if (mus->music) {
            Mix_FreeMusic(mus->music);
        }
        free(mus);
    }
}

static void sdl2_music_play(pb_platform* p, pb_music mus, bool loop)
{
    (void)p;
    if (!mus || !mus->music) return;
    Mix_PlayMusic(mus->music, loop ? -1 : 1);
}

static void sdl2_music_stop(pb_platform* p)
{
    (void)p;
    Mix_HaltMusic();
}

static void sdl2_music_set_volume(pb_platform* p, float volume)
{
    (void)p;
    Mix_VolumeMusic((int)(volume * MIX_MAX_VOLUME));
}

/*============================================================================
 * Platform Creation
 *============================================================================*/

pb_platform* pb_platform_sdl2_create(void)
{
    pb_platform* p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    sdl2_impl* impl = calloc(1, sizeof(*impl));
    if (!impl) {
        free(p);
        return NULL;
    }

    p->impl = impl;

    /* Wire up vtable */
    p->init = sdl2_init;
    p->shutdown = sdl2_shutdown;
    p->should_quit = sdl2_should_quit;
    p->begin_frame = sdl2_begin_frame;
    p->end_frame = sdl2_end_frame;
    p->get_ticks_ms = sdl2_get_ticks_ms;
    p->delay = sdl2_delay;
    p->poll_input = sdl2_poll_input;
    p->clear = sdl2_clear;
    p->draw_rect = sdl2_draw_rect;
    p->draw_rect_outline = sdl2_draw_rect_outline;
    p->draw_circle = sdl2_draw_circle;
    p->draw_circle_outline = sdl2_draw_circle_outline;
    p->draw_line = sdl2_draw_line;
    p->present = sdl2_present;
    p->texture_load = sdl2_texture_load;
    p->texture_create = sdl2_texture_create;
    p->texture_free = sdl2_texture_free;
    p->texture_draw = sdl2_texture_draw;
    p->texture_draw_ex = sdl2_texture_draw_ex;
    p->sound_load = sdl2_sound_load;
    p->sound_free = sdl2_sound_free;
    p->sound_play = sdl2_sound_play;
    p->music_load = sdl2_music_load;
    p->music_free = sdl2_music_free;
    p->music_play = sdl2_music_play;
    p->music_stop = sdl2_music_stop;
    p->music_set_volume = sdl2_music_set_volume;

    return p;
}

void pb_platform_free(pb_platform* p)
{
    if (p) {
        free(p->impl);
        free(p);
    }
}

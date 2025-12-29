/*
 * pb_render.h - 8-bit/Voxel Style Renderer Abstraction
 *
 * Platform-independent rendering interface with emphasis on:
 *   - 8-bit color palettes (256 colors)
 *   - Pixel-perfect rendering
 *   - Voxel-style 3D effects (faux depth via layering)
 *   - Dithering patterns for gradients
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_RENDER_H
#define PB_RENDER_H

#include "pb_types.h"
#include "pb_color.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 8-Bit Palette System
 *============================================================================*/

/* Classic 8-bit palette size */
#define PB_PALETTE_SIZE 256

/* Predefined palette slots for game elements */
typedef enum pb_palette_slot {
    /* Core bubble colors (0-7) */
    PB_PAL_BUBBLE_RED = 0,
    PB_PAL_BUBBLE_ORANGE,
    PB_PAL_BUBBLE_YELLOW,
    PB_PAL_BUBBLE_GREEN,
    PB_PAL_BUBBLE_CYAN,
    PB_PAL_BUBBLE_BLUE,
    PB_PAL_BUBBLE_PURPLE,
    PB_PAL_BUBBLE_PINK,

    /* Bubble shades (8-23, 2 shades per color) */
    PB_PAL_BUBBLE_SHADES_START = 8,

    /* UI colors (24-31) */
    PB_PAL_UI_START = 24,
    PB_PAL_UI_BG = 24,
    PB_PAL_UI_FG,
    PB_PAL_UI_HIGHLIGHT,
    PB_PAL_UI_SHADOW,
    PB_PAL_UI_ACCENT,
    PB_PAL_UI_WARNING,
    PB_PAL_UI_SUCCESS,
    PB_PAL_UI_ERROR,

    /* Background gradients (32-47) */
    PB_PAL_BG_START = 32,

    /* Effects/particles (48-63) */
    PB_PAL_EFFECT_START = 48,

    /* Grayscale ramp (240-255) */
    PB_PAL_GRAY_START = 240

} pb_palette_slot;

/**
 * 8-bit palette entry (indexed color).
 */
typedef struct pb_palette_entry {
    uint8_t r, g, b;
    uint8_t flags;  /* Reserved for transparency, animation, etc. */
} pb_palette_entry;

/**
 * Complete 256-color palette.
 */
typedef struct pb_palette {
    pb_palette_entry colors[PB_PALETTE_SIZE];
    char name[32];
} pb_palette;

/**
 * Generate default game palette.
 */
void pb_palette_init_default(pb_palette* pal);

/**
 * Generate CGA-style palette (16 colors extended).
 */
void pb_palette_init_cga(pb_palette* pal);

/**
 * Generate EGA-style palette (64 colors).
 */
void pb_palette_init_ega(pb_palette* pal);

/**
 * Generate NES-style palette (~56 colors).
 */
void pb_palette_init_nes(pb_palette* pal);

/**
 * Generate Game Boy palette (4 greens).
 */
void pb_palette_init_gameboy(pb_palette* pal);

/**
 * Apply CVD simulation to entire palette.
 */
void pb_palette_apply_cvd(pb_palette* pal, pb_cvd_type type, float severity);

/*============================================================================
 * Dithering Patterns
 *============================================================================*/

/**
 * Dither pattern types for gradients and shading.
 */
typedef enum pb_dither_type {
    PB_DITHER_NONE = 0,
    PB_DITHER_BAYER2X2,
    PB_DITHER_BAYER4X4,
    PB_DITHER_BAYER8X8,
    PB_DITHER_ORDERED,
    PB_DITHER_HALFTONE,
    PB_DITHER_CHECKERBOARD,
    PB_DITHER_NOISE
} pb_dither_type;

/**
 * Get dithered color index for a position and blend factor.
 *
 * @param x, y      Screen position
 * @param color_a   First palette index
 * @param color_b   Second palette index
 * @param blend     Blend factor (0.0 = color_a, 1.0 = color_b)
 * @param type      Dither pattern type
 * @return          Palette index to use at this position
 */
uint8_t pb_dither_blend(int x, int y, uint8_t color_a, uint8_t color_b,
                        pb_scalar blend, pb_dither_type type);

/*============================================================================
 * Sprite/Tile System
 *============================================================================*/

/**
 * Sprite flags.
 */
typedef enum pb_sprite_flags {
    PB_SPRITE_FLIP_H = 0x01,
    PB_SPRITE_FLIP_V = 0x02,
    PB_SPRITE_ROTATE_90 = 0x04,
    PB_SPRITE_ROTATE_180 = 0x08,
    PB_SPRITE_HIDDEN = 0x10,
    PB_SPRITE_PRIORITY = 0x20  /* Render on top layer */
} pb_sprite_flags;

/**
 * 8-bit indexed sprite (palette-based).
 * Different from pb_sprite in pb_platform.h which is texture-based.
 */
typedef struct pb_indexed_sprite {
    uint8_t* pixels;         /* Palette indices (width * height) */
    int width;
    int height;
    uint8_t transparent;     /* Transparent palette index (usually 0) */
} pb_indexed_sprite;

/**
 * Indexed sprite instance for rendering.
 */
typedef struct pb_indexed_sprite_instance {
    const pb_indexed_sprite* sprite;
    int x, y;                /* Screen position */
    int layer;               /* Z-order (voxel depth layer) */
    uint8_t flags;           /* pb_sprite_flags */
    uint8_t palette_offset;  /* Add to all sprite palette indices */
} pb_indexed_sprite_instance;

/*============================================================================
 * Voxel-Style Depth Layers
 *============================================================================*/

/* Maximum depth layers for pseudo-3D effect */
#define PB_MAX_LAYERS 8

/**
 * Layer configuration for voxel-style depth.
 */
typedef struct pb_layer_config {
    int y_offset;            /* Vertical offset (parallax) */
    pb_scalar scale;         /* Scale factor (1.0 = normal) */
    uint8_t shadow_color;    /* Shadow palette index for this layer */
    uint8_t ambient;         /* Ambient light level (0-255) */
} pb_layer_config;

/*============================================================================
 * Render Context
 *============================================================================*/

/**
 * Render configuration.
 */
typedef struct pb_render_config {
    int width;               /* Logical width (before scaling) */
    int height;              /* Logical height */
    int scale;               /* Integer scale factor (1, 2, 3, 4...) */
    bool scanlines;          /* Enable CRT scanline effect */
    bool pixel_perfect;      /* Force integer scaling */
    pb_dither_type dither;   /* Default dither pattern */
} pb_render_config;

/**
 * Render statistics.
 */
typedef struct pb_render_stats {
    int sprites_drawn;
    int tiles_drawn;
    int pixels_filled;
    int draw_calls;
    float frame_time_ms;
} pb_render_stats;

/**
 * Abstract render context (implemented by platform backend).
 */
typedef struct pb_render_context pb_render_context;

/*============================================================================
 * Render Context Lifecycle
 *============================================================================*/

/**
 * Create render context with given configuration.
 * Returns NULL on failure.
 */
pb_render_context* pb_render_create(const pb_render_config* config);

/**
 * Destroy render context and free resources.
 */
void pb_render_destroy(pb_render_context* ctx);

/**
 * Set active palette for rendering.
 */
void pb_render_set_palette(pb_render_context* ctx, const pb_palette* pal);

/**
 * Get current render configuration.
 */
const pb_render_config* pb_render_get_config(const pb_render_context* ctx);

/*============================================================================
 * Frame Operations
 *============================================================================*/

/**
 * Begin a new frame.
 */
void pb_render_begin_frame(pb_render_context* ctx);

/**
 * Clear screen with palette index.
 */
void pb_render_clear(pb_render_context* ctx, uint8_t color);

/**
 * End frame and present to display.
 */
void pb_render_end_frame(pb_render_context* ctx);

/**
 * Get last frame's render statistics.
 */
pb_render_stats pb_render_get_stats(const pb_render_context* ctx);

/*============================================================================
 * Primitive Drawing
 *============================================================================*/

/**
 * Draw a single pixel.
 */
void pb_render_pixel(pb_render_context* ctx, int x, int y, uint8_t color);

/**
 * Draw horizontal line.
 */
void pb_render_hline(pb_render_context* ctx, int x, int y, int width, uint8_t color);

/**
 * Draw vertical line.
 */
void pb_render_vline(pb_render_context* ctx, int x, int y, int height, uint8_t color);

/**
 * Draw line between two points.
 */
void pb_render_line(pb_render_context* ctx, int x1, int y1, int x2, int y2,
                    uint8_t color);

/**
 * Draw rectangle outline.
 */
void pb_render_rect(pb_render_context* ctx, int x, int y, int w, int h,
                    uint8_t color);

/**
 * Draw filled rectangle.
 */
void pb_render_rect_fill(pb_render_context* ctx, int x, int y, int w, int h,
                         uint8_t color);

/**
 * Draw circle outline.
 */
void pb_render_circle(pb_render_context* ctx, int cx, int cy, int radius,
                      uint8_t color);

/**
 * Draw filled circle.
 */
void pb_render_circle_fill(pb_render_context* ctx, int cx, int cy, int radius,
                           uint8_t color);

/*============================================================================
 * Sprite Drawing
 *============================================================================*/

/**
 * Draw indexed sprite at position.
 */
void pb_render_indexed_sprite(pb_render_context* ctx,
                               const pb_indexed_sprite_instance* inst);

/**
 * Draw indexed sprite batch (sorted by layer for depth).
 */
void pb_render_indexed_sprite_batch(pb_render_context* ctx,
                                     const pb_indexed_sprite_instance* sprites,
                                     int count);

/*============================================================================
 * Bubble-Specific Rendering
 *============================================================================*/

/**
 * Draw a bubble with 8-bit shading.
 * Renders with highlight, base color, and shadow for 3D appearance.
 *
 * @param ctx      Render context
 * @param cx, cy   Center position
 * @param radius   Bubble radius
 * @param color    Base color palette index (uses color+1, color+2 for shading)
 */
void pb_render_bubble(pb_render_context* ctx, int cx, int cy, int radius,
                      uint8_t color);

/**
 * Draw a bubble with pattern overlay (for accessibility).
 *
 * @param ctx      Render context
 * @param cx, cy   Center position
 * @param radius   Bubble radius
 * @param color    Base color palette index
 * @param pattern  Pattern type (0 = solid, 1 = stripes, 2 = dots, etc.)
 */
void pb_render_bubble_pattern(pb_render_context* ctx, int cx, int cy, int radius,
                              uint8_t color, int pattern);

/*============================================================================
 * Background/Parallax
 *============================================================================*/

/**
 * Draw tiled background.
 */
void pb_render_background_tiled(pb_render_context* ctx,
                                 const pb_indexed_sprite* tile,
                                 int offset_x, int offset_y);

/**
 * Draw gradient background with dithering.
 */
void pb_render_background_gradient(pb_render_context* ctx, uint8_t color_top,
                                    uint8_t color_bottom, pb_dither_type dither);

/*============================================================================
 * Post-Processing Effects
 *============================================================================*/

/**
 * Apply CRT scanline effect.
 */
void pb_render_effect_scanlines(pb_render_context* ctx, int intensity);

/**
 * Apply color aberration effect.
 */
void pb_render_effect_aberration(pb_render_context* ctx, int offset);

/**
 * Apply vignette effect.
 */
void pb_render_effect_vignette(pb_render_context* ctx, pb_scalar strength);

#ifdef __cplusplus
}
#endif

#endif /* PB_RENDER_H */

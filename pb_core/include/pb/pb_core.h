/*
 * pb_core.h - Puzzle Bobble Core Library
 *
 * Clean-room implementation of Puzzle Bobble game logic.
 * Platform-independent C library with optional fixed-point mode.
 *
 * Supports: C17 (default) -> C11 -> C99 -> C89 fallback
 * Size tiers: full (desktop) -> medium (embedded) -> mini (16-bit) -> micro (8-bit)
 *
 * Include this header for access to all pb_core functionality.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PB_CORE_H
#define PB_CORE_H

/* Build configuration and size tiers - MUST come first */
#include "pb_config.h"

/* C standard compatibility shims */
#include "pb_compat.h"

/* Core type definitions */
#include "pb_types.h"

/* Hexagonal grid mathematics */
#include "pb_hex.h"

/* Deterministic RNG */
#include "pb_rng.h"

/* Board operations and traversal */
#include "pb_board.h"

/* Shot physics and collision */
#include "pb_shot.h"

/* Trajectory prediction for aiming */
#include "pb_trajectory.h"

/* Bubble effect system */
#include "pb_effect.h"

/* Game state controller */
#include "pb_game.h"

/* Perceptual color space (Oklab/OKLCH) */
#include "pb_color.h"

/* Color Vision Deficiency simulation */
#include "pb_cvd.h"

/* Pattern overlays for accessibility */
#include "pb_pattern.h"

/* JSON data loading (levels, themes, rulesets, replays) */
#include "pb_data.h"

/* Deterministic replay recording and playback */
#include "pb_replay.h"

/* CRC-32 checksumming for sync verification */
#include "pb_checksum.h"

/* Game session with replay integration */
#include "pb_session.h"

/* Platform abstraction (SDL2, etc.) */
#include "pb_platform.h"

/* Level solver and validator */
#include "pb_solver.h"

/* A* and JPS pathfinding for hex grids */
#include "pb_path.h"

/* 8-bit/voxel style renderer abstraction */
#include "pb_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Library Version
 *============================================================================*/

#define PB_VERSION_MAJOR 0
#define PB_VERSION_MINOR 2
#define PB_VERSION_PATCH 0

/**
 * Get library version as packed integer (MAJOR << 16 | MINOR << 8 | PATCH).
 */
static inline uint32_t pb_version(void)
{
    return (PB_VERSION_MAJOR << 16) | (PB_VERSION_MINOR << 8) | PB_VERSION_PATCH;
}

/**
 * Get library version string.
 */
static inline const char* pb_version_string(void)
{
    return "0.2.0";
}

#ifdef __cplusplus
}
#endif

#endif /* PB_CORE_H */

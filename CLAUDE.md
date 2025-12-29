# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This repository contains multiple Puzzle Bobble/Bust-a-Move implementations across different languages and platforms, with **pb_core** as the primary actively developed component - a clean-room C library implementing deterministic game logic.

## Build Commands (pb_core)

```bash
# Build library
make -C pb_core

# Run tests (306 tests)
make -C pb_core test

# Fixed-point build (deterministic multiplayer)
FIXED_POINT=1 make -C pb_core test

# Debug build with sanitizers
DEBUG=1 make -C pb_core test

# Build SDL2 demo (requires SDL2, SDL2_image, SDL2_mixer)
make -C pb_core demo

# Build CLI tools and examples
make -C pb_core tools examples

# Static analysis
make -C pb_core audit    # Full analysis (lint + check + complexity)
make -C pb_core lint     # Quick cppcheck

# Cross-compilation testing
make -C pb_core cross-check-all    # Test all available cross-compilers
make -C pb_core check-all-standards  # Test C89/C99/C11/C17
```

## Architecture

### pb_core Library Structure

The library is organized into dependency layers:

1. **Core logic** (`src/core/`) - Platform-agnostic C17, no external dependencies
   - `pb_hex` - Hexagonal grid math (offset/axial/cube coordinates)
   - `pb_board` - Game board operations, bubble matching, orphan detection
   - `pb_shot` - Shot physics and collision detection
   - `pb_game` - Game state controller
   - `pb_trajectory` - Aiming line prediction
   - `pb_effect` - Special bubble effects
   - `pb_rng` - Deterministic RNG
   - `pb_checksum` - CRC-32 verification
   - `pb_replay` - Binary replay format with varint encoding
   - `pb_session` - High-level game session management
   - `pb_solver` - Level validation and difficulty estimation
   - `pb_color` - Oklab/OKLCH perceptual color space
   - `pb_cvd` - Color vision deficiency simulation

2. **Data loading** (`src/data/`) - JSON parsing via vendored cJSON
   - `pb_data` - Level and theme loading from JSON

3. **Platform abstraction** (`src/platform/`) - Optional SDL2 backend
   - `pb_platform` - vtable-based backend interface
   - `pb_sdl2` - SDL2 implementation

### Key Design Decisions

- **Determinism**: Fixed-point Q16.16 arithmetic (`FIXED_POINT=1`) enables bit-perfect replay and multiplayer
- **Portability**: Targets 8-bit to 64-bit architectures with size tiers (full/medium/mini/micro)
- **Accessibility**: CVD simulation for protanopia/deuteranopia/tritanopia, WCAG contrast checking
- **No hidden state**: All game state in explicit structs, no globals

### Test Framework

Custom macro-based framework in `tests/`:
```c
TEST(test_name) {
    ASSERT(condition);
    ASSERT_EQ(actual, expected);
}

int main(void) {
    RUN_TEST(test_name);
    return test_failed ? 1 : 0;
}
```

Run a single test file: `make -C pb_core dirs lib && gcc -std=c17 -Wall -Wextra -Ipb_core/include pb_core/tests/test_hex.c -Lpb_core/build/lib -lpb_core -lm -o test_hex && ./test_hex`

### Configuration Options

| Variable | Values | Description |
|----------|--------|-------------|
| `C_STD` | c17, c11, c99, c89 | C standard |
| `SIZE_TIER` | full, medium, mini, micro | Feature/memory tradeoff |
| `FIXED_POINT` | 1 | Enable Q16.16 fixed-point |
| `DEBUG` | 1 | Enable sanitizers |

## Other Implementations

Reference implementations are tracked as git submodules in `refs/`:
- `puzzle-bobble-pure-c/`, `puzzle-bobble-sdl/` - C with SDL
- `puzzle-bobble-cpp/`, `km-bubble-shooter/` - C++ with SDL
- `puzzle-bobble-js/` - Browser JavaScript
- `graph-bubble-shooter/` - Java LibGDX
- `jtbubl-fpga/` - FPGA Verilog/VHDL

## Colorblindness Submodule

CVD research tools in `colorblindness/`:
```bash
make -C colorblindness serve          # GTK4 template
make -C colorblindness oklch          # OKLCH token generation
make -C colorblindness contrast-check # WCAG contrast analysis
```

## JSON Data Formats

Level and theme schemas in `pb_core/schemas/`:
- `level.schema.json` - Grid layout, bubble positions, objectives
- `theme.schema.json` - Color palette, backgrounds
- `ruleset.schema.json` - Game mode configuration

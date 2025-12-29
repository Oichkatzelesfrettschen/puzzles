# pb_core - Clean-Room Puzzle Bobble Core Library

A portable, deterministic C library implementing Puzzle Bobble (Bust-a-Move) game logic.
No platform dependencies in the core - works anywhere C17 compiles.

## Features

- **Hexagonal grid math** - Offset/axial/cube coordinate conversions, neighbor lookups
- **Deterministic physics** - Optional Q16.16 fixed-point for bit-perfect multiplayer
- **Bubble mechanics** - Match detection, orphan dropping, special bubble effects
- **Replay system** - Binary format with varint encoding, CRC-32 checksums
- **Accessibility** - CVD simulation (protanopia/deuteranopia/tritanopia), WCAG contrast
- **Level validation** - Solver, difficulty estimation, solvability analysis
- **Platform abstraction** - vtable-based backend for SDL2, custom platforms

## Build

```bash
# Standard build (float mode)
make

# Fixed-point build (deterministic multiplayer)
FIXED_POINT=1 make

# Debug build with sanitizers
DEBUG=1 make

# Run tests (306 tests)
make test

# Build SDL2 demo (requires SDL2, SDL2_image, SDL2_mixer)
make demo

# Build tools (pb_validate)
make tools

# Build examples
make examples
```

## Architecture

```
pb_core/
├── include/pb/           # Public headers
│   ├── pb_core.h         # Master include (includes all)
│   ├── pb_types.h        # Core types, fixed-point math
│   ├── pb_hex.h          # Hexagonal coordinate math
│   ├── pb_board.h        # Game board operations
│   ├── pb_game.h         # Game state controller
│   ├── pb_shot.h         # Shot physics, collision
│   ├── pb_effect.h       # Special bubble effects
│   ├── pb_replay.h       # Replay recording/playback
│   ├── pb_session.h      # High-level game session
│   ├── pb_color.h        # Oklab/OKLCH color space
│   ├── pb_cvd.h          # Color vision deficiency
│   ├── pb_pattern.h      # Pattern overlay system
│   ├── pb_solver.h       # Level validation/solving
│   ├── pb_data.h         # JSON level/theme loading
│   └── pb_platform.h     # Platform abstraction
├── src/
│   ├── core/             # Core logic (no dependencies)
│   ├── data/             # JSON parsing (cJSON)
│   ├── platform/         # SDL2 backend
│   └── vendor/           # Third-party (cJSON)
├── tests/                # Test suite (306 tests)
├── tools/                # CLI tools (pb_validate)
├── examples/             # Standalone examples
├── levels/               # Example levels
└── schemas/              # JSON schemas
```

## Examples

Build and run the standalone examples:

```bash
make examples
./build/bin/minimal_game    # Console game simulation
./build/bin/replay_demo     # Replay recording and playback
./build/bin/cvd_palette     # Color accessibility analysis
```

| Example | Description |
|---------|-------------|
| `minimal_game` | Basic game loop with board display, shot firing, and score tracking |
| `replay_demo` | Record a game session, save to file, reload and verify determinism |
| `cvd_palette` | Analyze colors for accessibility across vision types |

## Quick Start

### Minimal Game Loop

```c
#include "pb/pb_core.h"

int main(void) {
    // Initialize game with default puzzle mode
    pb_game_state game;
    pb_game_init(&game, NULL, 12345);  // seed = 12345

    // Load a level
    pb_bubble bubbles[] = { /* ... */ };
    pb_game_load_board(&game, bubbles, 5);

    // Game loop
    while (!pb_game_is_over(&game)) {
        // Rotate cannon
        pb_game_rotate(&game, PB_FLOAT_TO_FIXED(0.02f));

        // Fire when ready
        if (should_fire) {
            pb_game_fire(&game);
        }

        // Advance simulation
        pb_game_tick(&game);
    }

    if (pb_game_is_won(&game)) {
        printf("Victory! Score: %u\n", game.score);
    }

    return 0;
}
```

### Session with Replay Recording

```c
#include "pb/pb_core.h"

int main(void) {
    pb_session_config config = {
        .mode = PB_SESSION_RECORDING,
        .seed = time(NULL),
        .checkpoint_interval = 300  // Every 5 seconds at 60fps
    };

    pb_session session;
    pb_session_create(&session, &config);

    // Play game...
    pb_session_fire(&session);
    pb_session_tick(&session);

    // Save replay
    pb_session_finalize(&session);
    pb_replay* replay = pb_session_extract_replay(&session);
    pb_replay_save_file(replay, "replay.pbr");
    pb_replay_free(replay);

    pb_session_destroy(&session);
    return 0;
}
```

### Level Validation

```c
#include "pb/pb_core.h"

int main(int argc, char** argv) {
    pb_level_data level;
    pb_data_result result;

    if (!pb_level_load_file(argv[1], &level, &result)) {
        fprintf(stderr, "Error: %s\n", result.error);
        return 1;
    }

    pb_board board;
    pb_level_to_board(&level, &board);

    // Validate
    pb_validation_info info;
    if (pb_validate_level(&board, NULL, 0xFF, &info) != PB_VALID) {
        printf("Invalid: %s\n", info.message);
        return 1;
    }

    // Estimate difficulty
    pb_difficulty_info diff;
    pb_estimate_difficulty(&board, NULL, &diff);
    printf("Difficulty: %s (%d moves estimated)\n",
           diff.notes, diff.estimated_moves);

    pb_level_data_free(&level);
    return 0;
}
```

## API Reference

### Core Types (pb_types.h)

| Type | Description |
|------|-------------|
| `pb_scalar` | float or Q16.16 fixed-point |
| `pb_offset` | Row/column coordinates |
| `pb_axial` | Axial hex coordinates (q,r) |
| `pb_bubble` | Bubble with kind, color, special |
| `pb_board` | Game board with cells |
| `pb_game_state` | Complete game state |
| `pb_ruleset` | Game mode configuration |

### Fixed-Point Macros

```c
PB_FLOAT_TO_FIXED(x)  // float -> pb_scalar
PB_FIXED_TO_FLOAT(x)  // pb_scalar -> float
PB_FIXED_MUL(a, b)    // Fixed-point multiply
PB_FIXED_DIV(a, b)    // Fixed-point divide
PB_SCALAR_SIN(x)      // Sine (auto-converts)
PB_SCALAR_COS(x)      // Cosine (auto-converts)
PB_SCALAR_SQRT(x)     // Square root
```

### Game Modes (pb_ruleset)

| Mode | Description |
|------|-------------|
| `PB_MODE_PUZZLE` | Fixed board, no pressure, no swap |
| `PB_MODE_ARCADE` | New rows every N shots |
| `PB_MODE_SURVIVAL` | Aggressive row insertion |
| `PB_MODE_TIME_ATTACK` | Time-limited scoring |
| `PB_MODE_VERSUS` | Competitive with garbage |
| `PB_MODE_COOP` | Shared board multiplayer |
| `PB_MODE_ZEN` | Practice mode, no pressure |

### Color Vision Deficiency (pb_cvd.h)

```c
// Simulate how a color appears to someone with CVD
pb_color_oklab simulated = pb_cvd_simulate(original, PB_CVD_PROTAN, 1.0f);

// Check if two colors are distinguishable for all vision types
pb_cvd_analysis analysis;
pb_cvd_analyze_pair(color_a, color_b, 0.04f, &analysis);
if (analysis.protan_pass && analysis.deutan_pass) {
    // Colors work for red-green colorblind users
}

// Analyze entire palette
pb_color_srgb8 palette[] = { ... };
pb_cvd_palette_result result;
pb_cvd_analyze_palette(palette, 8, 0.04f, &result);
```

### Replay System (pb_replay.h)

```c
// Record
pb_replay* replay = pb_replay_create(seed);
pb_replay_event event = { .type = PB_REPLAY_FIRE, .frame = 120, .angle = angle };
pb_replay_record_event(replay, &event);

// Save/Load
pb_replay_save_file(replay, "game.pbr");
pb_replay* loaded = pb_replay_load_file("game.pbr");

// Playback
pb_playback* pb = pb_playback_create(replay);
pb_playback_set_speed(pb, 2.0f);  // 2x speed
while (pb_playback_has_events(pb)) {
    pb_replay_event event;
    pb_playback_get_next(pb, &event);
    // Apply to game...
}
```

## JSON Formats

### Level Format

```json
{
    "version": "1.0",
    "name": "Level 1",
    "author": "Designer",
    "difficulty": 3,
    "grid": {
        "cols_even": 8, "cols_odd": 7, "rows": 12,
        "bubbles": [
            {"color": 0}, {"color": 1}, {"color": 0}, null, ...
        ]
    },
    "objectives": {
        "clear_all": true,
        "score": 10000,
        "moves": 30
    }
}
```

### Theme Format

```json
{
    "version": "1.0",
    "name": "Classic",
    "palette": [
        {"color": "#FF0000", "pattern": "solid"},
        {"color": "#00FF00", "pattern": "stripes"},
        {"color": "#0000FF", "pattern": "dots"}
    ],
    "backgrounds": ["#1a1a2e", "#16213e"]
}
```

## Tests

The library includes 306 tests across 15 test files:

- Hex coordinate math (21 tests)
- Board operations and matching (24 tests)
- Shot physics and collision (17 tests)
- Game controller logic (23 tests)
- Replay serialization (17 tests)
- Session management (22 tests)
- Checksum verification (17 tests)
- Color calculations (28 tests)
- CVD simulation and analysis (28 tests)
- Data loading (13 tests)
- Solver/validation (17 tests)
- Effects system (21 tests)
- Trajectory computation (19 tests)
- RNG and shuffle (19 tests)
- Pattern overlays (20 tests)

Run with:
```bash
make test                    # Float mode
FIXED_POINT=1 make test      # Fixed-point mode
DEBUG=1 make test            # With sanitizers
```

## Platform Compatibility

### 64-bit Architectures (Full Support)

| Architecture | Compiler | Status | Notes |
|--------------|----------|--------|-------|
| x86-64 | GCC 14+ | ✓ Full | Primary development platform |
| AArch64 | aarch64-linux-gnu-gcc | ✓ Full | ARM64 Linux, Apple Silicon |
| RISC-V 64 | riscv64-elf-gcc | ⚠ Needs libc | Bare-metal needs newlib |

### 32-bit Architectures

| Architecture | Compiler | Status | Notes |
|--------------|----------|--------|-------|
| x86 (i386) | GCC -m32 | ✓ Full | 32-bit Linux |
| ARM Cortex-M | arm-none-eabi-gcc | ✓ Full | Raspberry Pi Pico, STM32 |
| SuperH (Casio) | sh-elf-gcc | ⚠ Needs libc | Casio fx-CG50/fx-9860GII |
| M68K | m68k-elf-gcc | ⏳ Untested | Amiga, Sega Genesis |
| ESP32/Xtensa | xtensa-esp32-elf-gcc | ⏳ Untested | Espressif IoT modules |

### 16-bit Architectures

| Architecture | Compiler | Status | Notes |
|--------------|----------|--------|-------|
| 8086/DOS | ia16-elf-gcc | ✗ Data too large | pb_game_state > 64KB segment |
| MSP430 | msp430-elf-gcc | ⏳ Untested | TI LaunchPad |
| 6809/6309 | lwtools | N/A | Assembler only (no C compiler) |

### 8-bit Architectures

| Architecture | Compiler | Status | Notes |
|--------------|----------|--------|-------|
| AVR | avr-gcc | ⏳ Needs install | Arduino, ATmega series |
| Z80 | SDCC | ✗ ICE | Internal compiler error on pb_hex.c |
| Z80 | z88dk (sccz80) | ✗ C99 | No designated initializers |
| 6502 | cc65 | ✗ Data too large | pb_game_state > 64KB |
| 8051 | SDCC | ✗ C99 | Compound literals not implemented |
| STM8 | SDCC | ✗ C99 | Compound literals not implemented |
| Game Boy | gbdk-2020 | ⏳ Untested | GB/GBC homebrew |

**Legend:**
- ✓ Full: Compiles and all tests pass
- ⚠ Needs libc: Requires newlib/libc headers
- ✗ Data too large: pb_game_state (~500KB) exceeds architecture limits
- ✗ C99/ICE: Compiler doesn't support required C features or has bugs
- ⏳ Untested: Toolchain available but not yet tested

### Toolchain Availability (Arch Linux)

| Toolchain | Package | Repo | Notes |
|-----------|---------|------|-------|
| avr-gcc | avr-gcc, avr-libc | extra | Complete toolchain |
| arm-none-eabi-gcc | arm-none-eabi-gcc, arm-none-eabi-newlib | extra | Complete toolchain |
| riscv64-elf-gcc | riscv64-elf-gcc, riscv64-elf-newlib | extra | Complete toolchain |
| sh-elf-gcc | sh-elf-gcc, sh-elf-newlib | AUR | Orphaned packages |
| m68k-elf-gcc | m68k-elf-gcc | AUR | |
| xtensa-esp32-elf-gcc | xtensa-esp32-elf-gcc | AUR | Orphaned |
| ia16-elf-gcc | gcc-ia16 | AUR | |
| msp430-elf-gcc | msp430-elf-gcc | AUR | |
| z88dk | z88dk | AUR | |
| lwtools | lwtools | AUR | 6809/6309 assembler |
| SDCC | sdcc | extra | Z80, 8051, STM8 |
| cc65 | cc65 | AUR | 6502 family |
| gbdk-2020 | gbdk-2020 | AUR | Game Boy |

### 8/16-bit Targets (Future pb_core_mini)

For 8-bit and 16-bit targets, a stripped-down "pb_core_mini" variant would be needed:
- Reduced board size (8x8 instead of 32x16)
- Smaller event buffer (16 vs 256 events)
- No JSON parsing (static level data)
- Fixed-point only (no floating point)
- C89-compatible (no compound literals)

## License

BSD-3-Clause. See individual files for details.

cJSON (vendored) is MIT licensed.

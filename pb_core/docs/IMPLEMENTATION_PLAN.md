# pb_core Implementation Plan

## Dependency Graph

```
                    ┌─────────────────┐
                    │  JSON Schema    │
                    │  (data format)  │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
    ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
    │  pb_data    │  │   Oklab     │  │  Behavioral │
    │ JSON parser │  │ CVD colors  │  │    Spec     │
    └──────┬──────┘  └──────┬──────┘  └──────┬──────┘
           │                │                │
           └────────────────┼────────────────┘
                            │
              ┌─────────────┼─────────────┐
              │             │             │
              ▼             ▼             ▼
    ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
    │   Replay    │  │    SDL2     │  │   Solver    │
    │   System    │  │   Backend   │  │  Validator  │
    └─────────────┘  └─────────────┘  └─────────────┘
                            │
                            ▼
                    ┌─────────────────┐
                    │  Documentation  │
                    └─────────────────┘
```

## Phase 1: Data Foundation (JSON Schema + Parser)

### 1.1 JSON Schema Design
- Version field in all documents (`"version": "1.0"`)
- Level format: grid, bubbles, metadata, objectives
- Ruleset format: mode parameters, allowed specials
- Theme format: palette, patterns, visual mapping
- Replay format: seed, events, checksums

### 1.2 JSON Parser (cJSON integration)
- Single-file cJSON for portability
- Level loader: `pb_level_load()`
- Theme loader: `pb_theme_load()`
- Replay serialization: `pb_replay_save/load()`

## Phase 2: Color Pipeline (Oklab + CVD)

### 2.1 Oklab Implementation
- sRGB ↔ linear RGB (gamma)
- linear RGB ↔ Oklab (via LMS)
- Oklab ↔ Oklch (cylindrical)
- Gamut clipping for out-of-range colors

### 2.2 CVD Simulation
- Brettel 1997 algorithm (accurate)
- Protanopia, Deuteranopia, Tritanopia matrices
- Severity parameter (0.0-1.0)
- Minimum contrast verification

### 2.3 Palette Generation
- Perceptual distance in Oklab (ΔE)
- Auto-select CVD-safe colors
- Pattern overlay for additional discrimination

## Phase 3: Event System (Replay + Multiplayer)

### 3.1 Event Log Architecture
- Compact binary format: `[frame:u32][type:u8][data:var]`
- FNV-1a checksum per N frames
- Transport-independent: works for file replay or network

### 3.2 Replay System
- Record: seed + events → file
- Playback: deterministic re-execution
- Seek: checkpoint + replay from checkpoint
- Verification: compare checksums

### 3.3 Multiplayer Foundation
- Input-only synchronization (lockstep-ready)
- Checksum comparison for desync detection
- Event serialization for network transport

## Phase 4: Platform Layer (SDL2)

### 4.1 Reference Backend
- Window/renderer management
- Input mapping (keyboard, mouse, touch)
- Audio hooks (callback-based)
- Frame timing (fixed 60 fps tick)

### 4.2 Rendering Abstraction
- `pb_render_board()` - hex grid drawing
- `pb_render_shot()` - trajectory preview
- `pb_render_ui()` - score, preview bubbles

## Phase 5: Validation (Solver + Tests)

### 5.1 Level Solver
- BFS/DFS perfect-play search
- Solvability verification
- Difficulty estimation (min shots)

### 5.2 Test Suite Expansion
- Property-based tests (QuickCheck-style)
- Replay determinism verification
- Cross-platform checksum validation

## Implementation Order

| Order | Component | Est. LOC | Dependencies |
|-------|-----------|----------|--------------|
| 1 | JSON Schema | ~200 | None |
| 2 | cJSON integration | ~100 | Schema |
| 3 | pb_data loaders | ~400 | cJSON |
| 4 | Oklab color math | ~300 | None |
| 5 | CVD simulation | ~200 | Oklab |
| 6 | Palette generator | ~200 | CVD |
| 7 | Behavioral spec | ~150 | Core complete |
| 8 | Event serialization | ~250 | Core complete |
| 9 | Replay system | ~300 | Events |
| 10 | SDL2 backend | ~600 | Replay |
| 11 | Level solver | ~400 | All above |
| 12 | Extended tests | ~500 | All above |
| 13 | API documentation | ~300 | All above |

**Total estimated: ~4,000 additional LOC**

## Research Sources

### JSON Schema & Parsing
- [cJSON](https://github.com/DaveGamble/cJSON) - Ultralightweight JSON parser
- [yyjson](https://github.com/ibireme/yyjson) - Fastest JSON library in C
- [JSON Schema Spec](https://json-schema.org/specification)

### Color Science
- [Oklab](https://bottosson.github.io/posts/oklab/) - Perceptual color space
- [DaltonLens CVD Review](https://daltonlens.org/opensource-cvd-simulation/)
- Brettel et al. 1997 - Dichromacy simulation algorithm

### Networking & Replay
- [Gaffer on Games: Deterministic Lockstep](https://gafferongames.com/post/deterministic_lockstep/)
- [Preparing for Deterministic Netcode](https://yal.cc/preparing-your-game-for-deterministic-netcode/)

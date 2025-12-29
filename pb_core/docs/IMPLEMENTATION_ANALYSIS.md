# Puzzle Bobble Implementation Analysis & Feature Synthesis

This document synthesizes game mechanics from 10 reference implementations for cleanroom integration into pb_core.

## Implementations Analyzed

| Implementation | Language | Key Strengths |
|----------------|----------|---------------|
| puzzle-bobble-pure-c | C/SDL | Simple baseline |
| puzzle-bobble-sdl | C/SDL | BFS matching, hex adjacency |
| puzzle-bobble-cpp | C++/SDL | Multiplayer, DFS matching, garbage mechanics |
| km-bubble-shooter | C++/SDL | Micro-step collision, quantifier scoring |
| puzzle-bobble-js | JavaScript | Clean flood-fill, timer-based progression |
| graph-bubble-shooter | Java | Explicit graph structure, BFS/DFS separation |
| bustamove-np | CoffeeScript | Trajectory preview, state serialization |
| frozen-bubble | Perl | Mature production code, chain reactions, multiplayer |
| jtbubl-fpga | Verilog | Hardware timing, memory-mapped architecture |
| hp48-puzzle-bobble | Assembly | Fixed-point math, bitwise collision |

---

## 1. Physics & Movement

### Ball Velocity

| Implementation | Speed | Unit | Formula |
|----------------|-------|------|---------|
| puzzle-bobble-sdl | 2 | px/frame | `pos += vel * cos/sin(angle)` |
| puzzle-bobble-cpp | 5 | px/frame | `vx = 5*sin(angle), vy = -5*cos(angle)` |
| km-bubble-shooter | 3000 | px/sec | Delta-time: `pos += vel * dt` |
| puzzle-bobble-js | 10 | px/frame | `move(10)` in angle direction |
| frozen-bubble | 10 | px/frame | `x += SPEED*cos(dir), y -= SPEED*sin(dir)` |
| hp48-puzzle-bobble | table | fixed-pt | Lookup sin/cos tables, 3-bit fraction |

**Synthesis for pb_core:**
- Use Q16.16 fixed-point for determinism (already implemented)
- Base speed: 10 units/frame (normalized to 60fps)
- Delta-time optional for variable framerate modes
- Trigonometric velocity: `vx = speed * sin(angle), vy = -speed * cos(angle)`

### Wall Bounce

| Implementation | Formula |
|----------------|---------|
| puzzle-bobble-sdl | `alpha = PI - alpha` |
| puzzle-bobble-cpp | `speed.x *= -1` (debounce 100ms) |
| km-bubble-shooter | `velocity.x *= -1` |
| puzzle-bobble-js | `angle = 180 - angle` |
| frozen-bubble | `x = 2*limit - x, dir -= 2*(dir - PI/2)` |
| hp48-puzzle-bobble | `angle = 78 - angle` (lookup index inversion) |

**Synthesis for pb_core:**
- Horizontal velocity negation: `vx = -vx`
- Mirror position to prevent wall penetration: `x = 2*boundary - x`
- Debounce timer to prevent rapid oscillation (50-100ms)
- Sound event trigger on bounce

---

## 2. Collision Detection

### Ball-to-Ball Distance Threshold

| Implementation | Threshold | Notes |
|----------------|-----------|-------|
| puzzle-bobble-sdl | 0.87 * BUBBLE_SIZE | ~28px for 32px bubbles |
| puzzle-bobble-cpp | 18 * SCREEN_SIZE | Variable based on scaling |
| km-bubble-shooter | BALL_WIDTH - 6 = 17px | Slightly smaller than diameter |
| puzzle-bobble-js | 57-58 px | ~2 * radius (28px) |
| frozen-bubble | 0.82 * BUBBLE_SIZE | ~26px for 32px bubbles |
| hp48-puzzle-bobble | Pixel AND overlap | Bitwise collision |

**Synthesis for pb_core:**
- Collision distance: `0.85 * bubble_diameter` (best compromise)
- Use squared distance to avoid sqrt: `dx*dx + dy*dy < threshold_squared`
- Early exit optimization: check X distance first

### Snap-to-Grid Algorithm

All implementations use similar approach:
1. Calculate grid row from Y: `row = (y - top) / row_height`
2. Calculate grid column from X with hex offset: `col = (x - left - offset) / col_width`
3. Hex offset: `offset = (row % 2) * half_col_width`
4. Find nearest empty cell if target occupied

**Synthesis for pb_core:**
- Already implemented in `pb_hex.h` (axial/offset conversions)
- Enhance with "find nearest empty" search in 6-neighbor ring

---

## 3. Matching Algorithm

### Algorithm Comparison

| Implementation | Algorithm | Data Structure |
|----------------|-----------|----------------|
| puzzle-bobble-pure-c | 8-neighbor check | Direct array |
| puzzle-bobble-sdl | BFS flood-fill | FIFO queue |
| puzzle-bobble-cpp | Recursive DFS | Call stack |
| km-bubble-shooter | Recursive DFS | Call stack + searched list |
| puzzle-bobble-js | Recursive flood-fill | Match flag per bubble |
| graph-bubble-shooter | BFS with depth tracking | Queue + depth field |
| bustamove-np | BFS with seen set | Frontier array |
| frozen-bubble | BFS via neighbor traversal | Array iteration |

**Best Practice Synthesis:**
```
BFS_MATCH(root_bubble, color):
    queue = [root_bubble]
    visited = {root_bubble}
    matches = [root_bubble]

    while queue not empty:
        current = queue.pop_front()
        for neighbor in get_hex_neighbors(current):
            if neighbor.color == color AND neighbor not in visited:
                visited.add(neighbor)
                matches.append(neighbor)
                queue.push_back(neighbor)

    return matches if len(matches) >= 3 else []
```

**Synthesis for pb_core:**
- Use iterative BFS (not recursive DFS) to avoid stack overflow on large boards
- Maintain visited set using board cell flags
- Match threshold: 3+ bubbles (configurable via ruleset)
- Return match list for scoring/animation

### Hex Neighbor Offsets

From all implementations, consensus hex neighbor pattern:

```
Even rows (y % 2 == 0):           Odd rows (y % 2 == 1):
    (-1, -1) (0, -1)                  (0, -1) (+1, -1)
         \    /                            \    /
  (-1, 0) -- o -- (+1, 0)          (-1, 0) -- o -- (+1, 0)
         /    \                            /    \
    (-1, +1) (0, +1)                  (0, +1) (+1, +1)
```

Already correctly implemented in pb_core's `pb_hex.c`.

---

## 4. Orphan Detection

### Algorithm Comparison

| Implementation | Approach |
|----------------|----------|
| puzzle-bobble-pure-c | Not implemented |
| puzzle-bobble-sdl | Two-pass: anchor BFS from top, remove unmarked |
| puzzle-bobble-cpp | DFS from top row, check `checked` flag |
| km-bubble-shooter | Grid scan after match removal |
| puzzle-bobble-js | markAllConnected() from top, remove !connected |
| graph-bubble-shooter | DFS from row 0, `mDiscover` flag |
| bustamove-np | connectedToTop() BFS from all top cells |
| frozen-bubble | distance_to_root BFS, remove root=0 |

**Best Practice Synthesis:**
```
DETECT_ORPHANS(board):
    // Phase 1: Mark all as unconnected
    for each bubble in board:
        bubble.connected = false

    // Phase 2: BFS from top row anchors
    queue = []
    for each bubble in top_row:
        if bubble.exists:
            bubble.connected = true
            queue.push(bubble)

    while queue not empty:
        current = queue.pop_front()
        for neighbor in get_hex_neighbors(current):
            if neighbor.exists AND not neighbor.connected:
                neighbor.connected = true
                queue.push(neighbor)

    // Phase 3: Collect orphans
    orphans = []
    for each bubble in board:
        if bubble.exists AND not bubble.connected:
            orphans.append(bubble)

    return orphans
```

**Synthesis for pb_core:**
- Run after every match removal
- Return orphan list for falling animation and bonus scoring
- Clear connected flags before each detection

---

## 5. Special Bubbles

### Feature Matrix

| Implementation | Special Types |
|----------------|---------------|
| puzzle-bobble-pure-c | None (5 colors only) |
| puzzle-bobble-sdl | None (8 colors) |
| puzzle-bobble-cpp | None (8 colors, F1-F8 color change cheat) |
| km-bubble-shooter | None (6 colors) |
| puzzle-bobble-js | None (5 colors) |
| graph-bubble-shooter | None (3 colors + BLANK) |
| bustamove-np | None (8 colors, for NP-hard proof) |
| frozen-bubble | None (8 colors, colourblind symbols) |
| jtbubl-fpga | Hardware supports arbitrary tiles |
| hp48-puzzle-bobble | None (7 colors) |

**No reference implementations include special bubbles.** This is an area for pb_core expansion.

**Proposed pb_core Special Bubbles (from arcade Puzzle Bobble series):**
1. **Rainbow/Wild** - Matches any color
2. **Bomb** - Destroys radius of bubbles
3. **Star** - Clears all bubbles of matched color
4. **Metal** - Cannot be matched, only removed as orphan
5. **Ice** - Requires 2 matches to destroy
6. **Fire** - Chains to adjacent bubbles regardless of color

Already partially implemented in pb_core's `pb_effect.h`.

---

## 6. Combo/Scoring System

### Score Formulas

| Implementation | Formula | Notes |
|----------------|---------|-------|
| puzzle-bobble-pure-c | 100 per match | Flat rate |
| puzzle-bobble-sdl | 1 per bubble | No multiplier |
| puzzle-bobble-cpp | matches - 3 bonus | Linear bonus |
| km-bubble-shooter | `10 * floor(q/3 + 1)` | Quantifier system |
| puzzle-bobble-js | Not implemented | No score |
| frozen-bubble | `destroyed + falling - 2` | Malus-based |
| hp48-puzzle-bobble | Per bubble + orphan bonus | Two-tier |

**Best Practice Synthesis (arcade-accurate):**
```
BASE_POINTS = 10
CHAIN_MULTIPLIER = 2

score = 0
chain_level = 0

for each match_event:
    match_count = count(matched_bubbles)
    orphan_count = count(orphan_bubbles)

    // Base score for matched bubbles
    match_score = BASE_POINTS * match_count * (chain_level + 1)

    // Bonus for orphans (higher value)
    orphan_score = BASE_POINTS * orphan_count * CHAIN_MULTIPLIER * (chain_level + 1)

    score += match_score + orphan_score
    chain_level += 1  // Increases for subsequent matches in same turn
```

**Synthesis for pb_core:**
- Configurable base points via ruleset
- Chain multiplier for consecutive matches
- Higher bonus for orphan cascades
- Event callbacks for UI feedback

---

## 7. Grid System

### Dimensions

| Implementation | Cols | Rows | Cell Size |
|----------------|------|------|-----------|
| puzzle-bobble-pure-c | 15 | 8 | Variable |
| puzzle-bobble-sdl | 8 | 11 | 40x28 px |
| puzzle-bobble-cpp | 8/7 alt | 12+ | 16x14 px (scaled) |
| km-bubble-shooter | 17 | 15 | 23x23 px |
| puzzle-bobble-js | 8/7 alt | 10 | 55x48 px |
| frozen-bubble | 8 | 12 | 32x28 px |
| jtbubl-fpga | 8 | Variable | 32x32 tiles |
| hp48-puzzle-bobble | 10 | 12 | 8x8 px |

**Synthesis for pb_core:**
- Default: 8 columns (even rows), 7 columns (odd rows)
- Max rows: 16 (configurable)
- Cell size: 32 pixels (scalable)
- Row height: 28 pixels (7/8 ratio for hex packing)
- Already implemented in `pb_board.h` with PB_BOARD_COLS, PB_BOARD_ROWS

---

## 8. Cannon/Aiming

### Angle Constraints

| Implementation | Min | Max | Step |
|----------------|-----|-----|------|
| puzzle-bobble-pure-c | -1.0 slope | +1.0 slope | 0.1 |
| puzzle-bobble-sdl | ~5° | ~85° | 1 frame |
| puzzle-bobble-cpp | -70° | +70° | 2°/frame |
| km-bubble-shooter | 10° | 170° | Mouse-based |
| puzzle-bobble-js | 6° | 174° | 2°/frame |
| frozen-bubble | 0.1 rad | PI-0.1 rad | 0.03 rad/frame |
| hp48-puzzle-bobble | 0 (12°) | 78 (168°) | 1 index |

**Synthesis for pb_core:**
- Angle range: 10° to 170° (prevents near-horizontal shots)
- Rotation speed: 2°/frame (120°/sec at 60fps)
- Auto-center near vertical (optional)
- Already implemented in `pb_game.h` with PB_ANGLE_MIN, PB_ANGLE_MAX

### Trajectory Preview

| Implementation | Preview Type |
|----------------|--------------|
| puzzle-bobble-cpp | None |
| bustamove-np | Full path with bounces + landing circle |
| frozen-bubble | None (arcade-accurate) |
| hp48-puzzle-bobble | Optional dotted line |

**Synthesis for pb_core:**
- Already implemented in `pb_trajectory.h`
- Show predicted path with wall reflections
- Indicate snap position at end
- Configurable depth/segment count

---

## 9. Level Progression

### Row Insertion Timing

| Implementation | Trigger | Period |
|----------------|---------|--------|
| puzzle-bobble-pure-c | Timer | 10-35 seconds |
| puzzle-bobble-sdl | Timer | 15 seconds |
| puzzle-bobble-cpp | Shot count | 16 shots |
| puzzle-bobble-js | Timer | 5 seconds |
| frozen-bubble | Hurry timer | 8-11 frames |
| hp48-puzzle-bobble | Shot count | Configurable |

**Synthesis for pb_core:**
- Support both timer-based and shot-based triggers
- Configurable via `pb_ruleset`:
  - `row_insert_mode`: TIMER | SHOTS | BOTH
  - `row_insert_period`: seconds or shot count
  - `row_insert_warning`: time before insertion
- Already partially implemented in game modes

### Auto-fire / Hurry System

| Implementation | Warning | Force Fire |
|----------------|---------|------------|
| puzzle-bobble-cpp | 8 seconds | Auto-fire |
| frozen-bubble | 250-400 frames | 375-525 frames |

**Synthesis for pb_core:**
- Warning animation at 5 seconds
- Auto-fire at 8 seconds
- Configurable via ruleset

---

## 10. Multiplayer & Garbage

### Garbage Mechanics (from puzzle-bobble-cpp, frozen-bubble)

**Sending Garbage:**
```
garbage_amount = matched_bubbles + orphan_bubbles - 2
if garbage_amount > 0:
    send_to_opponent(garbage_amount)
```

**Receiving Garbage:**
- Spawn random-colored bubbles at opponent's board top
- Or: Insert new row with random colors
- Spawn velocity: upward, random horizontal offset

**Synthesis for pb_core:**
- Already implemented in `pb_session.h` for versus mode
- Event-based garbage exchange
- Configurable garbage multipliers

---

## 11. RNG Patterns

### Seed Strategies

| Implementation | Seeding |
|----------------|---------|
| puzzle-bobble-sdl | `srand(time(NULL))` per session |
| puzzle-bobble-cpp | `rand() % 8` |
| km-bubble-shooter | `rand() % 6 + 1` |
| puzzle-bobble-js | WoofJS `random(0, 4)` |
| frozen-bubble | `rand()` + validate color exists |
| hp48-puzzle-bobble | None (deterministic sequences) |

**Synthesis for pb_core:**
- Already implemented in `pb_rng.h` with deterministic PRNG
- Seed from replay or system time
- Color validation: only generate colors present on board (for 1P)

---

## Implementation Priority

### Phase 1: Core Mechanics (Already in pb_core)
- [x] Hex grid math (`pb_hex.h`)
- [x] Board operations (`pb_board.h`)
- [x] BFS matching (`pb_board.c`)
- [x] Orphan detection (`pb_board.c`)
- [x] Shot physics (`pb_shot.h`)
- [x] Trajectory prediction (`pb_trajectory.h`)
- [x] Deterministic RNG (`pb_rng.h`)

### Phase 2: Enhanced Features (Partially Implemented)
- [x] Special bubble effects (`pb_effect.h`)
- [x] Game modes (`pb_ruleset.h`)
- [ ] Chain reaction scoring
- [ ] Garbage exchange protocol
- [ ] Row insertion variants

### Phase 3: Advanced Features (To Implement)
- [ ] Trajectory preview with snap indicator
- [ ] Auto-fire timer
- [ ] Hurry-up warning system
- [ ] Score multiplier chains
- [ ] Network multiplayer sync

---

## Key Constants Summary

| Constant | Recommended Value | Source |
|----------|-------------------|--------|
| BUBBLE_SIZE | 32 px | frozen-bubble |
| ROW_HEIGHT | 28 px (7/8 ratio) | frozen-bubble, puzzle-bobble-sdl |
| COLLISION_THRESHOLD | 0.85 * diameter | Average of implementations |
| BALL_SPEED | 10 units/frame | frozen-bubble |
| LAUNCHER_SPEED | 2°/frame | puzzle-bobble-cpp, js |
| ANGLE_MIN | 10° | km-bubble-shooter |
| ANGLE_MAX | 170° | km-bubble-shooter |
| MATCH_THRESHOLD | 3 bubbles | All implementations |
| WALL_BOUNCE_DEBOUNCE | 50ms | puzzle-bobble-cpp |
| AUTO_FIRE_TIME | 8 seconds | puzzle-bobble-cpp |
| ROW_INSERT_PERIOD | 15 seconds | puzzle-bobble-sdl |
| BASE_SCORE | 10 points | km-bubble-shooter |
| ORPHAN_MULTIPLIER | 2x | Proposed |

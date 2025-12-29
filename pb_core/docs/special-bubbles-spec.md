# pb_effect - Special Bubble Effects Cleanroom Specification

## Purpose
Document special bubble types and their effects for cleanroom implementation
in pb_core. Based on observable behavior analysis of bubble shooter variants.

## Special Bubble Types

### 1. PB_SPECIAL_BOMB
**Behavior**: When activated, destroys all adjacent bubbles.

**Algorithm**:
1. Get 6 hexagonal neighbors of the bomb cell
2. Mark each non-indestructible neighbor for removal
3. Chain: If a neighbor is also a special bubble, queue its effect

**Visual**: Explosion animation radiating outward.

### 2. PB_SPECIAL_LIGHTNING
**Behavior**: When activated, destroys all bubbles in the same row.

**Algorithm**:
1. Determine row index of the lightning bubble
2. Iterate all columns in that row
3. Mark each non-indestructible bubble for removal
4. Chain: If any marked bubble is special, queue its effect

**Visual**: Horizontal lightning bolt across the row.

### 3. PB_SPECIAL_STAR
**Behavior**: When activated, destroys all bubbles of the same color as
the bubble that triggered it (the shot bubble's color).

**Algorithm**:
1. Record the triggering bubble's color_id
2. Scan entire board for bubbles matching that color
3. Mark all matching, non-indestructible bubbles for removal
4. Chain: If any marked bubble is special, queue its effect

**Visual**: Star burst, then colored bubbles flash and pop.

### 4. PB_SPECIAL_MAGNETIC
**Behavior**: Attracts the projectile toward itself during flight.
Cannot be destroyed by matching or special effects.

**Algorithm** (Projectile Attraction):
1. During shot physics step, detect nearby magnetic bubbles
2. Apply attraction force toward magnetic bubble center
3. Force magnitude: F = magnet_strength / distance^2
4. Clamp force to avoid infinite attraction at zero distance

**Properties**:
- flags |= PB_FLAG_INDESTRUCTIBLE
- Attracts shots within attraction_radius pixels
- magnet_strength configurable per bubble

### 5. PB_KIND_BLOCKER (Indestructible)
**Behavior**: Cannot be destroyed by any means. Only falls when disconnected.

**Properties**:
- flags |= PB_FLAG_INDESTRUCTIBLE
- Immune to: matching, bomb, lightning, star
- Subject to: orphan detection and gravity

## Effect Chaining

When a special bubble is destroyed, its effect may trigger other special
bubbles, creating a chain reaction.

**Chain Resolution Algorithm**:
```
effect_queue = [initial_special_bubble]
removed_set = {}

while effect_queue not empty:
    current = effect_queue.pop_front()

    if current in removed_set:
        continue  # Already processed

    affected = current.activate_effect(board)

    for cell in affected:
        if cell.is_indestructible():
            continue

        removed_set.add(cell)

        if cell.is_special():
            effect_queue.push_back(cell)

remove all cells in removed_set
run orphan detection
drop disconnected bubbles
```

**Chain Limits**:
- Maximum chain depth: configurable (default: 16)
- Prevents infinite loops via removed_set tracking

## Effect Priority

When multiple effects could apply simultaneously:
1. Process in order they were triggered
2. Earlier triggered effects take precedence
3. Already-removed bubbles are skipped

## API Design

```c
/* Effect result structure */
typedef struct pb_effect_result {
    pb_offset cells[PB_MAX_CELLS];
    int count;
    int chain_depth;
    bool triggered_chain;
} pb_effect_result;

/* Activate special bubble effect */
pb_effect_result pb_effect_activate(
    pb_board* board,
    pb_offset cell,
    uint8_t trigger_color  /* For star bubble */
);

/* Process full effect chain */
pb_effect_result pb_effect_chain(
    pb_board* board,
    pb_offset initial_cell,
    uint8_t trigger_color,
    int max_depth
);

/* Magnetic attraction force */
pb_vec2 pb_magnetic_force(
    pb_point shot_pos,
    pb_point magnet_pos,
    pb_scalar magnet_strength,
    pb_scalar max_radius
);
```

## Constants

```c
/* Effect configuration */
#define PB_EFFECT_MAX_CHAIN_DEPTH  16
#define PB_EFFECT_BOMB_RADIUS      1    /* Hex distance */

/* Magnetic bubble defaults */
#define PB_MAGNETIC_DEFAULT_STRENGTH  PB_FLOAT_TO_FIXED(100.0f)
#define PB_MAGNETIC_DEFAULT_RADIUS    PB_FLOAT_TO_FIXED(80.0f)
```

## Implementation Notes

1. **Cleanroom principle**: This spec is derived from observable game
   mechanics, not from reading source code implementations.

2. **Determinism**: Effect order must be deterministic for replay.
   Use consistent iteration order (row-major, low-to-high).

3. **Performance**: Effect chain processing should be O(n) where n is
   affected bubble count, not O(n^2).

4. **Memory**: Pre-allocate effect result arrays based on PB_MAX_CELLS
   to avoid dynamic allocation during gameplay.

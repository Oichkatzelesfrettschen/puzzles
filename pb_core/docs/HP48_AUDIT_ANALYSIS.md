# HP48 Puzzle Bobble Deep Audit & pb_core Improvement Analysis

This document provides a granular static analysis of the hp48-puzzle-bobble assembly implementation
for the HP48/49 calculator and identifies novel techniques that can improve pb_core.

## Source Files Analyzed

| File | Size | Description |
|------|------|-------------|
| V49/PzB.s | 60KB | Main game logic (physics, collision, matching, rendering) |
| V49/Level.s | 18KB | Level loading and progression |
| V49/Play.s | 5.6KB | Gameplay state machine |
| V49/Gagne.s | 11KB | Win/lose conditions and scoring |
| V48/PzB_beep.s | 62KB | Version with sound effects |

---

## 1. Fixed-Point Mathematics

### HP48 Implementation (Q12.12 in BCD)

The HP48 uses 20-bit (5-nibble) values stored as BCD quartets:

```
Format: SSSSS.FFF (5 nibbles integer, 3 nibbles fractional)
        Actually stored as: 5 nibbles with 12-bit fractional precision

Example: #1C800 = 28.500 (28 * 4096 + 2048 = 0x1C800)
         #07000 = 7.000  (7 * 4096 = 0x7000)
```

**Key constants extracted from PzB.s:**
- Ball position X range: 0 to 56 (`#38` = 56 decimal)
- Ball position Y max: 88 (`#58000` = 88.000)
- Cannon center: (28.5, 82.5) - `#1C800`, `#52800`
- Next ball preview: (1.0, 120.0) - `#01000`, `#78000`

### pb_core Implementation (Q16.16)

pb_core uses 32-bit Q16.16 format:
- 16 bits integer, 16 bits fractional
- Higher precision but more memory per value

### Novel Insight: Range-Specific Precision

**HP48 uses just 20 bits** for the same accuracy needed in gameplay. The fractional precision
(12 bits = 1/4096 resolution) is sufficient for bubble positions on a 56x88 playfield.

**pb_core Improvement Opportunity:**
```c
/* Add PB_SIZE_TIER_MICRO mode with Q12.12 for 8-bit targets */
#if PB_SIZE_TIER == PB_SIZE_TIER_MICRO
typedef int16_t pb_scalar;  /* Q4.12 for tiny boards */
#define PB_FIXED_SHIFT 12
#endif
```

---

## 2. Trigonometric Lookup Tables

### HP48 Implementation

Pre-computed sin/cos tables for **79 angles** from 12° to 168° in 2° steps:

```assembly
* From PzB.s lines 2508-2674:
* init des Cosinus
    CON(5) #00FA6    * COS(12)*16*16*16 = 0.978 * 4096 = 4006
    CON(5) #00F86    * COS(14)*16*16*16 = 0.970 * 4096 = 3974
    ...
    CON(5) #00800    * COS(60)*16*16*16 = 0.500 * 4096 = 2048
    ...
    CON(5) #00000    * COS(90)*16*16*16 = 0.000 * 4096 = 0
    CON(5) #FFF71    * COS(92)*16*16*16 = -0.035 * 4096 = -143 (two's complement)
```

**Lookup calculation:**
```assembly
* Direction index 0-78 maps to angles 12°-168°
* Each table entry is 5 nibbles (5 bytes)
* Index to address: table_base + (index * 5)

    C=C+C   A   * index * 2
    C=C+C   A   * index * 4
    C=C+D   A   * index * 5 (D held original index)
    D0=(2)  TAB_SIN
    A=DAT0  A   * table base
    C=C+A   A   * table_base + offset
    D1=C        * D1 points to correct entry
    A=DAT1  A   * A = SIN/COS value
```

### pb_core Implementation

pb_core's `pb_freestanding.h` has a **256-entry** lookup table covering full 2*PI:
- Q15 format (32767 = 1.0)
- 512 bytes for sin table
- Covers all angles, not just firing range

### Novel Insight: Game-Specific Angle Range

The HP48 only stores angles actually needed (12° to 168°), saving 70% memory:
- HP48: 79 entries × 5 bytes = **395 bytes** (sin + cos = 790 bytes)
- pb_core: 256 entries × 2 bytes = **512 bytes** (sin only, cos derived)

**pb_core Improvement Opportunity:**
```c
/* Game-specific angle lookup for cannon angles only */
#define PB_ANGLE_TABLE_MIN  PB_FLOAT_TO_FIXED(0.2094f)  /* ~12 degrees */
#define PB_ANGLE_TABLE_MAX  PB_FLOAT_TO_FIXED(2.9322f)  /* ~168 degrees */
#define PB_ANGLE_TABLE_STEP PB_FLOAT_TO_FIXED(0.0349f)  /* ~2 degrees */
#define PB_ANGLE_TABLE_SIZE 79

/* Compact table - only firing angles needed */
static const int16_t pb_cannon_sin[PB_ANGLE_TABLE_SIZE];
static const int16_t pb_cannon_cos[PB_ANGLE_TABLE_SIZE];

/* Index-based angle for efficient storage/network */
typedef uint8_t pb_angle_index;  /* 0-78 maps to 12°-168° */
```

---

## 3. Direction Index vs Radians

### HP48 Implementation

Direction stored as **integer index 0-78** rather than radians:

```assembly
CANON_DIRECT    ALLOC 2    * direction du canon de 0..78 (12° à 168°)

* Wall reflection is trivial:
    LC(2)   #4E        * 78 = max index
    C=C-D   B          * new_direction = 78 - old_direction
    D0=(2)  AFF_BILLE_DIRECT
    DAT0=C  B          * store reflected direction
```

**Benefits:**
1. Angle reflection is a single subtraction: `78 - index`
2. No trig computation for reflection
3. Bounds checking is simple integer comparison
4. Perfect for replay/network sync (1 byte vs 4 bytes)

### pb_core Implementation

Angles stored as pb_scalar (float or Q16.16):
- Wall reflection requires: `new_angle = PI - old_angle`
- More expensive computation

**pb_core Improvement Opportunity:**
```c
/* Add optional index-based angle mode */
typedef struct pb_cannon {
    pb_angle_index direction;  /* 0-78 index for efficient ops */
    pb_scalar angle;           /* Computed from index when needed */
} pb_cannon;

/* Trivial reflection */
#define PB_REFLECT_ANGLE(idx) (PB_ANGLE_TABLE_SIZE - 1 - (idx))

/* Network-friendly: send 1 byte instead of 4 */
uint8_t pb_cannon_get_direction_byte(const pb_cannon* cannon);
```

---

## 4. Pixel-Based Collision Detection

### HP48 Implementation (Affiche_Bille)

The HP48 uses **sprite overlap detection** via bitwise AND:

```assembly
* From PzB.s lines 1628-1641:
Rtn_aff_bille:
    C=DAT0  B          * Read sprite row from ball image
    A=DAT1  X          * Read existing pixels at destination
    ?A=0    X          * If destination empty...
    GOYES   Aff_b_direct   * ...write directly
        ACEX    X      * Swap A and C
        D=C     X      * Save ball sprite in D
        C=C&A   X      * AND: new sprite & existing pixels
        ?C=0    X      * If no overlap...
        GOYES   aff_b_pas_choc   * ...no collision
            ST=1    1      * COLLISION DETECTED!
aff_b_pas_choc:
        C=D     X      * Restore ball sprite
        C=C!A   X      * OR with existing (composite)
Aff_b_direct:
    DAT1=C  X          * Write to screen
```

**Algorithm:**
1. For each row of the 8×8 ball sprite:
2. Read existing screen pixels at destination
3. Compute `overlap = new_pixels AND existing_pixels`
4. If `overlap != 0`, collision detected
5. Write `new_pixels OR existing_pixels` to composite

### pb_core Implementation

Distance-based collision using squared distance:
```c
pb_scalar dx = a.x - b.x;
pb_scalar dy = a.y - b.y;
pb_scalar dist_sq = dx*dx + dy*dy;
return dist_sq < (threshold * threshold);
```

### Novel Insight: Pixel-Perfect for Retro Renderers

Pixel-based collision is **visually accurate** - if sprites overlap on screen, collision occurs.
This matters for pixel-art renderers where circle approximations create visible gaps/overlaps.

**pb_core Improvement Opportunity:**
```c
/* Add optional pixel-mask collision mode */
typedef struct pb_collision_mask {
    uint8_t width;
    uint8_t height;
    const uint8_t* mask;  /* 1 bit per pixel */
} pb_collision_mask;

/* Pixel-perfect collision check */
bool pb_collision_masks_overlap(
    const pb_collision_mask* a, pb_point pos_a,
    const pb_collision_mask* b, pb_point pos_b
);

/* Hybrid approach for pb_core */
#define PB_COLLISION_MODE_DISTANCE  0  /* Current: sqrt(dx²+dy²) < threshold */
#define PB_COLLISION_MODE_PIXEL     1  /* New: sprite mask overlap */
```

---

## 5. Board Representation & Memory Layout

### HP48 Implementation

Each cell is **2 nibbles (1 byte)** with bit flags:

```
Cell format: [B7 B6 B5 B4 | B3 B2 B1 B0]
             |         |   |         |
             |         |   +-- Color (0-7) or type code
             |         +------ Bit 4: Marked for deletion
             +---------------- Bit 5: Attached to ceiling

Type codes:
    0x00 = Empty
    0x01-0x07 = Colored bubble (7 colors)
    0x0A-0x0D = Barrier types
    0x0E = Pusher element
    0x0F = Wall/boundary
```

**Row layout (20 nibbles = 10 bytes per row):**
```
[Wall][B1][B2][B3][B4][B5][B6][B7][B8?][Wall]
  1     2   2   2   2   2   2   2   2?   1    = 18 or 20 nibbles
```

Even rows: 8 bubbles + 2 walls = 10 cells = 20 nibbles
Odd rows: 7 bubbles + 2 walls = 9 cells = 18 nibbles (but padded to 20)

### pb_core Implementation

```c
typedef struct pb_bubble {
    pb_bubble_kind kind;       /* 4 bytes (enum) */
    uint8_t color_id;          /* 1 byte */
    uint8_t flags;             /* 1 byte */
    pb_special_type special;   /* 4 bytes (enum) */
    union { ... };             /* 4+ bytes */
} pb_bubble;  /* ~16 bytes per bubble */
```

### Novel Insight: Compact Bubble Encoding

HP48 uses **1 byte per bubble** vs pb_core's **16+ bytes**.
For a 12×8 grid: HP48 = 96 bytes, pb_core = 1536+ bytes.

**pb_core Improvement Opportunity:**
```c
/* Add PB_COMPACT_BUBBLES mode for embedded */
#if PB_COMPACT_BUBBLES

/* 2-byte compact bubble (same info, 8x smaller) */
typedef struct pb_bubble_compact {
    uint8_t kind_color;  /* [kind:4][color:4] */
    uint8_t flags_special; /* [flags:4][special:4] */
} pb_bubble_compact;

/* Bit extraction macros */
#define PB_COMPACT_KIND(b)    ((b).kind_color >> 4)
#define PB_COMPACT_COLOR(b)   ((b).kind_color & 0x0F)
#define PB_COMPACT_FLAGS(b)   ((b).flags_special >> 4)
#define PB_COMPACT_SPECIAL(b) ((b).flags_special & 0x0F)

#endif
```

---

## 6. Matching Algorithm

### HP48 Implementation (Suppr_Tableau)

Iterative **flood-fill with bit marking**:

```assembly
* From PzB.s lines 968-1123:
Suppr_Tableau:
    ST=0    11         * Clear "new bubbles marked" flag

    * For each cell in grid:
    C=DAT1  B          * Read cell value
    ?CBIT=0 4          * Check if already marked (bit 4)
    GOYES   Fin_b_suppr_tab   * Skip if not marked
        * Test 6 neighbors (hardcoded offsets for hex grid)
        D1=D1-  2      * Previous cell
        GOSUB   Suppr_Tab_test
        D1=D1+  4      * Next cell
        GOSUB   Suppr_Tab_test
        * ... (4 more neighbor tests with row-dependent offsets)

Suppr_Tab_test:
    A=DAT1  B          * Read neighbor
    ?A#C    P          * Same color as current?
    RTNYES             * No, return
    ?ABIT=1 4          * Already marked?
    RTNYES             * Yes, return
    ST=1    11         * Set "new bubble marked" flag
    ABIT=1  4          * Mark the bubble
    DAT1=A  B          * Store marked bubble
    * Increment match counter
```

**Key insight:** Loop until `ST(11) == 0` (no new bubbles marked this pass).
This handles chain reactions naturally.

### pb_core Implementation

BFS with visited set and cell coordinates.

### Novel Insight: In-Place Bit Flags

HP48 marks bubbles **in-place** using bit 4, avoiding a separate visited set.
This is more memory-efficient for constrained systems.

**pb_core Improvement Opportunity:**
```c
/* Add in-place marking mode using bubble flags */
#define PB_FLAG_MATCH_MARKED   (1 << 6)  /* Temp: marked for this match */
#define PB_FLAG_ORPHAN_CHECK   (1 << 7)  /* Temp: attached check in progress */

/* In-place flood-fill (no separate visited array) */
int pb_board_find_matches_inplace(pb_board* board, pb_offset start);
int pb_board_find_orphans_inplace(pb_board* board);
```

---

## 7. Hex Neighbor Offsets

### HP48 Implementation

Hardcoded pointer arithmetic for hex neighbors:

```assembly
* Even rows (ABIT=0 at position 0):
* Row layout: 10 cells/row × 2 nibbles/cell = 20 nibbles/row
    D1=D1-  2      * Left: -2 nibbles
    D1=D1+  4      * Right: +4 nibbles (after left test)
    D1=D1+  18     * Down-left: +20 - 2 = +18
    D1=D1+  2      * Down-right: +20
    D1=D1-  40     * Up-right: -20 - 2 = -22 (but done as -40 after +18)
    D1=D1-  2      * Up-left: -20 - 4 = -24

* Odd rows (different offsets due to stagger):
    D1=D1-  2      * Left
    D1=D1+  4      * Right
    D1=D1+  16     * Down: +20 - 4 = +16
    D1=D1+  2      * Down+1: +20 - 2 = +18
    D1=D1-  38     * Up+1: -20 - 2 = -22 (as -38 after +16)
    D1=D1-  2      * Up: -20 = -24
```

### pb_core Implementation

Uses `pb_hex_offset_neighbors()` with coordinate-based lookup.

### Novel Insight: Direct Pointer Arithmetic

For tight loops, **pointer offsets** avoid coordinate conversion overhead.

**pb_core Improvement Opportunity:**
```c
/* Pre-computed neighbor offsets for linear array access */
static const int8_t pb_neighbor_offsets_even[6] = {-1, +1, +7, +8, -9, -8};
static const int8_t pb_neighbor_offsets_odd[6]  = {-1, +1, +8, +9, -8, -7};

/* Fast neighbor iteration without coordinate conversion */
#define PB_FOR_EACH_NEIGHBOR(board, idx, neighbor_idx) \
    const int8_t* _offsets = ((idx) / PB_COLS) % 2 ? \
        pb_neighbor_offsets_odd : pb_neighbor_offsets_even; \
    for (int _i = 0; _i < 6; _i++) \
        if ((neighbor_idx = (idx) + _offsets[_i]) >= 0 && \
            neighbor_idx < PB_MAX_CELLS)
```

---

## 8. Random Number Generation

### HP48 Implementation

Uses **hardware entropy sources**:

```assembly
* From PzB.s lines 531-534:
D0=(5)  #00138   * Hardware timer address
A=DAT0  8        * Read timer (causes CRC update)
D0=(2)  #04      * CRC register address
A=DAT0  1        * Read CRC low nibble (0-15)
ABIT=0  3        * Mask to 0-7 for color

* For X position (lines 616-625):
D1=(5)  #00104   * Different CRC offset
C=DAT1  B        * Read 2 nibbles (0-255)
CBIT=0  6
CBIT=0  7        * Mask to 0-63
* Clamp to 0-56 for valid X
```

### pb_core Implementation

Deterministic PRNG (`pb_rng.h`) seeded at game start.

### Novel Insight: Hardware Timer Entropy Mixing

HP48 **reads hardware timer** before reading CRC, causing the CRC to update.
This adds entropy from timing jitter.

**pb_core Improvement Opportunity:**
```c
/* Add hardware entropy mixing for platforms with timers */
#if PB_PLATFORM_HAS_TIMER
void pb_rng_mix_timer_entropy(pb_rng* rng)
{
    /* Read timer to mix in timing jitter */
    uint32_t timer = pb_platform_read_timer();
    rng->state ^= timer;
    rng->state = pb_rng_next(rng);  /* Advance state */
}
#endif

/* Call periodically during user input for better entropy */
void pb_rng_feed_entropy(pb_rng* rng, uint32_t entropy);
```

---

## 9. Auto-Fire Timer System

### HP48 Implementation (Affiche_Time)

10-second countdown with visual indicator:

```assembly
* From PzB.s lines 706-753:
Affiche_Time:
    D0=(2)  SAUVE_TIMER
    A=DAT0  8          * Saved timer at shot start
    D0=(2)  SAV_TIMER2
    C=DAT0  8          * Current timer
    C=A-C   WP         * Elapsed time
    CSRB.F  WP         * Convert to seconds

    LA(5)   #0000A     * 10 seconds limit
    A=A-C   A          * Remaining time
    ?ABIT=0 15         * Negative? (time exceeded)
    GOYES   aff_tim_pas0
        * TIME OUT - Auto-fire!
        ?ST=1   2      * Already in motion?
        RTNYES
        GOSUBL  Beep_tir_canon
        ST=1    2      * Set ball in motion
        RTN

aff_tim_pas0:
    LC(2)   #05        * Check if < 5 seconds
    ?A>C    B
    RTNYES             * > 5 seconds, no warning

    * Display countdown bubble (type 16-21 = countdown sprites)
    LC(2)   #10        * Base type for countdown
    A=A+C   A          * Add remaining seconds
    D0=(2)  TIME_BILLE_TYPE
    DAT0=A  B
    * Display countdown sprite...
```

### pb_core Current Implementation

Already has hurry-up system in `pb_game.c`:
```c
/* Auto-fire / hurry-up system check */
if (state->shot.phase == PB_SHOT_IDLE) {
    uint32_t frames_since_shot = state->frame - state->last_shot_frame;
    if (!state->hurry_active && frames_since_shot >= PB_HURRY_WARNING_FRAMES) {
        state->hurry_active = true;
        state->phase = PB_PHASE_HURRY;
    }
    if (frames_since_shot >= PB_HURRY_AUTOFIRE_FRAMES) {
        pb_game_fire(state);
    }
}
```

### Novel Insight: Visual Countdown Sprites

HP48 uses **dedicated countdown sprites** (types 16-21) that animate the remaining time.

**pb_core Improvement Opportunity:**
```c
/* Add countdown visualization info */
typedef struct pb_hurry_state {
    bool active;
    int seconds_remaining;  /* For countdown display */
    pb_point indicator_pos; /* Where to show countdown */
} pb_hurry_state;

pb_hurry_state pb_game_get_hurry_state(const pb_game_state* state);
```

---

## 10. Screen Vibration / Warning Effect

### HP48 Implementation (Test_Vibra_lign)

Screen shake warning before row insertion:

```assembly
* From PzB.s lines 444-480:
Test_Vibra_lign:
    D0=(2)  NB_TIRS_COURANT
    A=DAT0  B
    ?A=0    B          * At 0 = insert now
    RTNYES
    LC(2)   #02        * 2 shots remaining
    ?A>C    B          * More than 2?
    RTNYES             * No warning yet

    D0=(2)  SAV_TIMER2 * Read timer for oscillation
    C=DAT0  A

    ?ABIT=1 0          * 1 shot remaining?
    GOYES   tst_vibr_1
        * 2 shots: test bit 11 of timer
        ?CBIT=0 11
        GOYES   tst_vibr_marg0
            LC(1)   #9     * margin = 1 (shift right)
            GOTO    tst_vibr_ecrit
tst_vibr_marg0:
        LC(1)   #8         * margin = 0 (normal)
        GOTO    tst_vibr_ecrit

tst_vibr_1:
    * 1 shot remaining: test bit 10 (faster oscillation)
    ?CBIT=0 10
    GOYES   tst_vibr_marg0
        LC(1)   #9

tst_vibr_ecrit:
    D1=(5)  #00100     * Screen margin register
    DAT1=C  1          * Set margin (causes horizontal shift)
```

**Effect:** Uses timer bits to oscillate screen margin between 8 and 9,
creating a shake effect. Faster oscillation (bit 10 vs 11) when closer to insertion.

**pb_core Improvement Opportunity:**
```c
/* Screen shake effect for warnings */
typedef enum pb_screen_effect {
    PB_EFFECT_NONE = 0,
    PB_EFFECT_SHAKE_SLOW,   /* 2 shots remaining */
    PB_EFFECT_SHAKE_FAST,   /* 1 shot remaining */
    PB_EFFECT_FLASH
} pb_screen_effect;

pb_screen_effect pb_game_get_screen_effect(const pb_game_state* state);
pb_vec2 pb_game_get_screen_offset(const pb_game_state* state);  /* For shake */
```

---

## Summary: Novel Improvements for pb_core

### High Priority (Memory/Performance)

1. **Compact Bubble Encoding** - 8x memory reduction for embedded
2. **Game-Specific Angle Table** - 70% less trig table memory
3. **Direction Index Mode** - Trivial angle reflection, 1-byte network sync
4. **Pointer-Based Neighbor Iteration** - Avoid coordinate conversion

### Medium Priority (Features)

5. **Pixel-Mask Collision** - Visually accurate for retro renderers
6. **In-Place Bit Marking** - Memory-efficient flood-fill
7. **Hurry Countdown Visualization** - Dedicated countdown state
8. **Screen Shake Effects** - Warning animations

### Low Priority (Platform-Specific)

9. **Hardware Timer Entropy** - Better RNG on embedded
10. **Q12.12 Mode** - Micro-tier for 8-bit targets

---

## Implementation Recommendations

### Phase 1: Core Optimizations
```c
/* Add to pb_config.h */
#define PB_USE_COMPACT_BUBBLES    0  /* 2-byte bubbles for embedded */
#define PB_USE_ANGLE_INDEX        0  /* Direction 0-78 instead of radians */
#define PB_USE_INPLACE_MARKING    0  /* Bit flags instead of visited set */
```

### Phase 2: Rendering Integration
```c
/* Add to pb_render.h (new file) */
pb_screen_effect pb_game_get_screen_effect(const pb_game_state* state);
pb_hurry_state pb_game_get_hurry_state(const pb_game_state* state);
bool pb_collision_masks_overlap(/* ... */);
```

### Phase 3: Platform Support
```c
/* Enhance pb_rng.h */
void pb_rng_mix_timer_entropy(pb_rng* rng);
void pb_rng_feed_entropy(pb_rng* rng, uint32_t bits);
```

---

*Document generated from static analysis of hp48-puzzle-bobble V49 assembly source.*
*Authors: Lilian Pigallio & Denis Martinez (original implementation)*
*Analysis: pb_core integration study*

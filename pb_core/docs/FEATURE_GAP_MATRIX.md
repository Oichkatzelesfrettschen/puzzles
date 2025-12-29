# pb_core Feature Gap Matrix

**Generated**: 2025-12-27
**Purpose**: Compare pb_core features against 5 reference implementations
**Scope**: Identify missing functionality, prioritize additions

## Reference Implementations Audited

| Implementation | Language | Platform | LOC (approx) | Focus |
|----------------|----------|----------|--------------|-------|
| hp48-puzzle-bobble | Saturn ASM | HP 48 Calculator | ~5K | Two-player IR, compressed data |
| bubble-blast-saga | Swift | iOS | ~15K | Special bubbles, level designer |
| frozen-bubble | Perl/SDL + C | Linux/Unix | ~25K | Multiplayer (5p), level editor, i18n |
| zorx | C++/SDL/OpenGL | Desktop | ~8K | Clean arch, 2-player ready |
| puzzle-bobble-clone-libgdx | Java/libGDX | Android/Desktop | ~12K | Cross-platform, JSON levels |

---

## Feature Comparison Matrix

### Legend
- **Y** = Fully implemented
- **P** = Partially implemented / API exists but not complete
- **N** = Not implemented
- **-** = Not applicable to implementation

### Core Game Mechanics

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Hexagonal grid | Y | Y | Y | Y | Y | Y |
| Match-3 detection | Y | Y | Y | Y | Y | Y |
| Orphan dropping | Y | Y | Y | Y | Y | Y |
| Wall bouncing | Y | Y | Y | Y | Y | Y |
| Ceiling attachment | Y | Y | Y | Y | Y | Y |
| Aim trajectory preview | Y | N | Y | Y | Y | Y |
| Bubble swap (cur/next) | Y | N | Y | Y | Y | Y |
| Ceiling pressure | Y | Y | Y | Y | Y | Y |

### Special Bubble Types

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Bomb (radial destroy) | Y | N | Y | N | N | N |
| Lightning (row clear) | Y | N | Y | N | N | N |
| Star (color match all) | Y | N | Y | Y | N | N |
| Rainbow/Wildcard | Y | N | N | Y | N | N |
| Ice/Frozen | Y | N | N | Y | N | N |
| Magnetic | Y | N | N | N | N | N |
| Portal | Y | N | N | N | N | N |
| Shifter (color cycle) | Y | N | N | N | N | N |
| Split | Y | N | N | N | N | N |
| Virus (spread) | Y | N | N | N | N | N |
| Key/Lock | Y | N | N | N | N | N |
| Ghost | Y | N | N | N | N | N |

**Analysis**: pb_core has the most comprehensive special bubble system. bubble-blast-saga and frozen-bubble have subsets that pb_core already covers.

### Game Modes

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Puzzle (fixed board) | Y | Y | Y | Y | Y | Y |
| Arcade (campaign) | Y | Y | Y | Y | N | Y |
| Survival (endless) | Y | N | Y | Y | N | Y |
| Time Attack | Y | Y | N | N | N | N |
| Versus (competitive) | P | P | N | Y | P | N |
| Coop (shared board) | P | N | N | N | N | N |
| Zen (practice) | Y | N | N | N | N | N |

**Analysis**: pb_core mode definitions exist but Versus/Coop require networking layer.

### Multiplayer / Networking

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Local 2-player split | N | N | N | Y | P | N |
| Network 2-player | N | Y (IR) | N | Y | N | N |
| Network 5-player | N | N | N | Y | N | N |
| Garbage sending | P | Y | N | Y | N | N |
| Spectator mode | N | N | N | Y | N | N |
| Lockstep sync | P | - | - | N | - | - |
| Deterministic replay | Y | - | - | Y | - | - |

**Analysis**: pb_core has deterministic RNG and checksum verification for lockstep, but no network transport. frozen-bubble has the most mature networking (C server, TCP+UDP).

### Level System

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Hardcoded levels | N | Y | N | Y | N | N |
| JSON level format | Y | N | Y | N | N | Y |
| Binary level format | N | Y | N | Y | N | N |
| Level editor (runtime) | N | N | Y | Y | N | N |
| Level validation | Y | N | P | P | N | Y |
| Random generation | Y | N | Y | Y | Y | Y |
| Level packs/campaigns | P | N | Y | Y | N | Y |

**Analysis**: pb_core has JSON levels via cJSON. Missing: runtime level editor UI (platform-specific).

### Replay / Recording

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Input recording | Y | N | N | Y | N | N |
| Replay playback | Y | N | N | Y | N | N |
| Demo files | P | N | N | Y | N | N |
| Checksum verification | Y | N | N | N | N | N |
| Frame-accurate sync | Y | N | N | Y | N | N |

**Analysis**: pb_core has strong replay foundation. frozen-bubble has demo file format worth studying.

### Visual / Audio

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| Sprite rendering | - | Y | Y | Y | Y | Y |
| Particle effects | N | N | Y | Y | Y | Y |
| Animation system | N | N | Y | Y | Y | Y |
| Sound effects | - | Y | Y | Y | Y | Y |
| Background music | - | N | Y | Y | Y | Y |
| Theme/skin support | N | N | Y | Y | N | Y |

**Analysis**: pb_core is platform-agnostic (no rendering). Platform layers handle visuals. Consider: particle effect data structures in core?

### Accessibility

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| CVD simulation | Y | N | N | N | N | N |
| CVD-safe palettes | Y | N | N | N | N | N |
| Pattern overlays | Y | N | N | Y | N | N |
| High contrast mode | P | N | N | N | N | N |
| Screen reader hints | N | N | N | N | N | N |

**Analysis**: pb_core leads in CVD accessibility. frozen-bubble has pattern overlays. None have screen reader support.

### Localization

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| String externalization | N | N | Y | Y | N | Y |
| Translation files | N | N | Y | Y | N | Y |
| Language count | 0 | 0 | ~5 | 31+ | 0 | ~3 |
| RTL support | N | N | N | N | N | N |

**Analysis**: pb_core has no UI strings (platform-agnostic). Localization is platform layer concern.

### Build / Portability

| Feature | pb_core | hp48 | bubble-blast | frozen | zorx | libgdx |
|---------|---------|------|--------------|--------|------|--------|
| C11 standard | Y | - | - | P | Y | - |
| Cross-platform | Y | - | N | Y | Y | Y |
| Fixed-point option | Y | Y | N | N | N | N |
| Endian-aware serial | P | Y | - | - | - | - |
| WASM target | P | - | - | N | N | N |
| Mobile target | Y | - | Y | N | N | Y |

**Analysis**: pb_core designed for maximum portability. Needs endianness audit.

---

## Gap Priority Matrix

### Priority 1 (Core Gameplay Gaps)
| Gap | Effort | Impact | Source Reference |
|-----|--------|--------|------------------|
| Particle effect data model | Low | Medium | bubble-blast, libgdx |
| Animation state machine | Medium | Medium | zorx, frozen |

### Priority 2 (Multiplayer Gaps)
| Gap | Effort | Impact | Source Reference |
|-----|--------|--------|------------------|
| Network transport abstraction | High | High | frozen-bubble C server |
| Garbage queue system | Medium | High | frozen-bubble, hp48 |
| Spectator state | Low | Medium | frozen-bubble |

### Priority 3 (Tooling Gaps)
| Gap | Effort | Impact | Source Reference |
|-----|--------|--------|------------------|
| Level editor data model | Low | Medium | frozen-bubble, bubble-blast |
| Demo file format spec | Low | Medium | frozen-bubble |
| Binary level format | Medium | Low | hp48, frozen |

### Priority 4 (Polish Gaps)
| Gap | Effort | Impact | Source Reference |
|-----|--------|--------|------------------|
| Theme/skin data model | Low | Low | bubble-blast |
| Screen reader hint API | Low | Medium | (none - novel) |

---

## Recommendations

### Immediate (No Code Changes)
1. **Document endianness strategy** for replay serialization
2. **Specify demo file format** based on frozen-bubble's approach
3. **Document network sync protocol** requirements

### Short Term (Core Additions)
1. Add `pb_particle` type for platform-agnostic particle definitions
2. Add `pb_animation` state machine for effect timing
3. Formalize garbage queue in versus mode

### Medium Term (Architecture)
1. Design network transport abstraction layer
2. Implement binary level format for embedded targets
3. Add screen reader hint callbacks

### Not Recommended
- **Level editor in core**: UI is platform-specific; keep data model only
- **Audio in core**: Platform layer responsibility
- **Rendering in core**: Already correctly abstracted

---

## Source Code References

### Networking (frozen-bubble)
- `server/game.c` - TCP/UDP game server
- `lib/Games/FrozenBubble/Net/` - Perl network layer

### Special Bubbles (bubble-blast-saga)
- `BubbleBlastSaga/Models/SpecialBubble/` - Effect implementations
- `BubbleBlastSaga/Physics/` - Physics engine

### Level Format (libgdx)
- `core/src/main/resources/levels/` - JSON level examples
- `core/src/main/java/*/LevelLoader.java` - Parser

### Demo Recording (frozen-bubble)
- `lib/Games/FrozenBubble/Stuff.pm` - Demo file I/O
- Demo format: frame number + action pairs

---

## Conclusion

pb_core is **feature-complete for single-player gameplay** with the most comprehensive special bubble system among all audited implementations.

**Primary gaps**:
1. **Network transport** - Required for multiplayer modes that are already defined
2. **Particle/animation data** - Nice-to-have for richer platform integrations
3. **Binary level format** - Useful for embedded/calculator targets

**Unique strengths**:
- CVD accessibility (unmatched)
- Fixed-point determinism (only hp48 matches)
- Clean separation of concerns (no rendering in core)
- Comprehensive effect system (12 special types with data-driven triggers)

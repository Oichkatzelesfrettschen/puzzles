# pb_core Endianness Audit Report

**Date**: 2025-12-27
**Scope**: Binary serialization in replay, checksum, and data layers
**Status**: Issues found requiring remediation

---

## Executive Summary

pb_core has **partial endianness safety**. Event packing and checksum computation
use explicit byte ordering, but header and checkpoint serialization use raw
`memcpy()`, causing replay files to be **non-portable across architectures**.

A replay recorded on x86_64 (little-endian) cannot be loaded on powerpc64
(big-endian) and vice versa.

---

## Audit Scope

| File | Purpose | Endian Status |
|------|---------|---------------|
| `src/core/pb_replay.c` | Replay serialization | **MIXED** |
| `src/core/pb_checksum.c` | CRC-32 computation | **SAFE** |
| `src/data/pb_data.c` | JSON level loading | **SAFE** (text format) |

---

## Findings

### 1. pb_checksum.c - SAFE

The checksum code correctly uses explicit little-endian byte ordering:

```c
/* src/core/pb_checksum.c:75-85 */
static uint32_t crc_feed_u32(uint32_t crc, uint32_t val)
{
    /* Little-endian byte order for consistency */
    uint8_t bytes[4] = {
        (uint8_t)(val & 0xFF),
        (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF),
        (uint8_t)((val >> 24) & 0xFF)
    };
    return pb_crc32_update(crc, bytes, 4);
}
```

This ensures checksum values match regardless of host architecture.

### 2. Event Packing - SAFE

`pb_event_pack()` correctly uses explicit byte ordering for angle values:

```c
/* src/core/pb_replay.c:131-144 */
out[written++] = (uint8_t)(angle_fixed & 0xFF);
out[written++] = (uint8_t)((angle_fixed >> 8) & 0xFF);
out[written++] = (uint8_t)((angle_fixed >> 16) & 0xFF);
out[written++] = (uint8_t)((angle_fixed >> 24) & 0xFF);
```

Varint encoding (LEB128) is also inherently endian-safe since it works byte-by-byte.

### 3. Header Serialization - UNSAFE

`pb_replay_serialize()` uses raw memcpy for the header:

```c
/* src/core/pb_replay.c:405-407 */
memcpy(buffer + offset, &replay->header, sizeof(pb_replay_header));
```

**Affected fields in `pb_replay_header`:**

| Field | Type | Size | Issue |
|-------|------|------|-------|
| `magic` | uint32_t | 4 | Byte order differs |
| `reserved` | uint16_t | 2 | Byte order differs |
| `seed` | uint64_t | 8 | Byte order differs |
| `event_count` | uint32_t | 4 | Byte order differs |
| `checkpoint_count` | uint32_t | 4 | Byte order differs |
| `duration_frames` | uint32_t | 4 | Byte order differs |
| `final_score` | uint32_t | 4 | Byte order differs |

Single-byte fields (`version`, `flags`, `outcome`, `reserved2`) and char arrays
(`level_id`, `ruleset_id`) are endian-neutral.

### 4. Checkpoint Serialization - UNSAFE

`pb_replay_serialize()` uses raw memcpy for checkpoints:

```c
/* src/core/pb_replay.c:412-414 */
memcpy(buffer + offset, replay->checkpoints, checkpoint_size);
```

**Affected fields in `pb_checkpoint`:**

| Field | Type | Size | Issue |
|-------|------|------|-------|
| `frame` | uint32_t | 4 | Byte order differs |
| `event_index` | uint32_t | 4 | Byte order differs |
| `state_checksum` | uint32_t | 4 | Byte order differs |
| `board_checksum` | uint32_t | 4 | Byte order differs |
| `rng_state[4]` | uint32_t[4] | 16 | Byte order differs |
| `score` | uint32_t | 4 | Byte order differs |
| `shots_fired` | int | 4 | **CRITICAL**: size varies by platform |

**Critical issue**: `shots_fired` uses `int` which is platform-dependent
(typically 4 bytes but could be 2 on some embedded). Should use `int32_t`.

### 5. Deserialization - SAME ISSUES

`pb_replay_deserialize()` has the same issues in reverse:

```c
/* src/core/pb_replay.c:443-445 */
memcpy(&replay->header, buffer, sizeof(pb_replay_header));
```

```c
/* src/core/pb_replay.c:468 */
memcpy(replay->checkpoints, buffer + offset, checkpoint_size);
```

---

## Risk Assessment

| Scenario | Impact | Likelihood |
|----------|--------|------------|
| Cross-platform replay sharing | **HIGH** - Replays unreadable | Medium (hp48/ARM targets) |
| Network multiplayer sync | **HIGH** - Desyncs on mixed arch | Low (no network yet) |
| Checksum mismatch detection | None - checksums are safe | N/A |
| RNG desync on replay | **HIGH** - Wrong rng_state loaded | Medium |

---

## Recommended Fixes

### Immediate: Type Safety

Change `int shots_fired` to `int32_t shots_fired` in `pb_checkpoint` for
consistent sizing across platforms.

### Short-term: Explicit Byte Ordering

Add serialization helper functions:

```c
static void write_le16(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static void write_le32(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static void write_le64(uint8_t* buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
}

static uint16_t read_le16(const uint8_t* buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t read_le32(const uint8_t* buf) {
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static uint64_t read_le64(const uint8_t* buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (uint64_t)buf[i] << (i * 8);
    }
    return val;
}
```

Replace memcpy with explicit field serialization:

```c
/* Header serialization */
write_le32(buffer + offset, replay->header.magic); offset += 4;
buffer[offset++] = replay->header.version;
buffer[offset++] = replay->header.flags;
write_le16(buffer + offset, replay->header.reserved); offset += 2;
write_le64(buffer + offset, replay->header.seed); offset += 8;
memcpy(buffer + offset, replay->header.level_id, 64); offset += 64;
memcpy(buffer + offset, replay->header.ruleset_id, 64); offset += 64;
write_le32(buffer + offset, replay->header.event_count); offset += 4;
/* ... etc ... */
```

### Version Bump

When implementing fixes, increment `PB_REPLAY_VERSION` to ensure old
(endian-broken) replays are rejected gracefully rather than loading corrupted.

---

## Verification Plan

1. Build for both x86_64 and ppc64 (via cross-compiler)
2. Create replay on x86_64, save to file
3. Hexdump and verify little-endian byte order
4. Load replay on ppc64 (via QEMU user-mode)
5. Verify frame-perfect playback

---

## References

- frozen-bubble demo format: Frame-delimited text, avoids binary issues
- hp48-puzzle-bobble: Saturn is big-endian; uses explicit byte packing
- libgdx JSON levels: Text format, inherently portable

---

## Conclusion

pb_core has a well-designed event encoding layer (varint + explicit bytes) but
incomplete endianness handling in the replay header and checkpoint
serialization. The fix is straightforward: replace memcpy with explicit
little-endian read/write helpers.

**Priority**: Medium - Required before cross-platform replay sharing or
big-endian platform releases (hp48, PowerPC retro targets).

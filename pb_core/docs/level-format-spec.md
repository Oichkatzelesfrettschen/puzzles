# pb_level - Ultra-Compressed Level Format Specification

## Purpose
Define a compact binary level format for pb_core suitable for extremely
memory-constrained platforms (HP 48 calculators, 8-bit systems, embedded).

## Design Goals
1. **Minimal size**: Target 32-128 bytes per level (vs. 512+ bytes uncompressed)
2. **No heap allocation**: Fixed-size decode buffer
3. **Deterministic decoding**: Bit-exact results for replay compatibility
4. **Simple implementation**: O(n) decode, portable C89

## Format Overview

### Header (4 bytes)
```
Offset  Size  Field
------  ----  -----
0       1     magic (0xBB = "Bubble Board")
1       1     version | flags
              bits 0-3: version (0 = v1.0)
              bits 4-5: color_bits (0=3-bit, 1=4-bit, 2=5-bit)
              bit 6: has_specials
              bit 7: has_rle
2       1     rows (1-32)
3       1     cols_base | col_offset
              bits 0-5: cols_even (1-32)
              bits 6-7: col_offset (cols_odd = cols_even - col_offset)
```

### Color Palette (optional, if version >= 1)
```
Offset  Size  Field
------  ----  -----
4       1     num_colors (1-8)
5-12    0-8   color_indices (maps local 0-7 to global color IDs)
```

### Cell Data (variable)
Cells are stored row-major, left-to-right, top-to-bottom.

#### Mode 0: Packed nibbles (no RLE, 4 bits per cell)
```
Each nibble (4 bits):
  0x0: empty cell
  0x1-0x7: color 1-7
  0x8-0xE: special types 1-7 (bomb, lightning, star, etc.)
  0xF: indestructible blocker
```

#### Mode 1: RLE + packed nibbles (has_rle=1)
```
Each byte is either:
  0x00-0x7F: Raw nibble pair (high nibble, low nibble)
  0x80-0xBF: RLE run of (byte & 0x3F)+1 empty cells
  0xC0-0xFF: RLE run of (byte & 0x3F)+1 cells of next nibble's color
```

#### Mode 2: Bitpacked (color_bits < 4)
```
3-bit mode: 8 bubbles per 3 bytes
  Byte0: cell0[2:0] | cell1[2:0] | cell2[1:0]
  Byte1: cell2[2] | cell3[2:0] | cell4[2:0] | cell5[0]
  Byte2: cell5[2:1] | cell6[2:0] | cell7[2:0]

2-bit mode: 4 bubbles per byte (for <= 4 colors)
  bits 7-6: cell0
  bits 5-4: cell1
  bits 3-2: cell2
  bits 1-0: cell3
```

## Special Cell Encoding

When has_specials=1, the format supports special bubble types:
```
Nibble  Type              Effect
------  ----              ------
0x8     PB_SPECIAL_BOMB       Destroys neighbors
0x9     PB_SPECIAL_LIGHTNING  Destroys row
0xA     PB_SPECIAL_STAR       Destroys matching color
0xB     PB_SPECIAL_MAGNETIC   Attracts projectile
0xC     PB_SPECIAL_RAINBOW    Matches any color
0xD     PB_SPECIAL_ICE        Requires multiple hits
0xE     Reserved
0xF     PB_FLAG_INDESTRUCTIBLE Blocker
```

## Size Examples

### Example 1: Standard 8x8 board, 6 colors
- Uncompressed: 8 * 8 = 64 cells @ 1 byte = 64 bytes
- Mode 0 (nibbles): 64 / 2 = 32 bytes + 4 header = 36 bytes
- Mode 1 (RLE): ~20-28 bytes typical
- Mode 2 (3-bit): 64 * 3/8 = 24 bytes + 4 header = 28 bytes

### Example 2: Full 16x12 board, 8 colors, sparse
- Uncompressed: 16 * 12 = 192 cells @ 1 byte = 192 bytes
- Mode 0 (nibbles): 192 / 2 = 96 bytes + 4 header = 100 bytes
- Mode 1 (RLE): ~40-60 bytes typical (half empty)
- Mode 2 (3-bit): 192 * 3/8 = 72 bytes + 4 header = 76 bytes

## API

```c
/* Decode result */
typedef struct pb_level_decode_result {
    int rows;
    int cols_even;
    int cols_odd;
    uint8_t cells[PB_MAX_CELLS];  /* Color/special per cell */
    int cell_count;
    pb_result status;
} pb_level_decode_result;

/* Decode compressed level to cell array */
pb_result pb_level_decode(
    const uint8_t* data,
    size_t size,
    pb_level_decode_result* out
);

/* Encode board to compressed format */
pb_result pb_level_encode(
    const pb_board* board,
    uint8_t* out,
    size_t out_size,
    size_t* bytes_written
);

/* Get minimum buffer size for encoding */
size_t pb_level_encode_size(const pb_board* board);
```

## Implementation Notes

1. **Endianness**: All multi-byte values are little-endian.

2. **Hex grid handling**: Odd rows have fewer columns. The decoder
   must track row parity for correct cell indexing.

3. **Bounds checking**: Decoder must validate that cell count matches
   rows * cols to prevent buffer overflows.

4. **Color mapping**: When color palette is present, stored colors
   0-7 are mapped through the palette to actual color_ids.

5. **Determinism**: RLE must use deterministic tie-breaking (prefer
   longer runs, left-to-right for equal runs).

## Cleanroom Note
This specification is independently designed based on observable
requirements for compact level storage. It does not copy any
proprietary format.

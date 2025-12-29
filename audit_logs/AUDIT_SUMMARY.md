# pb_core Static Analysis Audit Summary
Date: 2025-12-28

## Tools Used
- cppcheck 2.13.0 (C17 mode)
- flawfinder 2.0.19
- splint 3.1.2
- cpplint 2.0.2
- lizard 1.19.0 (cyclomatic complexity)

## Findings Summary

### Security Issues (flawfinder)
| Severity | Count | Description |
|----------|-------|-------------|
| Level 4  | 3     | strcpy/snprintf format string risks |
| Level 2  | 7+    | Static buffer char arrays |

Key issues:
- `pb_data.c:21` - snprintf format string risk
- `pb_data.c:458` - strcpy without bounds check
- `cJSON.c:456` - strcpy in vendor code (external)

### Code Quality (cppcheck)
| Type | Count | Description |
|------|-------|-------------|
| style | 45+  | Variables could be const |
| style | 40+  | Unused functions (library code, expected) |
| portability | 1 | Negative shift in freestanding.h |

### Cyclomatic Complexity (lizard)
Functions with CCN > 10 (complexity warning threshold):
- `sdl2_init`: CCN=11 (67 lines)
- `sdl2_poll_input`: CCN=14 (51 lines)
- `parse_bubble`: CCN=19 (52 lines)
- `pb_level_load_string`: CCN=11 (93 lines)
- `pb_theme_load_string`: CCN=15 (120 lines)
- `handle_input`: CCN=10 (38 lines)

### Code Style (cpplint)
- Most issues are in vendor cJSON code
- Some 80-char line length violations
- Header guard style differences (acceptable for project)

## Recommendations

### Immediate
1. Review strcpy usages in pb_data.c - consider safe alternatives
2. Add bounds checking on static char arrays
3. Refactor high-CCN functions (parse_bubble, pb_theme_load_string)

### Future
1. Consider const-correctness pass
2. Add test coverage for unused library functions
3. Address portability warning in pb_freestanding.h

## Files Analyzed
- pb_core/src/core/*.c (13 files)
- pb_core/src/platform/*.c (2 files)
- pb_core/src/data/*.c (1 file)
- pb_core/include/pb/*.h (22+ files)

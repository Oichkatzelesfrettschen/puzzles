# pb_core Build System

Comprehensive guide to building pb_core for desktop, embedded, and retro platforms.

## Quick Start

```bash
# Standard library build
make

# Run all tests (306 tests)
make test

# Fixed-point build (deterministic multiplayer)
FIXED_POINT=1 make test

# Debug build with sanitizers
DEBUG=1 make test

# Build SDL2 demo (requires SDL2, SDL2_image, SDL2_mixer)
make demo
```

## Docker Build Environment

Docker provides a consistent build environment with all 30+ cross-compilers pre-installed.

### Quick Docker Start

```bash
# Build the Docker image (first time only)
make docker-build-full

# Run tests inside Docker
make docker-test

# Test all cross-compilers inside Docker
make docker-cross-all

# Interactive shell with all compilers
make docker-shell

# Build for PDP-11 (via Docker)
make docker-pdp11
```

### Docker Images

| Image | Size | Contents |
|-------|------|----------|
| `pb-core-build` | ~1.5GB | Base compilers (30+ GCC cross-compilers) |
| `pb-core-build:full` | ~2GB | All compilers + LLVM-MOS (6502 C99) |
| `pb-core-build:minimal` | ~1GB | Essential compilers only |
| `jameshagerman/gcc-pdp11-aout` | ~500MB | PDP-11 cross-compiler |

### Docker Targets

```bash
# Build images
make docker-build           # Standard image
make docker-build-full      # With LLVM-MOS
make docker-build-minimal   # Minimal set

# Run make targets in Docker
make docker-make TARGET=test
make docker-make TARGET="FIXED_POINT=1 test"
make docker-make TARGET=cross-check-all

# Convenience shortcuts
make docker-test            # Run tests
make docker-cross-all       # All cross-compiler checks
make docker-build-all       # Build all architectures

# PDP-11 (via external Docker image)
make docker-pull-pdp11      # Pull PDP-11 image
make docker-pdp11           # Build for PDP-11

# Management
make docker-info            # Show Docker status
make docker-clean           # Remove pb-core images
```

### Docker Compose

```bash
docker-compose build                    # Build image
docker-compose run build make           # Run make
docker-compose run build make test      # Run tests
docker-compose run shell                # Interactive shell
docker-compose run pdp11 pdp11-aout-gcc --version
```

### Running Any Target in Docker

Use `DOCKER=1` prefix with any make target:

```bash
DOCKER=1 make test
DOCKER=1 make cross-check-all
DOCKER=1 FIXED_POINT=1 make lib
```

## Build Targets

| Target | Description |
|--------|-------------|
| `make` / `make lib` | Build static library `build/lib/libpb_core.a` |
| `make test` | Build and run all 306 tests |
| `make demo` | Build SDL2 graphical demo |
| `make tools` | Build CLI tools |
| `make examples` | Build example programs |
| `make audit` | Full static analysis (lint + check + complexity) |
| `make lint` | Quick cppcheck analysis |
| `make clean` | Remove all build artifacts |
| `make dirs` | Create build directory structure |

## Configuration Variables

### C Standard (`C_STD`)

```bash
C_STD=c89 make   # ANSI C (oldest, most portable)
C_STD=c99 make   # C99 (fixed-width types)
C_STD=c11 make   # C11 (atomics, better alignment)
C_STD=c17 make   # C17 (default, bug fixes)
```

**Note**: Some compilers don't support C17 (AVR-GCC uses C11 max).

### Size Tier (`SIZE_TIER`)

Controls feature/memory tradeoff for embedded targets:

| Tier | Board Size | Session | Features | RAM Target |
|------|------------|---------|----------|------------|
| `full` | 15 rows | Full | All | 512KB+ |
| `medium` | 12 rows | Full | Most | 128KB+ |
| `mini` | 10 rows | Basic | Core | 32KB+ |
| `micro` | 8 rows | None | Minimal | 4KB+ |

```bash
SIZE_TIER=micro make        # Minimal for 8-bit MCUs
SIZE_TIER=mini make         # Reduced for embedded
SIZE_TIER=medium make       # Balanced
SIZE_TIER=full make         # Desktop (default)
```

### Fixed-Point Arithmetic (`FIXED_POINT`)

Enables Q16.16 fixed-point instead of float for:
- Bit-perfect determinism across platforms
- Multiplayer synchronization
- Platforms without FPU
- Replay compatibility

```bash
FIXED_POINT=1 make          # Q16.16 fixed-point
FIXED_POINT=1 make test     # Verify determinism
```

**Fixed-Point Formats** (via `PB_FIXED_FORMAT`):
| Format | Precision | Range | Use Case |
|--------|-----------|-------|----------|
| Q16.16 (default) | 16 bits | ±32767 | Desktop, 32-bit MCU |
| Q12.12 | 12 bits | ±2047 | Memory-constrained |
| Q8.8 | 8 bits | ±127 | 8-bit MCU (6502, AVR) |

```bash
# Q8.8 for 8-bit targets
FIXED_POINT=1 EXTRA_CFLAGS="-DPB_FIXED_FORMAT=2" make
```

### Debug Mode (`DEBUG`)

Enables sanitizers and debug symbols:

```bash
DEBUG=1 make                # AddressSanitizer, UndefinedBehaviorSanitizer
DEBUG=1 make test           # Run tests with sanitizers
```

### Freestanding Mode (`PB_FREESTANDING`)

For bare-metal embedded without standard library:

```bash
EXTRA_CFLAGS="-DPB_FREESTANDING=1" make
```

Enables:
- Pure integer trigonometry (lookup tables)
- No stdio/stdlib dependencies
- Minimal memory footprint

## Cross-Compilation

### Syntax-Only Checks (Fast)

Verifies code compiles without producing object files:

```bash
make cross-check-all           # Test ALL available compilers
make cross-aarch64             # ARM 64-bit
make cross-riscv64             # RISC-V 64-bit
make cross-avr                 # AVR (Arduino)
make cross-6502-llvm           # 6502 (LLVM-MOS)
```

### Full Cross-Builds (Real Libraries)

Produces actual `.o` and `.a` files for each architecture:

```bash
make build-all-cross           # Build all available architectures
make build-aarch64             # ARM64 only
make build-riscv32             # RISC-V 32-bit (picolibc)
make build-6502                # 6502 via LLVM-MOS
```

**Output**: `build/cross/<arch>/lib/libpb_core.a`

### Successfully Built Architectures

| Category | Architectures |
|----------|---------------|
| 64-bit | x86_64, aarch64, alpha, mips64, ppc64, ppc64le, s390x, sparc64 |
| 32-bit | arm, m68k, mips, ppc |
| Embedded | avr, riscv32, riscv64 |
| 8-bit | 6502 (LLVM-MOS, Q8.8 fixed-point) |

## Installed Compilers

See `docs/INSTALLED_COMPILERS.md` for the complete audit of all 66 cross-compiler packages.

### Key Compiler Groups

**APT Packages (35 GCC cross-compilers)**:
```bash
sudo apt install gcc-aarch64-linux-gnu gcc-arm-none-eabi gcc-avr \
    gcc-riscv64-linux-gnu gcc-m68k-linux-gnu gcc-mingw-w64
```

**PPAs Required**:
```bash
sudo add-apt-repository ppa:vriviere/ppa       # Atari ST, SuperH
sudo add-apt-repository ppa:tkchia/build-ia16  # 8086 DOS
```

**LLVM-MOS (6502 C99)**:
```bash
cd /tmp && curl -LO https://github.com/llvm-mos/llvm-mos-sdk/releases/download/v22.4.0/llvm-mos-linux.tar.xz
sudo mkdir -p /opt/llvm-mos && sudo tar -xf llvm-mos-linux.tar.xz -C /opt/llvm-mos --strip-components=1
```

## Architecture-Specific Notes

### 8-bit Targets (AVR, 6502)

```bash
# AVR requires C11 (not C17) and SIZE_MICRO
SIZE_TIER=micro FIXED_POINT=1 make cross-avr

# 6502 requires fixed-point (no float) and freestanding
# Automatically configured in Makefile for build-6502
```

**Excluded modules for 6502**: `pb_color.c`, `pb_cvd.c` (require float)

### Freestanding Embedded

For bare-metal without libc:

```bash
# RISC-V with picolibc
make build-riscv32
make build-riscv64

# ARM Cortex-M (bare-metal)
make build-arm
```

### Windows Cross-Compilation

```bash
make cross-mingw32    # 32-bit Windows
make cross-mingw64    # 64-bit Windows
```

## Test Framework

Custom macro-based framework:

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

**Run single test file**:
```bash
make dirs lib && \
gcc -std=c17 -Wall -Wextra -Iinclude tests/test_hex.c -Lbuild/lib -lpb_core -lm -o test && \
./test
```

**Test all C standards**:
```bash
make check-all-standards
```

## Static Analysis

```bash
make audit       # Full analysis (cppcheck + lizard + flawfinder)
make lint        # Quick cppcheck only
```

**Tools required**:
```bash
sudo apt install cppcheck flawfinder python3-pip
pip install lizard
```

## Troubleshooting

### "unrecognized command line option '-std=c17'"

Some compilers (AVR, older GCC) don't support C17:
```bash
C_STD=c11 make cross-avr
```

### "total size of local objects too large" (AVR)

Use micro size tier:
```bash
SIZE_TIER=micro FIXED_POINT=1 make cross-avr
```

### "call to undeclared library function 'sinf'" (6502)

Enable freestanding mode for integer-only trig:
```bash
EXTRA_CFLAGS="-DPB_FREESTANDING=1" make
```

### "NEEDS LIBC" / "NEEDS NEWLIB"

Bare-metal targets need freestanding mode or picolibc:
```bash
# For RISC-V, picolibc is automatically used
sudo apt install picolibc-riscv64-unknown-elf
```

### Cross-compiler not found

Check installation:
```bash
make cross-check-all  # Shows status of all compilers
```

Install missing compilers:
```bash
sudo apt install gcc-<arch>-linux-gnu
```

## Directory Structure

```
pb_core/
├── build/
│   ├── bin/           # Test executables
│   ├── lib/           # Native libpb_core.a
│   ├── obj/           # Object files
│   └── cross/         # Cross-compiled libraries
│       ├── aarch64/lib/libpb_core.a
│       ├── avr/lib/libpb_core.a
│       ├── 6502/lib/libpb_core.a
│       └── ...
├── include/pb/        # Public headers
├── src/
│   ├── core/          # Core library (no dependencies)
│   ├── data/          # JSON loading (cJSON)
│   └── platform/      # SDL2 backend
├── tests/             # Test files
├── tools/             # CLI utilities
└── docs/              # Documentation
```

## Integration Example

```c
#include <pb/pb_core.h>

int main(void) {
    pb_session session;
    pb_session_init(&session);

    // Load level
    pb_level level;
    pb_load_level(&level, "levels/001.json");

    // Game loop
    while (session.state == PB_STATE_PLAYING) {
        pb_session_update(&session);
    }

    return 0;
}
```

**Compile**:
```bash
gcc -std=c17 -Iinclude main.c -Lbuild/lib -lpb_core -lm -o game
```

## Version Information

- **Build System**: GNU Make 4.3+
- **Default C Standard**: C17
- **Tested Compilers**: GCC 11+, Clang 14+, LLVM-MOS 22.x
- **Cross-Architectures**: 30+ (syntax-checked), 16 (full builds)

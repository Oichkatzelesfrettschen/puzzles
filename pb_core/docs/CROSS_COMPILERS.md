# Cross-Compilation Matrix

This document tracks pb_core's cross-compilation support across various CPU architectures. The library is designed for maximum portability from 8-bit microcontrollers to 64-bit servers.

## Quick Start

```bash
# Test all available cross-compilers
make cross-check-all

# Test specific architecture
make cross-aarch64
make cross-riscv64
make cross-avr

# Test all C standards
make check-all-standards
```

## Build Requirements

The library uses syntax-only checking (`-fsyntax-only`) for cross-compilation tests. This verifies code compiles without requiring target-specific libraries.

For bare-metal targets (AVR, ARM Cortex-M, RISC-V, etc.), use `SIZE_TIER=micro` and `FIXED_POINT=1` for minimal memory footprint.

## Architecture Matrix

### 64-bit Linux Architectures

| Architecture | Compiler | Package | Status | Notes |
|-------------|----------|---------|--------|-------|
| x86-64 | gcc | build-essential | ✅ OK | Primary development target |
| AArch64 | aarch64-linux-gnu-gcc | gcc-aarch64-linux-gnu | ✅ OK | ARM 64-bit |
| RISC-V 64 | riscv64-linux-gnu-gcc | gcc-riscv64-linux-gnu | ⚠️ NEEDS NEWLIB | Bare-metal, no libc |
| Alpha | alpha-linux-gnu-gcc | gcc-alpha-linux-gnu | ✅ OK | DEC Alpha |
| MIPS64 BE | mips64-linux-gnuabi64-gcc | gcc-mips64-linux-gnuabi64 | ✅ OK | Big-endian |
| MIPS64 LE | mips64el-linux-gnuabi64-gcc | gcc-mips64el-linux-gnuabi64 | ✅ OK | Little-endian |
| PowerPC64 BE | powerpc64-linux-gnu-gcc | gcc-powerpc64-linux-gnu | ✅ OK | Big-endian |
| PowerPC64 LE | powerpc64le-linux-gnu-gcc | gcc-powerpc64le-linux-gnu | ✅ OK | Little-endian |
| S/390x | s390x-linux-gnu-gcc | gcc-s390x-linux-gnu | ✅ OK | IBM Mainframe |
| SPARC64 | sparc64-linux-gnu-gcc | gcc-sparc64-linux-gnu | ✅ OK | Sun/Oracle |
| LoongArch64 | loongarch64-linux-gnu-gcc-14 | gcc-14-loongarch64-linux-gnu | ✅ OK | Chinese MIPS successor |
| HP-PA64 | hppa64-linux-gnu-gcc | gcc-hppa64-linux-gnu | ✅ OK | PA-RISC 2.0 |

### 32-bit Linux Architectures

| Architecture | Compiler | Package | Status | Notes |
|-------------|----------|---------|--------|-------|
| i386 | gcc -m32 | gcc-multilib | ✅ OK | x86 32-bit |
| ARM Cortex-M | arm-none-eabi-gcc | gcc-arm-none-eabi | ✅ OK | Embedded ARM |
| ARC | arc-linux-gnu-gcc | gcc-arc-linux-gnu | ✅ OK | Synopsys ARC |
| HP-PA | hppa-linux-gnu-gcc | gcc-hppa-linux-gnu | ✅ OK | PA-RISC 1.x |
| MIPS BE | mips-linux-gnu-gcc | gcc-mips-linux-gnu | ✅ OK | Big-endian |
| MIPS LE | mipsel-linux-gnu-gcc | gcc-mipsel-linux-gnu | ✅ OK | Little-endian |
| PowerPC 32 | powerpc-linux-gnu-gcc | gcc-powerpc-linux-gnu | ✅ OK | PowerPC 32-bit |
| SH4 | sh4-linux-gnu-gcc | gcc-sh4-linux-gnu | ✅ OK | Dreamcast/embedded |
| M68K Linux | m68k-linux-gnu-gcc | gcc-m68k-linux-gnu | ✅ OK | Amiga/Genesis |
| M68K Atari | m68k-atari-mint-gcc | vriviere/gcc-m68k PPA | ✅ OK | Atari ST/MiNT |

### 32-bit Embedded/Bare-Metal

| Architecture | Compiler | Package | Status | Notes |
|-------------|----------|---------|--------|-------|
| SuperH | sh-elf-gcc | vriviere/gcc-sh PPA | ✅ OK | Casio calculators |
| OpenRISC | or1k-elf-gcc | gcc-or1k-elf | ✅ OK | Open-source ISA |
| RISC-V 32 | riscv64-unknown-elf-gcc | gcc-riscv64-unknown-elf + picolibc | ✅ OK | Embedded, multilib 32/64 |
| RISC-V 64 | riscv64-unknown-elf-gcc | gcc-riscv64-unknown-elf + picolibc | ✅ OK | Embedded with picolibc |

### 16-bit Architectures

| Architecture | Compiler | Package | Status | Notes |
|-------------|----------|---------|--------|-------|
| 8086/DOS | ia16-elf-gcc | tkchia/build-ia16 PPA | ✅ OK | Real-mode DOS |
| MSP430 | msp430-elf-gcc | Not packaged | ❌ NOT INSTALLED | TI official toolchain |
| H8/300 | h8300-hms-gcc | gcc-h8300-hms | ⚠️ NEEDS LIBC | Bare-metal |

### 8-bit Architectures

| Architecture | Compiler | Package | Status | Notes |
|-------------|----------|---------|--------|-------|
| AVR | avr-gcc | gcc-avr | ⚠️ NEEDS LIBC | Arduino/ATmega |
| Xtensa LX106 | xtensa-lx106-elf-gcc | gcc-xtensa-lx106 | ✅ OK | ESP8266 |
| Z80 (SDCC) | sdcc -mz80 | sdcc | ✅ OK | Small Device C Compiler |
| Z80 (z88dk) | z88dk.zcc | z88dk | ❌ FAIL | Build issues |
| 6502 (cc65) | cc65 | cc65 | ⚠️ C89 only | No C99, 64KB limit |
| 6502 (LLVM) | mos-common-clang | LLVM-MOS SDK | ✅ OK | C99, Q8.8 fixed-point |
| C64 | mos-c64-clang | LLVM-MOS SDK | ✅ OK | Commodore 64 |
| NES | mos-nes-clang | LLVM-MOS SDK | ✅ OK | Nintendo Entertainment System |
| Atari 2600 | mos-atari2600-clang | LLVM-MOS SDK | ✅ OK | Atari VCS |

### Windows Cross-Compilation

| Architecture | Compiler | Package | Status | Notes |
|-------------|----------|---------|--------|-------|
| Windows 32 | i686-w64-mingw32-gcc | gcc-mingw-w64-i686 | ✅ OK | Win32 API |
| Windows 64 | x86_64-w64-mingw32-gcc | gcc-mingw-w64-x86-64 | ✅ OK | Win64 API |

## Installation

### Ubuntu/Debian Apt Packages

```bash
# Essential cross-compilers
sudo apt install gcc-aarch64-linux-gnu gcc-arm-none-eabi \
    gcc-riscv64-linux-gnu gcc-avr

# RISC-V embedded (bare-metal with picolibc)
sudo apt install gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf

# Exotic architectures (Ubuntu 24.04+)
sudo apt install gcc-alpha-linux-gnu gcc-arc-linux-gnu \
    gcc-hppa-linux-gnu gcc-hppa64-linux-gnu \
    gcc-mips-linux-gnu gcc-mipsel-linux-gnu \
    gcc-mips64-linux-gnuabi64 gcc-mips64el-linux-gnuabi64 \
    gcc-powerpc-linux-gnu gcc-powerpc64-linux-gnu gcc-powerpc64le-linux-gnu \
    gcc-s390x-linux-gnu gcc-sparc64-linux-gnu gcc-sh4-linux-gnu \
    gcc-or1k-elf gcc-h8300-hms gcc-xtensa-lx106 \
    gcc-14-loongarch64-linux-gnu gcc-m68k-linux-gnu

# Windows cross-compile
sudo apt install gcc-mingw-w64

# 8-bit/retro (traditional)
sudo apt install sdcc cc65 z88dk
```

### LLVM-MOS (Modern 6502 Compiler)

LLVM-MOS provides C99/C++11 support for 6502-based systems. No float support - requires fixed-point math.

```bash
# Download and install LLVM-MOS SDK
cd /tmp
curl -LO https://github.com/llvm-mos/llvm-mos-sdk/releases/download/v22.4.0/llvm-mos-linux.tar.xz
sudo mkdir -p /opt/llvm-mos
sudo tar -xf llvm-mos-linux.tar.xz -C /opt/llvm-mos --strip-components=1

# Add to PATH (optional)
export PATH="/opt/llvm-mos/bin:$PATH"
```

Supported targets include:
- `mos-c64-clang` - Commodore 64
- `mos-nes-clang` - Nintendo Entertainment System
- `mos-atari2600-*-clang` - Atari 2600/VCS
- `mos-atari8-*-clang` - Atari 8-bit computers
- `mos-pet-clang` - Commodore PET
- `mos-vic20-clang` - Commodore VIC-20
- `mos-common-clang` - Generic 6502

### PPAs Required

```bash
# Atari ST / MiNT cross-compiler
sudo add-apt-repository ppa:vriviere/gcc-m68k
sudo apt install gcc-m68k-atari-mint

# SuperH (Casio calculators)
sudo add-apt-repository ppa:vriviere/gcc-sh
sudo apt install gcc-sh-elf

# 8086/DOS (real-mode x86)
sudo add-apt-repository ppa:tkchia/build-ia16
sudo apt install gcc-ia16-elf
```

## Status Legend

- ✅ **OK** - Compiles successfully
- ⚠️ **NEEDS LIBC/NEWLIB** - Bare-metal target, requires freestanding mode
- ⚠️ **C99/SIZE** - Compiler limitations (C89 only or memory constraints)
- ❌ **FAIL** - Build errors requiring fixes
- ❌ **NOT INSTALLED** - Compiler not available via apt

## Known Limitations

### Bare-Metal Targets
Targets marked "NEEDS LIBC" compile in freestanding mode but lack standard library support. Use `SIZE_TIER=micro` which removes stdio/stdlib dependencies:

```bash
SIZE_TIER=micro FIXED_POINT=1 make cross-avr
```

### 6502 Compilers

**cc65** (traditional):
- C89 only (no C99 features)
- 64KB address space limit
- No 32-bit integers
- Good library support for many platforms

**LLVM-MOS** (modern):
- C99/C++11 support
- No float/double (use `FIXED_POINT=1` with `PB_FIXED_FORMAT=2` for Q8.8)
- Superior code generation (outperforms cc65 by ~10-20%)
- Targets: C64, NES, Atari 2600/8-bit, VIC-20, PET, and more
- Install from: https://github.com/llvm-mos/llvm-mos-sdk/releases

### z88dk
Currently has build issues. SDCC provides better Z80 support.

### MSP430
TI's official MSP430 toolchain requires manual installation from [TI's website](https://www.ti.com/tool/MSP430-GCC-OPENSOURCE).

## Compilers Not Packaged

These require building from source:

| Target | Source | Notes |
|--------|--------|-------|
| PDP-11 | [jrengdahl/PDP-11](https://github.com/jrengdahl/PDP-11) | GCC 13 bare-metal, build scripts |
| PDP-11 (Docker) | [jameshagerman/gcc-pdp11-aout](https://hub.docker.com/r/jameshagerman/gcc-pdp11-aout) | GCC 9.2 via Docker |
| Nios II | Deprecated GCC 15 | Intel FPGA soft-core |
| MicroBlaze | Xilinx tools | AMD/Xilinx FPGA soft-core |
| CRIS | Build from source | Axis Communications |
| Retro68 | GitHub | Classic Mac 68K |
| DJGPP | Build from source | DOS 32-bit protected mode |
| OpenWatcom | owopen.org | DOS/OS2/Windows |

### PDP-11 Installation Options

The PDP-11 minicomputer (1970-1990s) requires manual build. Two options:

**Option 1: Build from source (GCC 13)**
```bash
git clone https://github.com/jrengdahl/PDP-11.git
cd PDP-11
# Edit build-13.sh for your paths
./build-13.sh
export PATH="$HOME/PDP-11/pdp11-aout-13/bin:$PATH"
```

**Option 2: Docker (GCC 9.2)**
```bash
docker pull jameshagerman/gcc-pdp11-aout
docker run -v $(pwd):/work jameshagerman/gcc-pdp11-aout pdp11-aout-gcc -o output input.c
```

Note: PDP-11/05 lacks hardware multiply/divide - use software emulation.
Output is a.out format requiring conversion to DEC LDA for SIMH.

## Adding New Architectures

1. Install the cross-compiler
2. Add a target in `Makefile`:
   ```makefile
   cross-newarch:
       @echo -n "New Architecture... "
       @if command -v newarch-gcc >/dev/null 2>&1; then \
           newarch-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
       else echo "NOT INSTALLED"; fi
   ```
3. Add to `cross-check-all` in appropriate section
4. Update this document

## Version History

- **2024-12-29 (v3)**: Added PDP-11 documentation (Docker/build-from-source), full cross-build targets producing real .o/.a files for 16 architectures
- **2024-12-29 (v2)**: Added LLVM-MOS for 6502 C99 support, RISC-V embedded with picolibc, retro console targets (C64, NES, Atari 2600)
- **2024-12-29**: Added 18 new architectures (Alpha, ARC, HPPA, MIPS variants, PowerPC variants, S/390x, SPARC64, LoongArch64, SH4, OpenRISC, H8/300, Xtensa, MinGW)
- **2024-12-28**: Initial cross-compilation matrix with ARM, RISC-V, AVR, Z80, 6502, M68K

# Cross-Compiler Setup for pb_core

This document describes how to set up the cross-compilation toolchain for testing pb_core across multiple architectures on Ubuntu 24.04 LTS (Noble Numbat).

## Quick Start

```bash
# Install all available cross-compilers from Ubuntu repos
sudo apt-get update
sudo apt-get install -y \
    gcc-multilib libc6-dev-i386 \
    gcc-aarch64-linux-gnu \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf \
    gcc-sh-elf libnewlib-sh-elf-dev \
    gcc-m68k-linux-gnu \
    gcc-avr avr-libc \
    sdcc sdcc-libraries \
    cc65

# Add PPAs for additional compilers
sudo add-apt-repository -y ppa:tkchia/build-ia16      # 8086/DOS
sudo add-apt-repository -y ppa:vriviere/ppa           # Atari ST/MiNT
sudo apt-get update
sudo apt-get install -y gcc-ia16-elf cross-mint-essential

# Install z88dk via snap (Z80 with 100+ targets)
sudo snap install --edge z88dk

# Run the cross-compilation matrix
make -C pb_core cross-check-all
```

## Architecture Support Matrix

### Current Build Status

```
============================================
=== FULL CROSS-COMPILATION MATRIX =========
============================================

--- 64-bit Architectures ---
x86-64 (gcc).............. OK
AArch64................... OK
RISC-V 64................. OK (needs headers for linking)

--- 32-bit Architectures ---
i386 (gcc -m32)........... OK
ARM Cortex-M.............. OK
SuperH (Casio)............ OK
M68K (Amiga/Genesis)...... OK (linux)
M68K (Atari ST/MiNT)...... OK

--- 16-bit Architectures ---
8086/DOS (ia16)........... OK
MSP430 (TI)............... NOT INSTALLED (manual)

--- 8-bit Architectures ---
AVR (Arduino)............. OK (needs headers for linking)
Z80 (SDCC)................ OK
Z80 (z88dk)............... FAIL (C99 enum issues)
6502 (cc65)............... FAIL (C99/size limits)

============================================
```

### Detailed Compiler Matrix

| Arch | Bits | Compiler | Version | Package | Target Platforms |
|------|------|----------|---------|---------|------------------|
| **x86-64** | 64 | gcc | 13.3.0 | gcc | Linux, Windows (WSL) |
| **x86-64** | 64 | clang | 18.1.3 | clang | Linux, Windows (WSL) |
| **AArch64** | 64 | aarch64-linux-gnu-gcc | 13.3.0 | gcc-aarch64-linux-gnu | ARM64 Linux (RPi4, AWS) |
| **RISC-V** | 64 | riscv64-unknown-elf-gcc | 13.2.0 | gcc-riscv64-unknown-elf | RISC-V embedded |
| **RISC-V** | 64 | riscv64-linux-gnu-gcc | 13.3.0 | gcc-riscv64-linux-gnu | RISC-V Linux |
| **i386** | 32 | gcc -m32 | 13.3.0 | gcc-multilib | x86 Linux, DOS (via ia16) |
| **ARM** | 32 | arm-none-eabi-gcc | 13.2.1 | gcc-arm-none-eabi | Cortex-M (STM32, Arduino) |
| **SuperH** | 32 | sh-elf-gcc | 13.2.0 | gcc-sh-elf | Casio Calculators, Sega Saturn |
| **M68K** | 32 | m68k-linux-gnu-gcc | 13.3.0 | gcc-m68k-linux-gnu | Linux/m68k |
| **M68K** | 32 | m68k-atari-mint-gcc | 4.6.4 | cross-mint-essential (PPA) | Atari ST/TT/Falcon |
| **8086** | 16 | ia16-elf-gcc | 6.3.0 | gcc-ia16-elf (PPA) | MS-DOS, FreeDOS, ELKS |
| **AVR** | 8 | avr-gcc | 7.3.0 | gcc-avr | Arduino, ATmega, ATtiny |
| **Z80** | 8 | sdcc | 4.2.0 | sdcc | ZX Spectrum, MSX, Game Boy, 8051 |
| **Z80** | 8 | z88dk.zcc | 24183 | snap:z88dk | 100+ Z80 machines |
| **6502** | 8 | cc65 | 2.18 | cc65 | C64, NES, Apple II, Atari |

### Platform Details

#### 64-bit Architectures

| Platform | Endian | Pointer | Typical Targets | pb_core Tier |
|----------|--------|---------|-----------------|--------------|
| x86-64 | Little | 8 bytes | Desktop Linux, Windows WSL | FULL |
| AArch64 | Little | 8 bytes | Raspberry Pi 4+, AWS Graviton | FULL |
| RISC-V 64 | Little | 8 bytes | SiFive boards, QEMU | FULL |

#### 32-bit Architectures

| Platform | Endian | Pointer | Typical Targets | pb_core Tier |
|----------|--------|---------|-----------------|--------------|
| i386 | Little | 4 bytes | Legacy Linux, DOS (with djgpp) | FULL/MEDIUM |
| ARM Cortex-M | Little | 4 bytes | STM32, Arduino Due, ESP32 | MEDIUM |
| SuperH | Big | 4 bytes | Casio fx-9860, Sega Saturn | MEDIUM |
| M68K | Big | 4 bytes | Amiga, Atari ST, Sega Genesis | MEDIUM |

#### 16-bit Architectures

| Platform | Endian | Pointer | Typical Targets | pb_core Tier |
|----------|--------|---------|-----------------|--------------|
| 8086 (IA-16) | Little | 2 bytes | MS-DOS, FreeDOS, PC BIOS | MINI |
| MSP430 | Little | 2 bytes | TI Launchpad, IoT sensors | MINI |

#### 8-bit Architectures

| Platform | Endian | Pointer | Typical Targets | pb_core Tier |
|----------|--------|---------|-----------------|--------------|
| AVR | Little | 2 bytes | Arduino Uno/Mega, ATtiny | MICRO |
| Z80 | Little | 2 bytes | ZX Spectrum, Game Boy, MSX | MICRO |
| 6502 | Little | 2 bytes | C64, NES, Apple II | MICRO |

## Support Matrix

## Detailed Installation

### Ubuntu Repository Packages

#### 32-bit x86 Support (gcc -m32)

Required for compiling with `-m32` flag:

```bash
sudo apt-get install gcc-multilib libc6-dev-i386
```

#### ARM Bare-Metal (Cortex-M/R)

For embedded ARM development (Arduino, STM32, etc.):

```bash
sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi
# Optional: C++ support
sudo apt-get install libstdc++-arm-none-eabi-newlib
```

Provides: `arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-objcopy`, etc.

#### RISC-V 64-bit Bare-Metal

For embedded RISC-V development:

```bash
sudo apt-get install gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf
# Alternative with newlib:
sudo apt-get install libnewlib-riscv64-unknown-elf
```

Provides: `riscv64-unknown-elf-gcc`

#### SuperH (Casio Calculators, Sega Saturn)

For SuperH embedded development:

```bash
sudo apt-get install gcc-sh-elf libnewlib-sh-elf-dev
```

Provides: `sh-elf-gcc` (SH-1 through SH-4a support)

**Note:** For Casio calculator development, consider the fxSDK toolchain from [Planete Casio](https://git.planet-casio.com/Lephenixnoir/sh-elf-gcc).

#### M68K (Amiga, Genesis, Classic Mac)

For Linux-targeted M68K cross-compilation:

```bash
sudo apt-get install gcc-m68k-linux-gnu
```

Provides: `m68k-linux-gnu-gcc`

**Note:** For bare-metal (AmigaOS, Genesis/Megadrive), build from source:
- [m68k-elf-toolchain](https://github.com/ddraig68k/m68k-elf-toolchain)
- [rosco-m68k toolchain](https://rosco-m68k.com/docs/toolchain-installation)

#### AVR (Arduino, ATmega, ATtiny)

For Arduino and AVR microcontrollers:

```bash
sudo apt-get install gcc-avr avr-libc binutils-avr
```

Provides: `avr-gcc`

#### SDCC (Z80, 8051, STM8)

Small Device C Compiler for 8-bit architectures:

```bash
sudo apt-get install sdcc sdcc-libraries sdcc-doc
# Optional: Simulator
sudo apt-get install sdcc-ucsim
```

Provides: `sdcc` (use with `-mz80`, `-mz180`, `-mstm8`, `-mmos6502`, etc.)

Supported architectures: Z80, Z180, Z80N, GBZ80, 8051, DS390, STM8, MOS 6502

#### CC65 (6502, C64, NES, Apple II)

For 6502-based systems:

```bash
sudo apt-get install cc65 cc65-doc
```

Provides: `cc65`, `ca65`, `ld65`

Supported targets: C64, C128, VIC-20, Plus/4, CBM-II, Apple II, Atari, NES, PCE, Lynx, Supervision

### PPA Packages

#### IA-16 / 8086 / DOS (PPA: tkchia/build-ia16)

For MS-DOS and 16-bit x86 development:

```bash
sudo add-apt-repository ppa:tkchia/build-ia16
sudo apt-get update
sudo apt-get install gcc-ia16-elf
# Optional: DOS C library
sudo apt-get install libi86-ia16-elf
```

Provides: `ia16-elf-gcc`

**Reference:** [gcc-ia16 on GitHub](https://github.com/tkchia/gcc-ia16)

#### M68K Atari/MiNT (PPA: vriviere/ppa)

For Atari ST/TT/Falcon development with MiNT:

```bash
sudo add-apt-repository ppa:vriviere/ppa
sudo apt-get update
sudo apt-get install cross-mint-essential
```

Provides: `m68k-atari-mint-gcc`, `m68k-atari-mint-g++`

Installs to `/opt/cross-mint/`. Optional libraries available:
- `cflib-m68k-atari-mint`, `gemma-m68k-atari-mint`, `sdl-m68k-atari-mint`

**Reference:** [Vincent Rivière's cross-tools](http://vincent.riviere.free.fr/soft/m68k-atari-mint/ubuntu.php)

### Snap Packages

#### z88dk (Z80 Development Kit)

For Z80 family machines (ZX Spectrum, Amstrad CPC, MSX, etc.):

```bash
sudo snap install z88dk
```

Provides: `z88dk.zcc` (C compiler), `z88dk.z80asm` (assembler)

Supports 100+ Z80-based targets including Sinclair ZX Spectrum, Amstrad CPC, MSX, Sega Master System, CP/M.

**Reference:** [z88dk on GitHub](https://github.com/z88dk/z88dk)

### Manual Installation

#### MSP430 (Texas Instruments)

TI provides the official MSP430-GCC toolchain. Ubuntu's `binutils-msp430` package is outdated.

1. Download from [TI MSP430-GCC-OPENSOURCE](https://www.ti.com/tool/MSP430-GCC-OPENSOURCE)
2. Install to `/opt/ti/msp430/gcc`:

```bash
# Example (version may vary)
wget https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-LlCjWuAbzH/9.3.1.2/msp430-gcc-9.3.1.2_linux64.tar.bz2
sudo mkdir -p /opt/ti/msp430
sudo tar -xjf msp430-gcc-*.tar.bz2 -C /opt/ti/msp430
sudo ln -s /opt/ti/msp430/msp430-gcc-*/bin/msp430-elf-gcc /usr/local/bin/
```

Provides: `msp430-elf-gcc`

#### M68K Bare-Metal (ELF)

For Genesis/Megadrive, Amiga NDOS, or rosco_m68k:

```bash
git clone https://github.com/ddraig68k/m68k-elf-toolchain.git
cd m68k-elf-toolchain
make PREFIX=/opt/m68k-elf
sudo make install PREFIX=/opt/m68k-elf
echo 'export PATH=/opt/m68k-elf/bin:$PATH' >> ~/.bashrc
```

Provides: `m68k-elf-gcc`

## Current System Status

Run `make -C pb_core info` to see which compilers are installed:

```
=== Installed Cross-Compilers ===
  gcc: installed
  aarch64-linux-gnu-gcc: installed
  arm-none-eabi-gcc: not installed
  riscv64-elf-gcc: not installed
  ...
```

Run `make -C pb_core cross-check-all` to test all architectures:

```
============================================
=== FULL CROSS-COMPILATION MATRIX =========
============================================

--- 64-bit Architectures ---
x86-64 (gcc)... OK
AArch64... OK
RISC-V 64... OK

--- 32-bit Architectures ---
i386 (gcc -m32)... OK
ARM Cortex-M... OK
...
```

## Compiler Naming Conventions

The pb_core Makefile detects compilers using these binary names:

| Makefile Target | Primary Binary | Ubuntu Alternative |
|-----------------|----------------|-------------------|
| `cross-riscv64` | `riscv64-elf-gcc` | `riscv64-unknown-elf-gcc` |
| `cross-sh` | `sh-elf-gcc` | `sh-elf-gcc` |
| `cross-m68k` | `m68k-elf-gcc` | `m68k-linux-gnu-gcc` |

The Makefile includes fallback detection for Ubuntu package naming conventions.

## Troubleshooting

### i386 (-m32) Fails with "bits/libc-header-start.h: No such file"

Install 32-bit development libraries:

```bash
sudo apt-get install gcc-multilib libc6-dev-i386
```

### "command not found" for installed compiler

Check if the package provides a differently-named binary:

```bash
dpkg -L gcc-riscv64-unknown-elf | grep bin/
```

### RISC-V/SuperH "NEEDS NEWLIB" warning

Install the corresponding libc:

```bash
# RISC-V
sudo apt-get install picolibc-riscv64-unknown-elf
# SuperH
sudo apt-get install libnewlib-sh-elf-dev
```

### AVR/MSP430 "NEEDS LIBC" warning

Install the AVR or MSP430 C library:

```bash
sudo apt-get install avr-libc  # AVR
```

### SDCC "ICE" (Internal Compiler Error)

SDCC has limitations with C99 features. The pb_core tests use only basic syntax checking for Z80.

### cc65 "C99/SIZE" warning

cc65 is a C89 compiler with limited C99 support. Complex data structures may exceed memory limits.

## Architecture Notes

### Bare-Metal vs Linux Targets

- **Bare-metal** (`*-elf-gcc`, `*-none-eabi-gcc`): For embedded systems without OS, uses newlib/picolibc
- **Linux** (`*-linux-gnu-gcc`): For cross-compiling Linux userspace programs, uses glibc

pb_core uses `-fsyntax-only` for cross-compilation checks, so either works for validation.

### Freestanding Mode

Embedded targets use `-ffreestanding` which:
- Disables hosted C library assumptions
- Only requires `<float.h>`, `<limits.h>`, `<stdarg.h>`, `<stddef.h>`
- pb_core's `PB_SIZE_MICRO` tier is designed for this environment

## References

- [GCC Cross-Compiler - OSDev Wiki](https://wiki.osdev.org/GCC_Cross-Compiler)
- [ia16-elf-gcc PPA](https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16)
- [Vincent Rivière's m68k-atari-mint PPA](https://launchpad.net/~vriviere/+archive/ubuntu/ppa)
- [TI MSP430-GCC](https://www.ti.com/tool/MSP430-GCC-OPENSOURCE)
- [Planete Casio sh-elf-gcc](https://git.planet-casio.com/Lephenixnoir/sh-elf-gcc)
- [m68k-elf-toolchain](https://github.com/ddraig68k/m68k-elf-toolchain)
- [rosco-m68k Toolchain](https://rosco-m68k.com/docs/toolchain-installation)
- [z88dk Z80 Development Kit](https://github.com/z88dk/z88dk)
- [cc65 6502 Compiler](https://cc65.github.io/)

# Installed Cross-Compilers Audit

This document provides a complete audit of all cross-compilers installed on this Ubuntu 24.04 LTS system for the pb_core project.

**Audit Date**: 2024-12-29
**System**: Ubuntu 24.04 LTS (Noble Numbat) on WSL2

## Summary

| Category | Count |
|----------|-------|
| GCC Cross-Compilers (apt) | 35 |
| Binutils Packages (apt) | 28 |
| Retro/8-bit Compilers (apt) | 2 |
| Snap Packages | 1 |
| Manual Installations | 1 |
| PPAs Added | 2 |

---

## PPAs (Personal Package Archives)

### 1. vriviere/ppa - Vincent Rivière's Cross-Compilers

**URL**: `ppa:vriviere/ppa`
**Launchpad**: https://launchpad.net/~vriviere/+archive/ubuntu/ppa

Provides Atari ST/MiNT and SuperH cross-compilers.

```bash
sudo add-apt-repository ppa:vriviere/ppa
```

**Packages installed from this PPA**:
| Package | Version | Description |
|---------|---------|-------------|
| gcc-m68k-atari-mint | - | Atari ST/TT/Falcon (MiNT OS) |
| gcc-sh-elf | - | SuperH (Casio calculators) |
| binutils-m68k-atari-mint | - | Binutils for Atari ST |
| binutils-sh-elf | - | Binutils for SuperH |

### 2. tkchia/build-ia16 - TK Chia's 8086 Toolchain

**URL**: `ppa:tkchia/build-ia16`
**Launchpad**: https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16

Provides 16-bit x86 (8086/8088) cross-compiler for DOS.

```bash
sudo add-apt-repository ppa:tkchia/build-ia16
```

**Packages installed from this PPA**:
| Package | Version | Description |
|---------|---------|-------------|
| gcc-ia16-elf | - | 8086/8088 real-mode DOS |
| binutils-ia16-elf | - | Binutils for 8086 |
| libnewlib-ia16-elf | - | Newlib C library for 8086 |

---

## APT Packages - GCC Cross-Compilers

### 64-bit Linux Targets

| Package | Target | Architecture |
|---------|--------|--------------|
| gcc-aarch64-linux-gnu | Linux | ARM64/AArch64 |
| gcc-alpha-linux-gnu | Linux | DEC Alpha |
| gcc-hppa64-linux-gnu | Linux | HP PA-RISC 2.0 64-bit |
| gcc-14-loongarch64-linux-gnu | Linux | LoongArch 64-bit |
| gcc-mips64-linux-gnuabi64 | Linux | MIPS64 (big-endian) |
| gcc-mips64el-linux-gnuabi64 | Linux | MIPS64 (little-endian) |
| gcc-powerpc64-linux-gnu | Linux | PowerPC 64-bit BE |
| gcc-powerpc64le-linux-gnu | Linux | PowerPC 64-bit LE |
| gcc-riscv64-linux-gnu | Linux | RISC-V 64-bit |
| gcc-s390x-linux-gnu | Linux | IBM S/390x Mainframe |
| gcc-sparc64-linux-gnu | Linux | SPARC64 (Sun/Oracle) |

### 32-bit Linux Targets

| Package | Target | Architecture |
|---------|--------|--------------|
| gcc-arc-linux-gnu | Linux | Synopsys ARC |
| gcc-arm-linux-gnueabi | Linux | ARM 32-bit |
| gcc-hppa-linux-gnu | Linux | HP PA-RISC 1.x |
| gcc-m68k-linux-gnu | Linux | Motorola 68000 |
| gcc-mips-linux-gnu | Linux | MIPS (big-endian) |
| gcc-mipsel-linux-gnu | Linux | MIPS (little-endian) |
| gcc-powerpc-linux-gnu | Linux | PowerPC 32-bit |
| gcc-sh4-linux-gnu | Linux | SuperH4 |

### Embedded/Bare-Metal Targets

| Package | Target | Architecture |
|---------|--------|--------------|
| gcc-arm-none-eabi | Bare-metal | ARM Cortex-M/R |
| gcc-avr | Bare-metal | AVR (Arduino/ATmega) |
| gcc-h8300-hms | Bare-metal | Hitachi H8/300 |
| gcc-or1k-elf | Bare-metal | OpenRISC 1000 |
| gcc-riscv64-unknown-elf | Bare-metal | RISC-V 32/64-bit |
| gcc-xtensa-lx106 | Bare-metal | Xtensa LX106 (ESP8266) |

### Windows Cross-Compilation

| Package | Target | Architecture |
|---------|--------|--------------|
| gcc-mingw-w64 | Windows | Meta-package |
| gcc-mingw-w64-i686 | Windows | Win32 (32-bit) |
| gcc-mingw-w64-i686-posix | Windows | Win32 + POSIX threads |
| gcc-mingw-w64-i686-win32 | Windows | Win32 + Win32 threads |
| gcc-mingw-w64-x86-64 | Windows | Win64 (64-bit) |
| gcc-mingw-w64-x86-64-posix | Windows | Win64 + POSIX threads |
| gcc-mingw-w64-x86-64-win32 | Windows | Win64 + Win32 threads |

---

## APT Packages - Binutils

| Package | Target |
|---------|--------|
| binutils-aarch64-linux-gnu | ARM64 Linux |
| binutils-alpha-linux-gnu | Alpha Linux |
| binutils-arc-linux-gnu | ARC Linux |
| binutils-arm-linux-gnueabi | ARM Linux |
| binutils-arm-none-eabi | ARM bare-metal |
| binutils-avr | AVR |
| binutils-h8300-hms | H8/300 |
| binutils-hppa-linux-gnu | HP-PA Linux |
| binutils-hppa64-linux-gnu | HP-PA64 Linux |
| binutils-ia16-elf | 8086 DOS |
| binutils-loongarch64-linux-gnu | LoongArch64 Linux |
| binutils-m68k-atari-mint | Atari ST/MiNT |
| binutils-m68k-linux-gnu | M68K Linux |
| binutils-mingw-w64-i686 | Windows 32-bit |
| binutils-mingw-w64-x86-64 | Windows 64-bit |
| binutils-mips-linux-gnu | MIPS Linux |
| binutils-mips64-linux-gnuabi64 | MIPS64 Linux |
| binutils-mips64el-linux-gnuabi64 | MIPS64EL Linux |
| binutils-mipsel-linux-gnu | MIPSEL Linux |
| binutils-or1k-elf | OpenRISC |
| binutils-powerpc-linux-gnu | PowerPC Linux |
| binutils-powerpc64-linux-gnu | PowerPC64 Linux |
| binutils-powerpc64le-linux-gnu | PowerPC64LE Linux |
| binutils-riscv64-linux-gnu | RISC-V Linux |
| binutils-riscv64-unknown-elf | RISC-V bare-metal |
| binutils-s390x-linux-gnu | S/390x Linux |
| binutils-sh-elf | SuperH bare-metal |
| binutils-sh4-linux-gnu | SH4 Linux |
| binutils-sparc64-linux-gnu | SPARC64 Linux |
| binutils-xtensa-lx106 | Xtensa |

---

## APT Packages - Support Libraries

| Package | Version | Description |
|---------|---------|-------------|
| picolibc-riscv64-unknown-elf | 1.8.6-2 | Embedded C library for RISC-V |
| libnewlib-ia16-elf | - | Newlib for 8086 |

---

## APT Packages - Retro/8-bit Compilers

| Package | Version | Description |
|---------|---------|-------------|
| cc65 | 2.19-1 | 6502 cross-compiler (C64, Apple II, NES, etc.) |
| sdcc | 4.2.0+dfsg-1 | Small Device C Compiler (Z80, 8051, etc.) |
| sdcc-libraries | 4.2.0+dfsg-1 | SDCC standard libraries |
| sdcc-doc | 4.2.0+dfsg-1 | SDCC documentation |

---

## Snap Packages

| Package | Version | Channel | Description |
|---------|---------|---------|-------------|
| z88dk | 24183-41c54f4333-20251217 | latest/edge | Z80 Development Kit |

```bash
sudo snap install z88dk --edge
```

---

## Manual Installations

### LLVM-MOS SDK v22.4.0

**Location**: `/opt/llvm-mos/`
**Version**: Clang 22.0.0git
**Source**: https://github.com/llvm-mos/llvm-mos-sdk/releases

Modern C99/C++11 compiler for 6502-based systems (no float support).

**Installation**:
```bash
cd /tmp
curl -LO https://github.com/llvm-mos/llvm-mos-sdk/releases/download/v22.4.0/llvm-mos-linux.tar.xz
sudo mkdir -p /opt/llvm-mos
sudo tar -xf llvm-mos-linux.tar.xz -C /opt/llvm-mos --strip-components=1
```

**Available compilers**:
| Compiler | Target Platform |
|----------|-----------------|
| mos-common-clang | Generic 6502 |
| mos-c64-clang | Commodore 64 |
| mos-nes-clang | Nintendo Entertainment System |
| mos-atari2600-4k-clang | Atari 2600 (4K ROM) |
| mos-atari8-dos-clang | Atari 8-bit (DOS) |
| mos-vic20-clang | Commodore VIC-20 |
| mos-pet-clang | Commodore PET |
| mos-c128-clang | Commodore 128 |
| mos-cx16-clang | Commander X16 |
| mos-mega65-clang | MEGA65 |
| mos-lynx-clang | Atari Lynx |
| mos-pce-clang | PC Engine/TurboGrafx |

---

## Installation Commands

### One-liner for all APT cross-compilers

```bash
# Add PPAs first
sudo add-apt-repository -y ppa:vriviere/ppa
sudo add-apt-repository -y ppa:tkchia/build-ia16
sudo apt update

# Install all cross-compilers
sudo apt install -y \
  gcc-aarch64-linux-gnu gcc-alpha-linux-gnu gcc-arc-linux-gnu \
  gcc-arm-linux-gnueabi gcc-arm-none-eabi gcc-avr \
  gcc-h8300-hms gcc-hppa-linux-gnu gcc-hppa64-linux-gnu \
  gcc-ia16-elf gcc-14-loongarch64-linux-gnu \
  gcc-m68k-linux-gnu gcc-m68k-atari-mint \
  gcc-mingw-w64 \
  gcc-mips-linux-gnu gcc-mipsel-linux-gnu \
  gcc-mips64-linux-gnuabi64 gcc-mips64el-linux-gnuabi64 \
  gcc-or1k-elf gcc-powerpc-linux-gnu \
  gcc-powerpc64-linux-gnu gcc-powerpc64le-linux-gnu \
  gcc-riscv64-linux-gnu gcc-riscv64-unknown-elf \
  gcc-s390x-linux-gnu gcc-sh-elf gcc-sh4-linux-gnu \
  gcc-sparc64-linux-gnu gcc-xtensa-lx106 \
  picolibc-riscv64-unknown-elf \
  cc65 sdcc

# Snap packages
sudo snap install z88dk --edge
```

### LLVM-MOS Installation

```bash
cd /tmp
curl -LO https://github.com/llvm-mos/llvm-mos-sdk/releases/download/v22.4.0/llvm-mos-linux.tar.xz
sudo mkdir -p /opt/llvm-mos
sudo tar -xf llvm-mos-linux.tar.xz -C /opt/llvm-mos --strip-components=1
rm llvm-mos-linux.tar.xz
```

---

## Verification

Run the cross-compilation matrix test:

```bash
cd pb_core
make cross-check-all
```

Expected output shows 36+ architecture targets with status (OK, NEEDS LIBC, NOT INSTALLED, etc.).

---

## Architecture Coverage

| Bit Width | Architectures |
|-----------|---------------|
| 64-bit | x86-64, AArch64, Alpha, MIPS64, PowerPC64, RISC-V64, S/390x, SPARC64, LoongArch64, HP-PA64 |
| 32-bit | i386, ARM, ARC, HP-PA, M68K, MIPS, PowerPC, SH4, RISC-V32, OpenRISC |
| 16-bit | 8086 (DOS), MSP430*, H8/300 |
| 8-bit | AVR, Z80, 6502, Xtensa |

*MSP430 requires manual installation from TI

## Platform Coverage

| Platform | Compilers |
|----------|-----------|
| Linux | 20+ architectures |
| Windows | MinGW-w64 (32/64-bit) |
| DOS | ia16-elf (8086), DJGPP* |
| Atari ST | m68k-atari-mint |
| Commodore | LLVM-MOS (C64, VIC-20, PET, C128) |
| Nintendo | LLVM-MOS (NES) |
| Atari | LLVM-MOS (2600, 8-bit, Lynx) |
| Casio | sh-elf |
| Arduino | avr-gcc |
| ESP8266 | xtensa-lx106 |

*DJGPP requires manual build from source

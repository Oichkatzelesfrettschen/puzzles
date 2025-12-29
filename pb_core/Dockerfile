# pb_core Cross-Compilation Build Environment
# SPDX-License-Identifier: BSD-3-Clause
#
# Multi-architecture cross-compiler environment for pb_core
# Supports 30+ CPU architectures from 8-bit to 64-bit
#
# Build:
#   docker build -t pb-core-build .
#   docker build --build-arg INSTALL_LLVM_MOS=1 -t pb-core-build-full .
#
# Usage:
#   docker run -v $(pwd):/work pb-core-build make
#   docker run -v $(pwd):/work pb-core-build make cross-check-all
#   docker run -v $(pwd):/work pb-core-build make build-all-cross

FROM ubuntu:24.04

LABEL maintainer="pb_core"
LABEL description="Cross-compilation environment for pb_core (30+ architectures)"

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Build arguments for optional components
ARG INSTALL_LLVM_MOS=0
ARG INSTALL_PDP11=0

# Install base build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    make \
    git \
    curl \
    ca-certificates \
    xz-utils \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# Add PPAs for exotic compilers
RUN add-apt-repository -y ppa:vriviere/ppa && \
    add-apt-repository -y ppa:tkchia/build-ia16 && \
    apt-get update

# Install all GCC cross-compilers (organized by category)
RUN apt-get install -y --no-install-recommends \
    # 64-bit Linux targets
    gcc-aarch64-linux-gnu \
    gcc-alpha-linux-gnu \
    gcc-hppa64-linux-gnu \
    gcc-14-loongarch64-linux-gnu \
    gcc-mips64-linux-gnuabi64 \
    gcc-mips64el-linux-gnuabi64 \
    gcc-powerpc64-linux-gnu \
    gcc-powerpc64le-linux-gnu \
    gcc-riscv64-linux-gnu \
    gcc-s390x-linux-gnu \
    gcc-sparc64-linux-gnu \
    # 32-bit Linux targets
    gcc-arc-linux-gnu \
    gcc-arm-linux-gnueabi \
    gcc-hppa-linux-gnu \
    gcc-m68k-linux-gnu \
    gcc-mips-linux-gnu \
    gcc-mipsel-linux-gnu \
    gcc-powerpc-linux-gnu \
    gcc-sh4-linux-gnu \
    # Embedded/Bare-metal
    gcc-arm-none-eabi \
    gcc-avr \
    gcc-h8300-hms \
    gcc-or1k-elf \
    gcc-riscv64-unknown-elf \
    gcc-xtensa-lx106 \
    picolibc-riscv64-unknown-elf \
    # Windows cross-compilation
    gcc-mingw-w64 \
    # PPA compilers
    gcc-m68k-atari-mint \
    gcc-sh-elf \
    gcc-ia16-elf \
    # 8-bit/retro (traditional)
    sdcc \
    cc65 \
    && rm -rf /var/lib/apt/lists/*

# Install static analysis tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    cppcheck \
    python3-pip \
    && pip3 install --break-system-packages lizard \
    && rm -rf /var/lib/apt/lists/*

# Install LLVM-MOS (6502 C99 compiler) - optional
RUN if [ "$INSTALL_LLVM_MOS" = "1" ]; then \
    cd /tmp && \
    curl -LO https://github.com/llvm-mos/llvm-mos-sdk/releases/download/v22.4.0/llvm-mos-linux.tar.xz && \
    mkdir -p /opt/llvm-mos && \
    tar -xf llvm-mos-linux.tar.xz -C /opt/llvm-mos --strip-components=1 && \
    rm llvm-mos-linux.tar.xz; \
    fi

# Install z88dk from snap (Z80) - using alternative method in Docker
# Note: Snap doesn't work in Docker, using SDCC for Z80 instead

# Set up working directory
WORKDIR /work

# Default command
CMD ["make", "help"]

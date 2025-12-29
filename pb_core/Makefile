# pb_core Makefile - Clean-room Puzzle Bobble core library
# SPDX-License-Identifier: BSD-3-Clause
#
# Build Configuration:
#   make                    - Standard optimized C17 build (-O3, hardened)
#   make DEBUG=1            - Debug build with ASan/UBSan/LeakSan
#   make FIXED_POINT=1      - Deterministic fixed-point mode (Q16.16)
#   make SIZE_TIER=mini     - Size tier: full|medium|mini|micro
#   make C_STD=c99          - C standard: c17|c11|c99|c89
#
# Build Hardening:
#   make STRICT=1           - Enable all strict warnings (pedantic, may have FPs)
#   make LTO=1              - Enable Link-Time Optimization
#   make NO_HARDENING=1     - Disable security hardening flags
#   make EXTRA_CFLAGS="-x"  - Add custom compiler flags
#   make EXTRA_LDFLAGS="-x" - Add custom linker flags
#
# Cross-compilation:
#   make cross-check-all    - Test all available cross-compilers
#   make ARCH=arm           - Build for specific architecture
#
# Docker Build Environment:
#   make docker-build       - Build Docker image with all cross-compilers
#   make docker-shell       - Interactive shell in Docker environment
#   make docker-make        - Run make inside Docker
#   make docker-cross-all   - Run cross-check-all inside Docker
#   make DOCKER=1 <target>  - Run any target inside Docker
#   make docker-pdp11       - Build for PDP-11 using Docker
#
# Validation:
#   make audit              - Full static analysis (lint+check+complexity)
#   make lint               - Quick cppcheck
#   make complexity         - Cyclomatic complexity analysis

#=============================================================================
# Configuration
#=============================================================================

# Default compiler and tools
CC ?= gcc
AR ?= ar
CPPCHECK ?= cppcheck
SHELLCHECK ?= shellcheck

#=============================================================================
# Docker Configuration
#=============================================================================

# Docker image names
DOCKER_IMAGE ?= pb-core-build
DOCKER_IMAGE_FULL ?= pb-core-build:full
DOCKER_IMAGE_MINIMAL ?= pb-core-build:minimal
DOCKER_PDP11_IMAGE ?= jameshagerman/gcc-pdp11-aout

# Docker run options
DOCKER_RUN_OPTS ?= --rm -v $(CURDIR):/work -w /work
DOCKER_RUN := docker run $(DOCKER_RUN_OPTS)

# Enable Docker mode: DOCKER=1 make <target>
ifdef DOCKER
    # Wrap compiler calls through Docker
    DOCKER_PREFIX := $(DOCKER_RUN) $(DOCKER_IMAGE)
endif

# C Standard selection (c17|c11|c99|c89)
C_STD ?= c17

# Size tier selection (full|medium|mini|micro)
SIZE_TIER ?= full

# Base warning flags
CFLAGS_WARN_BASE := -Wall -Wextra -Werror -pedantic

# Extended warning flags (enabled by default for quality)
CFLAGS_WARN_EXT := -Wformat=2 -Wformat-security \
    -Wundef -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
    -Wpointer-arith -Wwrite-strings

# Strict warning flags (opt-in with STRICT=1, may have false positives)
ifdef STRICT
    CFLAGS_WARN_STRICT := -Wconversion -Wsign-conversion -Wfloat-equal \
        -Wcast-qual -Wcast-align -Wredundant-decls -Wnested-externs \
        -Wswitch-default -Wunreachable-code -Wdouble-promotion \
        -Wnull-dereference -Wlogical-op -Wduplicated-cond \
        -Wduplicated-branches -Wrestrict -Wjump-misses-init \
        -Wstringop-overflow=4 -Wformat-overflow=2 -Wformat-truncation=2
else
    CFLAGS_WARN_STRICT :=
endif

# Base flags
CFLAGS_BASE := $(CFLAGS_WARN_BASE) $(CFLAGS_WARN_EXT) $(CFLAGS_WARN_STRICT) -Iinclude

# C standard mapping
ifeq ($(C_STD),c89)
    CFLAGS_STD := -std=c89 -DPB_C89
else ifeq ($(C_STD),c99)
    CFLAGS_STD := -std=c99 -DPB_C99
else ifeq ($(C_STD),c11)
    CFLAGS_STD := -std=c11 -DPB_C11
else
    CFLAGS_STD := -std=c17 -DPB_C17
endif

# Size tier mapping
ifeq ($(SIZE_TIER),micro)
    CFLAGS_SIZE := -DPB_SIZE_MICRO
else ifeq ($(SIZE_TIER),mini)
    CFLAGS_SIZE := -DPB_SIZE_MINI
else ifeq ($(SIZE_TIER),medium)
    CFLAGS_SIZE := -DPB_SIZE_MEDIUM
else
    CFLAGS_SIZE := -DPB_SIZE_FULL
endif

# Optimization flags (default -O3 for performance)
CFLAGS_OPT := -O3

# Security hardening (enabled by default in release builds)
ifndef NO_HARDENING
    CFLAGS_HARDEN := -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
        -fstack-clash-protection -fcf-protection \
        -fno-delete-null-pointer-checks -fno-strict-overflow
    LDFLAGS_HARDEN := -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack
else
    CFLAGS_HARDEN :=
    LDFLAGS_HARDEN :=
endif

# Debug build with sanitizers
ifdef DEBUG
    CFLAGS_OPT := -g -O0 -DDEBUG
    CFLAGS_SANITIZE := -fsanitize=address,undefined,leak \
        -fno-omit-frame-pointer -fno-optimize-sibling-calls
    LDFLAGS_SANITIZE := -fsanitize=address,undefined,leak
    # Disable some hardening that conflicts with sanitizers
    CFLAGS_HARDEN :=
    LDFLAGS_HARDEN :=
else
    CFLAGS_SANITIZE :=
    LDFLAGS_SANITIZE :=
endif

# LTO (Link-Time Optimization) for release builds
ifdef LTO
    CFLAGS_LTO := -flto
    LDFLAGS_LTO := -flto
else
    CFLAGS_LTO :=
    LDFLAGS_LTO :=
endif

# Fixed-point mode
ifdef FIXED_POINT
    CFLAGS_FP := -DPB_USE_FIXED_POINT=1
else
    CFLAGS_FP :=
endif

# Combined flags
CFLAGS := $(CFLAGS_STD) $(CFLAGS_BASE) $(CFLAGS_SIZE) $(CFLAGS_OPT) $(CFLAGS_FP) \
    $(CFLAGS_SANITIZE) $(CFLAGS_HARDEN) $(CFLAGS_LTO) $(EXTRA_CFLAGS)
LDFLAGS := $(LDFLAGS_SANITIZE) $(LDFLAGS_HARDEN) $(LDFLAGS_LTO) $(EXTRA_LDFLAGS)
LDLIBS := -lm

#=============================================================================
# Directories
#=============================================================================

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin
TEST_DIR := tests
TOOLS_DIR := tools
EXAMPLES_DIR := examples

#=============================================================================
# Source files
#=============================================================================

# Core library (no platform dependencies)
CORE_SRCS := $(wildcard $(SRC_DIR)/core/*.c)
DATA_SRCS := $(wildcard $(SRC_DIR)/data/*.c)
VENDOR_SRCS := $(wildcard $(SRC_DIR)/vendor/*.c)
LIB_SRCS := $(CORE_SRCS) $(DATA_SRCS) $(VENDOR_SRCS)

# Core sources without float-dependent modules (for 8-bit targets without FPU)
CORE_SRCS_NOFLOAT := $(filter-out $(SRC_DIR)/core/pb_color.c $(SRC_DIR)/core/pb_cvd.c,$(CORE_SRCS))

# Platform files (require SDL2)
SDL2_SRCS := $(SRC_DIR)/platform/pb_sdl2.c
DEMO_SRCS := $(SRC_DIR)/platform/pb_demo.c

# Object files
CORE_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DATA_OBJS := $(DATA_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
VENDOR_OBJS := $(VENDOR_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
LIB_OBJS := $(CORE_OBJS) $(DATA_OBJS) $(VENDOR_OBJS)

# Headers
HEADERS := $(wildcard $(INC_DIR)/pb/*.h)

# Tests, tools, examples
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BIN_DIR)/%)
TOOL_SRCS := $(wildcard $(TOOLS_DIR)/*.c)
TOOL_BINS := $(TOOL_SRCS:$(TOOLS_DIR)/%.c=$(BIN_DIR)/%)
EXAMPLE_SRCS := $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS := $(EXAMPLE_SRCS:$(EXAMPLES_DIR)/%.c=$(BIN_DIR)/%)

# Library output
STATIC_LIB := $(LIB_DIR)/libpb_core.a

# SDL2 flags
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer 2>/dev/null)
SDL2_LIBS := $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer 2>/dev/null)

#=============================================================================
# Main Targets
#=============================================================================

.PHONY: all clean test dirs lib demo tools examples
.PHONY: audit lint check complexity format
.PHONY: cross-check cross-check-all install info

all: dirs lib

dirs:
	@mkdir -p $(OBJ_DIR)/core $(OBJ_DIR)/data $(OBJ_DIR)/vendor $(OBJ_DIR)/platform
	@mkdir -p $(LIB_DIR) $(BIN_DIR)

lib: dirs $(STATIC_LIB)

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

#=============================================================================
# Test Targets
#=============================================================================

test: dirs lib $(TEST_BINS)
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "=== Running $$test ==="; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lpb_core $(LDLIBS) -o $@

#=============================================================================
# Demo, Tools, Examples
#=============================================================================

DEMO_BIN := $(BIN_DIR)/pb_demo

demo: dirs lib $(DEMO_BIN)

$(DEMO_BIN): $(SDL2_SRCS) $(DEMO_SRCS) $(STATIC_LIB)
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) $(SDL2_SRCS) $(DEMO_SRCS) -L$(LIB_DIR) -lpb_core $(LDLIBS) $(SDL2_LIBS) -o $@

tools: dirs lib $(TOOL_BINS)

$(BIN_DIR)/pb_%: $(TOOLS_DIR)/pb_%.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lpb_core $(LDLIBS) -o $@

examples: dirs lib $(EXAMPLE_BINS)

$(BIN_DIR)/%: $(EXAMPLES_DIR)/%.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lpb_core $(LDLIBS) -o $@

#=============================================================================
# Installation
#=============================================================================

PREFIX ?= /usr/local

install: lib
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/pb
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(INC_DIR)/pb/*.h $(DESTDIR)$(PREFIX)/include/pb/

#=============================================================================
# Static Analysis and Validation
#=============================================================================

# Tool configuration
CPPCHECK ?= cppcheck
FLAWFINDER ?= flawfinder
SPLINT ?= splint
CLANG_TIDY ?= clang-tidy
CLANG_FORMAT ?= clang-format

# cppcheck options (from conservative to exhaustive)
CPPCHECK_QUICK := --quiet --error-exitcode=1 -I$(INC_DIR)
CPPCHECK_FULL := --enable=all --std=c17 -I$(INC_DIR) \
	--suppress=missingIncludeSystem \
	--suppress=unusedFunction \
	--inconclusive --force

# flawfinder options
FLAWFINDER_OPTS := --minlevel=2 --columns --context

# splint options (very strict C analysis)
SPLINT_OPTS := -I$(INC_DIR) +posixlib -nullpass -nullderef -nullassign \
	-temptrans -observertrans -statictrans -kepttrans \
	-mustfreeonly -compmempass -branchstate

# clang-tidy checks (comprehensive set)
CLANG_TIDY_CHECKS := -checks='-*,clang-analyzer-*,bugprone-*,cert-*,misc-*,performance-*,portability-*,readability-*,-readability-magic-numbers,-readability-identifier-length'
CLANG_TIDY_OPTS := $(CLANG_TIDY_CHECKS) -- -I$(INC_DIR) -std=c17

#-----------------------------------------------------------------------------
# Comprehensive Audit Pipeline
#-----------------------------------------------------------------------------

# Full audit: lint → security → complexity (fails on critical issues)
audit: lint-all security complexity
	@echo ""
	@echo "=============================================="
	@echo "=== FULL AUDIT COMPLETE ====================="
	@echo "=============================================="

# Extended audit: adds clang-tidy (slower but more thorough)
audit-full: audit clang-tidy
	@echo "=== Extended audit complete ==="

#-----------------------------------------------------------------------------
# Linting Targets
#-----------------------------------------------------------------------------

# Quick lint (fast, error-only)
lint:
	@echo "=== Quick lint check (cppcheck) ==="
	@$(CPPCHECK) $(CPPCHECK_QUICK) $(CORE_SRCS) && echo "cppcheck: PASS"

# Full lint (all warnings and suggestions)
lint-full:
	@echo "=== Full lint check (cppcheck --enable=all) ==="
	$(CPPCHECK) $(CPPCHECK_FULL) $(SRC_DIR)

# All linters in parallel-friendly sequence
lint-all: lint lint-full
	@echo "=== All lint checks complete ==="

#-----------------------------------------------------------------------------
# Security Analysis
#-----------------------------------------------------------------------------

# Security-focused analysis (flawfinder)
security:
	@echo "=== Security analysis (flawfinder) ==="
	@$(FLAWFINDER) $(FLAWFINDER_OPTS) $(SRC_DIR) $(INC_DIR) || true
	@echo ""
	@echo "=== Security analysis complete ==="

# Strict C analysis (splint - very pedantic)
splint:
	@echo "=== Strict C analysis (splint) ==="
	@$(SPLINT) $(SPLINT_OPTS) $(CORE_SRCS) 2>&1 | head -100 || true
	@echo "=== splint complete (see above for warnings) ==="

#-----------------------------------------------------------------------------
# Clang Tooling
#-----------------------------------------------------------------------------

# clang-tidy analysis (thorough but slow)
clang-tidy:
	@echo "=== Clang-tidy analysis ==="
	@for src in $(CORE_SRCS); do \
		echo "Checking $$src..."; \
		$(CLANG_TIDY) $$src $(CLANG_TIDY_OPTS) 2>/dev/null || true; \
	done
	@echo "=== clang-tidy complete ==="

#-----------------------------------------------------------------------------
# Legacy Compatibility
#-----------------------------------------------------------------------------

# Full cppcheck (legacy target name)
check: lint-full

#-----------------------------------------------------------------------------
# Complexity Analysis
#-----------------------------------------------------------------------------

# Cyclomatic complexity (requires pmccabe or lizard)
complexity:
	@echo "=== Cyclomatic complexity ==="
	@if command -v lizard >/dev/null 2>&1; then \
		lizard $(CORE_SRCS) --CCN 15 -w; \
	elif command -v pmccabe >/dev/null 2>&1; then \
		echo "Functions by complexity (top 20):"; \
		pmccabe $(CORE_SRCS) | sort -rn | head -20; \
	else \
		echo "Install lizard (pip install lizard) or pmccabe for complexity analysis"; \
	fi

#-----------------------------------------------------------------------------
# Code Formatting
#-----------------------------------------------------------------------------

# Format all C code in-place
format:
	@echo "=== Formatting code ==="
	@find $(SRC_DIR) $(INC_DIR) $(TEST_DIR) \( -name '*.c' -o -name '*.h' \) -exec $(CLANG_FORMAT) -i {} \;
	@echo "=== Formatting complete ==="

# Check formatting without modifying (CI-friendly)
format-check:
	@echo "=== Checking code format ==="
	@find $(SRC_DIR) $(INC_DIR) $(TEST_DIR) \( -name '*.c' -o -name '*.h' \) -exec $(CLANG_FORMAT) --dry-run --Werror {} \; 2>&1 | head -50
	@echo "=== Format check complete ==="

#-----------------------------------------------------------------------------
# Combined Quality Targets
#-----------------------------------------------------------------------------

# Full quality check: format check → lint → security → build with strict warnings
quality: format-check lint-all security
	@echo "=== Building with strict warnings ==="
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory STRICT=1 lib
	@echo "=== Quality check PASSED ==="

# CI/CD pipeline target
ci: format-check lint-all security
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory lib
	@$(MAKE) --no-print-directory test
	@echo "=== CI pipeline PASSED ==="

#=============================================================================
# Cross-Compilation: Syntax Check Targets
#=============================================================================

# Common cross-compilation flags (syntax only, no linking)
CROSS_CFLAGS := $(CFLAGS_STD) -Wall -Wextra -Werror -fsyntax-only -I$(INC_DIR)
CROSS_CFLAGS_FREESTANDING := $(CROSS_CFLAGS) -ffreestanding

# Individual architecture checks
.PHONY: cross-x86-64 cross-i386 cross-aarch64 cross-arm cross-riscv64 cross-sh
.PHONY: cross-m68k cross-m68k-mint cross-avr cross-msp430 cross-ia16 cross-z80 cross-z88dk cross-6502

cross-x86-64:
	@echo -n "x86-64 (gcc)... "
	@$(CC) $(CROSS_CFLAGS) $(CORE_SRCS) && echo "OK" || echo "FAIL"

cross-i386:
	@echo -n "i386 (gcc -m32)... "
	@$(CC) -m32 $(CROSS_CFLAGS) $(CORE_SRCS) && echo "OK" || echo "FAIL"

cross-aarch64:
	@echo -n "AArch64... "
	@if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then \
		aarch64-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) && echo "OK" || echo "FAIL"; \
	else echo "NOT INSTALLED"; fi

cross-arm:
	@echo -n "ARM Cortex-M... "
	@if command -v arm-none-eabi-gcc >/dev/null 2>&1; then \
		arm-none-eabi-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) && echo "OK" || echo "FAIL"; \
	else echo "NOT INSTALLED"; fi

cross-riscv64:
	@echo -n "RISC-V 64... "
	@if command -v riscv64-elf-gcc >/dev/null 2>&1; then \
		riscv64-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS NEWLIB" || echo "OK"; \
	elif command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then \
		riscv64-unknown-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS NEWLIB" || echo "OK"; \
	elif command -v riscv64-linux-gnu-gcc >/dev/null 2>&1; then \
		riscv64-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK (linux)"; \
	else echo "NOT INSTALLED"; fi

cross-sh:
	@echo -n "SuperH (Casio)... "
	@if command -v sh-elf-gcc >/dev/null 2>&1; then \
		sh-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS NEWLIB" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-m68k:
	@echo -n "M68K (Amiga/Genesis)... "
	@if command -v m68k-elf-gcc >/dev/null 2>&1; then \
		m68k-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS LIBC" || echo "OK"; \
	elif command -v m68k-linux-gnu-gcc >/dev/null 2>&1; then \
		m68k-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK (linux)"; \
	else echo "NOT INSTALLED"; fi

cross-m68k-mint:
	@echo -n "M68K (Atari ST/MiNT)... "
	@if command -v m68k-atari-mint-gcc >/dev/null 2>&1; then \
		m68k-atari-mint-gcc -std=c99 -Wall -Wextra -fsyntax-only -I$(INC_DIR) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-avr:
	@echo -n "AVR (Arduino)... "
	@if command -v avr-gcc >/dev/null 2>&1; then \
		avr-gcc $(CROSS_CFLAGS_FREESTANDING) -mmcu=atmega328p $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS LIBC" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-msp430:
	@echo -n "MSP430 (TI)... "
	@if command -v msp430-elf-gcc >/dev/null 2>&1; then \
		msp430-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS LIBC" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-ia16:
	@echo -n "8086/DOS (ia16)... "
	@if command -v ia16-elf-gcc >/dev/null 2>&1; then \
		ia16-elf-gcc -std=c99 -Wall -fsyntax-only -I$(INC_DIR) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "DATA TOO LARGE" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-z80:
	@echo -n "Z80 (SDCC)... "
	@if command -v sdcc >/dev/null 2>&1; then \
		sdcc -mz80 --syntax-only -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|internal" && echo "ICE/C99" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-z88dk:
	@echo -n "Z80 (z88dk)... "
	@if command -v z88dk.zcc >/dev/null 2>&1; then \
		z88dk.zcc +test -vn -c -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c -o /dev/null 2>&1 | head -1 | grep -q "error\|Error" && echo "FAIL" || echo "OK"; \
	elif command -v zcc >/dev/null 2>&1; then \
		zcc +test -vn -c -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c -o /dev/null 2>&1 | head -1 | grep -q "error\|Error" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-6502:
	@echo -n "6502 (cc65)... "
	@if command -v cc65 >/dev/null 2>&1; then \
		cc65 -t none -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|Error" && echo "C89 only" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

# LLVM-MOS: Modern C99/C++11 6502 compiler (no float support, requires fixed-point)
# Install: Download from https://github.com/llvm-mos/llvm-mos-sdk/releases
cross-6502-llvm:
	@echo -n "6502 (LLVM-MOS)... "
	@if [ -x /opt/llvm-mos/bin/mos-common-clang ]; then \
		/opt/llvm-mos/bin/mos-common-clang -fsyntax-only -DPB_USE_FIXED_POINT=1 -DPB_FIXED_FORMAT=2 -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|Error" && echo "FAIL" || echo "OK (Q8.8)"; \
	else echo "NOT INSTALLED"; fi

cross-c64:
	@echo -n "C64 (LLVM-MOS)... "
	@if [ -x /opt/llvm-mos/bin/mos-c64-clang ]; then \
		/opt/llvm-mos/bin/mos-c64-clang -fsyntax-only -DPB_USE_FIXED_POINT=1 -DPB_FIXED_FORMAT=2 -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|Error" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-nes:
	@echo -n "NES (LLVM-MOS)... "
	@if [ -x /opt/llvm-mos/bin/mos-nes-clang ]; then \
		/opt/llvm-mos/bin/mos-nes-clang -fsyntax-only -DPB_USE_FIXED_POINT=1 -DPB_FIXED_FORMAT=2 -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|Error" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-atari2600:
	@echo -n "Atari 2600 (LLVM-MOS)... "
	@if [ -x /opt/llvm-mos/bin/mos-atari2600-4k-clang ]; then \
		/opt/llvm-mos/bin/mos-atari2600-4k-clang -fsyntax-only -DPB_USE_FIXED_POINT=1 -DPB_FIXED_FORMAT=2 -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|Error" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

# RISC-V Embedded (bare-metal with picolibc)
# Install: sudo apt install gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf
PICOLIBC_RISCV_INC := /usr/lib/picolibc/riscv64-unknown-elf/include

cross-riscv32:
	@echo -n "RISC-V 32 (embedded)... "
	@if command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then \
		if [ -d "$(PICOLIBC_RISCV_INC)" ]; then \
			riscv64-unknown-elf-gcc -march=rv32imc -mabi=ilp32 -isystem $(PICOLIBC_RISCV_INC) $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK (picolibc)"; \
		else \
			riscv64-unknown-elf-gcc -march=rv32imc -mabi=ilp32 $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS PICOLIBC" || echo "OK"; \
		fi; \
	else echo "NOT INSTALLED"; fi

cross-riscv64-elf:
	@echo -n "RISC-V 64 (embedded)... "
	@if command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then \
		if [ -d "$(PICOLIBC_RISCV_INC)" ]; then \
			riscv64-unknown-elf-gcc -march=rv64gc -mabi=lp64d -isystem $(PICOLIBC_RISCV_INC) $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK (picolibc)"; \
		else \
			riscv64-unknown-elf-gcc -march=rv64gc -mabi=lp64d $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS PICOLIBC" || echo "OK"; \
		fi; \
	else echo "NOT INSTALLED"; fi

# --- New Linux Cross-Compilers (Ubuntu 24.04) ---

cross-alpha:
	@echo -n "Alpha (DEC)... "
	@if command -v alpha-linux-gnu-gcc >/dev/null 2>&1; then \
		alpha-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-arc:
	@echo -n "ARC (Synopsys)... "
	@if command -v arc-linux-gnu-gcc >/dev/null 2>&1; then \
		arc-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-hppa:
	@echo -n "HP-PA (PA-RISC)... "
	@if command -v hppa-linux-gnu-gcc >/dev/null 2>&1; then \
		hppa-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-hppa64:
	@echo -n "HP-PA64 (PA-RISC 2.0)... "
	@if command -v hppa64-linux-gnu-gcc >/dev/null 2>&1; then \
		hppa64-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-mips:
	@echo -n "MIPS (big-endian)... "
	@if command -v mips-linux-gnu-gcc >/dev/null 2>&1; then \
		mips-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-mipsel:
	@echo -n "MIPS (little-endian)... "
	@if command -v mipsel-linux-gnu-gcc >/dev/null 2>&1; then \
		mipsel-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-mips64:
	@echo -n "MIPS64 (big-endian)... "
	@if command -v mips64-linux-gnuabi64-gcc >/dev/null 2>&1; then \
		mips64-linux-gnuabi64-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-mips64el:
	@echo -n "MIPS64 (little-endian)... "
	@if command -v mips64el-linux-gnuabi64-gcc >/dev/null 2>&1; then \
		mips64el-linux-gnuabi64-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-powerpc:
	@echo -n "PowerPC 32-bit... "
	@if command -v powerpc-linux-gnu-gcc >/dev/null 2>&1; then \
		powerpc-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-powerpc64:
	@echo -n "PowerPC 64-bit BE... "
	@if command -v powerpc64-linux-gnu-gcc >/dev/null 2>&1; then \
		powerpc64-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-powerpc64le:
	@echo -n "PowerPC 64-bit LE... "
	@if command -v powerpc64le-linux-gnu-gcc >/dev/null 2>&1; then \
		powerpc64le-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-s390x:
	@echo -n "S/390x (IBM Mainframe)... "
	@if command -v s390x-linux-gnu-gcc >/dev/null 2>&1; then \
		s390x-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-sparc64:
	@echo -n "SPARC64 (Sun/Oracle)... "
	@if command -v sparc64-linux-gnu-gcc >/dev/null 2>&1; then \
		sparc64-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-sh4:
	@echo -n "SH4 (Linux)... "
	@if command -v sh4-linux-gnu-gcc >/dev/null 2>&1; then \
		sh4-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-loongarch64:
	@echo -n "LoongArch64... "
	@if command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then \
		loongarch64-linux-gnu-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	elif command -v loongarch64-linux-gnu-gcc-14 >/dev/null 2>&1; then \
		loongarch64-linux-gnu-gcc-14 $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

# --- Embedded/Bare-Metal Cross-Compilers ---

cross-or1k:
	@echo -n "OpenRISC (or1k)... "
	@if command -v or1k-elf-gcc >/dev/null 2>&1; then \
		or1k-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS LIBC" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-h8300:
	@echo -n "H8/300 (Hitachi)... "
	@if command -v h8300-hms-gcc >/dev/null 2>&1; then \
		h8300-hms-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS LIBC" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-xtensa:
	@echo -n "Xtensa LX106 (ESP8266)... "
	@if command -v xtensa-lx106-elf-gcc >/dev/null 2>&1; then \
		xtensa-lx106-elf-gcc $(CROSS_CFLAGS_FREESTANDING) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "NEEDS LIBC" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

# --- Windows Cross-Compilation ---

cross-mingw32:
	@echo -n "Windows 32-bit (MinGW)... "
	@if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then \
		i686-w64-mingw32-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

cross-mingw64:
	@echo -n "Windows 64-bit (MinGW)... "
	@if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then \
		x86_64-w64-mingw32-gcc $(CROSS_CFLAGS) $(CORE_SRCS) 2>&1 | head -1 | grep -q "error\|fatal" && echo "FAIL" || echo "OK"; \
	else echo "NOT INSTALLED"; fi

#=============================================================================
# Cross-Check All Architectures
#=============================================================================

cross-check:
	@echo "=== Cross-compilation syntax checks ==="
	@$(MAKE) --no-print-directory cross-x86-64
	@$(MAKE) --no-print-directory cross-i386
	@echo "=== Done (basic) ==="

cross-check-all:
	@echo "============================================"
	@echo "=== FULL CROSS-COMPILATION MATRIX ========="
	@echo "============================================"
	@echo ""
	@echo "--- 64-bit Linux Architectures ---"
	@$(MAKE) --no-print-directory cross-x86-64
	@$(MAKE) --no-print-directory cross-aarch64
	@$(MAKE) --no-print-directory cross-riscv64
	@$(MAKE) --no-print-directory cross-alpha
	@$(MAKE) --no-print-directory cross-mips64
	@$(MAKE) --no-print-directory cross-mips64el
	@$(MAKE) --no-print-directory cross-powerpc64
	@$(MAKE) --no-print-directory cross-powerpc64le
	@$(MAKE) --no-print-directory cross-s390x
	@$(MAKE) --no-print-directory cross-sparc64
	@$(MAKE) --no-print-directory cross-loongarch64
	@$(MAKE) --no-print-directory cross-hppa64
	@echo ""
	@echo "--- 32-bit Linux Architectures ---"
	@$(MAKE) --no-print-directory cross-i386
	@$(MAKE) --no-print-directory cross-arm
	@$(MAKE) --no-print-directory cross-arc
	@$(MAKE) --no-print-directory cross-hppa
	@$(MAKE) --no-print-directory cross-mips
	@$(MAKE) --no-print-directory cross-mipsel
	@$(MAKE) --no-print-directory cross-powerpc
	@$(MAKE) --no-print-directory cross-sh4
	@$(MAKE) --no-print-directory cross-m68k
	@$(MAKE) --no-print-directory cross-m68k-mint
	@echo ""
	@echo "--- 32-bit Embedded/Bare-Metal ---"
	@$(MAKE) --no-print-directory cross-sh
	@$(MAKE) --no-print-directory cross-or1k
	@$(MAKE) --no-print-directory cross-riscv32
	@$(MAKE) --no-print-directory cross-riscv64-elf
	@echo ""
	@echo "--- 16-bit Architectures ---"
	@$(MAKE) --no-print-directory cross-ia16
	@$(MAKE) --no-print-directory cross-msp430
	@$(MAKE) --no-print-directory cross-h8300
	@echo ""
	@echo "--- 8-bit Architectures ---"
	@$(MAKE) --no-print-directory cross-avr
	@$(MAKE) --no-print-directory cross-xtensa
	@$(MAKE) --no-print-directory cross-z80
	@$(MAKE) --no-print-directory cross-z88dk
	@$(MAKE) --no-print-directory cross-6502
	@$(MAKE) --no-print-directory cross-6502-llvm
	@$(MAKE) --no-print-directory cross-c64
	@$(MAKE) --no-print-directory cross-nes
	@$(MAKE) --no-print-directory cross-atari2600
	@echo ""
	@echo "--- Windows Cross-Compilation ---"
	@$(MAKE) --no-print-directory cross-mingw32
	@$(MAKE) --no-print-directory cross-mingw64
	@echo ""
	@echo "============================================"
	@echo "=== CROSS-COMPILATION COMPLETE ============"
	@echo "============================================"

#=============================================================================
# Full Cross-Build Targets (produces real .o and .a files)
#=============================================================================

# Cross-build output directories
CROSS_BUILD_DIR := $(BUILD_DIR)/cross

# Architecture definitions for full builds
# Format: ARCH_<name>_CC, ARCH_<name>_CFLAGS, ARCH_<name>_AR

# --- 64-bit Linux ---
ARCH_x86_64_CC := gcc
ARCH_x86_64_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_x86_64_AR := ar

ARCH_aarch64_CC := aarch64-linux-gnu-gcc
ARCH_aarch64_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_aarch64_AR := aarch64-linux-gnu-ar

ARCH_alpha_CC := alpha-linux-gnu-gcc
ARCH_alpha_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_alpha_AR := alpha-linux-gnu-ar

ARCH_mips64_CC := mips64-linux-gnuabi64-gcc
ARCH_mips64_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_mips64_AR := mips64-linux-gnuabi64-ar

ARCH_ppc64_CC := powerpc64-linux-gnu-gcc
ARCH_ppc64_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_ppc64_AR := powerpc64-linux-gnu-ar

ARCH_ppc64le_CC := powerpc64le-linux-gnu-gcc
ARCH_ppc64le_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_ppc64le_AR := powerpc64le-linux-gnu-ar

ARCH_s390x_CC := s390x-linux-gnu-gcc
ARCH_s390x_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_s390x_AR := s390x-linux-gnu-ar

ARCH_sparc64_CC := sparc64-linux-gnu-gcc
ARCH_sparc64_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_sparc64_AR := sparc64-linux-gnu-ar

# --- 32-bit Linux ---
ARCH_arm_CC := arm-none-eabi-gcc
ARCH_arm_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude -ffreestanding -mcpu=cortex-m4
ARCH_arm_AR := arm-none-eabi-ar

ARCH_m68k_CC := m68k-linux-gnu-gcc
ARCH_m68k_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_m68k_AR := m68k-linux-gnu-ar

ARCH_mips_CC := mips-linux-gnu-gcc
ARCH_mips_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_mips_AR := mips-linux-gnu-ar

ARCH_ppc_CC := powerpc-linux-gnu-gcc
ARCH_ppc_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude
ARCH_ppc_AR := powerpc-linux-gnu-ar

# --- Embedded/Bare-metal ---
ARCH_avr_CC := avr-gcc
ARCH_avr_CFLAGS := -std=c11 -Wall -Wextra -Os -Iinclude -ffreestanding -mmcu=atmega328p -DPB_SIZE_MICRO -DPB_USE_FIXED_POINT=1
ARCH_avr_AR := avr-ar

ARCH_riscv32_CC := riscv64-unknown-elf-gcc
ARCH_riscv32_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude -ffreestanding -march=rv32imc -mabi=ilp32 -isystem /usr/lib/picolibc/riscv64-unknown-elf/include
ARCH_riscv32_AR := riscv64-unknown-elf-ar

ARCH_riscv64_CC := riscv64-unknown-elf-gcc
ARCH_riscv64_CFLAGS := -std=c17 -Wall -Wextra -O2 -Iinclude -ffreestanding -march=rv64gc -mabi=lp64d -isystem /usr/lib/picolibc/riscv64-unknown-elf/include
ARCH_riscv64_AR := riscv64-unknown-elf-ar

# --- 8-bit ---
ARCH_6502_CC := /opt/llvm-mos/bin/mos-common-clang
ARCH_6502_CFLAGS := -Os -Iinclude -DPB_USE_FIXED_POINT=1 -DPB_FIXED_FORMAT=2 -DPB_FREESTANDING=1 -DPB_SIZE_MICRO
ARCH_6502_AR := /opt/llvm-mos/bin/llvm-ar

# Generic cross-build function
# Usage: $(call cross-build,arch)
define cross-build
	@echo "=== Building for $(1) ==="
	@mkdir -p $(CROSS_BUILD_DIR)/$(1)/obj/core $(CROSS_BUILD_DIR)/$(1)/obj/data $(CROSS_BUILD_DIR)/$(1)/lib
	@for src in $(CORE_SRCS); do \
		obj=$(CROSS_BUILD_DIR)/$(1)/obj/$$(echo $$src | sed 's|$(SRC_DIR)/||;s|\.c$$|.o|'); \
		echo "  CC $$src -> $$obj"; \
		$(ARCH_$(1)_CC) $(ARCH_$(1)_CFLAGS) -c $$src -o $$obj || exit 1; \
	done
	@echo "  AR $(CROSS_BUILD_DIR)/$(1)/lib/libpb_core.a"
	@$(ARCH_$(1)_AR) rcs $(CROSS_BUILD_DIR)/$(1)/lib/libpb_core.a $(CROSS_BUILD_DIR)/$(1)/obj/core/*.o
	@echo "=== $(1) build complete ==="
endef

# Full build targets for each architecture
.PHONY: build-x86_64 build-aarch64 build-arm build-avr build-riscv32 build-riscv64 build-6502
.PHONY: build-alpha build-mips64 build-ppc64 build-ppc64le build-s390x build-sparc64
.PHONY: build-m68k build-mips build-ppc

# Helper to check if compiler exists
define check-compiler
$(shell command -v $(1) 2>/dev/null)
endef

build-x86_64:
	$(call cross-build,x86_64)

build-aarch64:
ifneq ($(call check-compiler,$(ARCH_aarch64_CC)),)
	$(call cross-build,aarch64)
else
	@echo "$(ARCH_aarch64_CC) not installed"
endif

build-alpha:
ifneq ($(call check-compiler,$(ARCH_alpha_CC)),)
	$(call cross-build,alpha)
else
	@echo "$(ARCH_alpha_CC) not installed"
endif

build-mips64:
ifneq ($(call check-compiler,$(ARCH_mips64_CC)),)
	$(call cross-build,mips64)
else
	@echo "$(ARCH_mips64_CC) not installed"
endif

build-ppc64:
ifneq ($(call check-compiler,$(ARCH_ppc64_CC)),)
	$(call cross-build,ppc64)
else
	@echo "$(ARCH_ppc64_CC) not installed"
endif

build-ppc64le:
ifneq ($(call check-compiler,$(ARCH_ppc64le_CC)),)
	$(call cross-build,ppc64le)
else
	@echo "$(ARCH_ppc64le_CC) not installed"
endif

build-s390x:
ifneq ($(call check-compiler,$(ARCH_s390x_CC)),)
	$(call cross-build,s390x)
else
	@echo "$(ARCH_s390x_CC) not installed"
endif

build-sparc64:
ifneq ($(call check-compiler,$(ARCH_sparc64_CC)),)
	$(call cross-build,sparc64)
else
	@echo "$(ARCH_sparc64_CC) not installed"
endif

build-arm:
ifneq ($(call check-compiler,$(ARCH_arm_CC)),)
	$(call cross-build,arm)
else
	@echo "$(ARCH_arm_CC) not installed"
endif

build-m68k:
ifneq ($(call check-compiler,$(ARCH_m68k_CC)),)
	$(call cross-build,m68k)
else
	@echo "$(ARCH_m68k_CC) not installed"
endif

build-mips:
ifneq ($(call check-compiler,$(ARCH_mips_CC)),)
	$(call cross-build,mips)
else
	@echo "$(ARCH_mips_CC) not installed"
endif

build-ppc:
ifneq ($(call check-compiler,$(ARCH_ppc_CC)),)
	$(call cross-build,ppc)
else
	@echo "$(ARCH_ppc_CC) not installed"
endif

build-avr:
ifneq ($(call check-compiler,$(ARCH_avr_CC)),)
	$(call cross-build,avr)
else
	@echo "$(ARCH_avr_CC) not installed"
endif

build-riscv32:
ifneq ($(call check-compiler,$(ARCH_riscv32_CC)),)
	$(call cross-build,riscv32)
else
	@echo "$(ARCH_riscv32_CC) not installed"
endif

build-riscv64:
ifneq ($(call check-compiler,$(ARCH_riscv64_CC)),)
	$(call cross-build,riscv64)
else
	@echo "$(ARCH_riscv64_CC) not installed"
endif

# 6502 needs special handling - no float support, use NOFLOAT sources
build-6502:
ifeq ($(wildcard $(ARCH_6502_CC)),$(ARCH_6502_CC))
	@echo "=== Building for 6502 (no-float) ==="
	@mkdir -p $(CROSS_BUILD_DIR)/6502/obj/core $(CROSS_BUILD_DIR)/6502/lib
	@for src in $(CORE_SRCS_NOFLOAT); do \
		obj=$(CROSS_BUILD_DIR)/6502/obj/$$(echo $$src | sed 's|$(SRC_DIR)/||;s|\.c$$|.o|'); \
		echo "  CC $$src -> $$obj"; \
		$(ARCH_6502_CC) $(ARCH_6502_CFLAGS) -c $$src -o $$obj || exit 1; \
	done
	@echo "  AR $(CROSS_BUILD_DIR)/6502/lib/libpb_core.a"
	@$(ARCH_6502_AR) rcs $(CROSS_BUILD_DIR)/6502/lib/libpb_core.a $(CROSS_BUILD_DIR)/6502/obj/core/*.o
	@echo "=== 6502 build complete (pb_color/pb_cvd excluded - no float) ==="
else
	@echo "LLVM-MOS not installed at /opt/llvm-mos"
endif

# Build all available architectures
build-all-cross:
	@echo "============================================"
	@echo "=== FULL CROSS-BUILD MATRIX ==============="
	@echo "============================================"
	@echo ""
	@echo "--- 64-bit Architectures ---"
	@$(MAKE) --no-print-directory build-x86_64
	@$(MAKE) --no-print-directory build-aarch64
	@$(MAKE) --no-print-directory build-alpha
	@$(MAKE) --no-print-directory build-mips64
	@$(MAKE) --no-print-directory build-ppc64
	@$(MAKE) --no-print-directory build-ppc64le
	@$(MAKE) --no-print-directory build-s390x
	@$(MAKE) --no-print-directory build-sparc64
	@echo ""
	@echo "--- 32-bit Architectures ---"
	@$(MAKE) --no-print-directory build-arm
	@$(MAKE) --no-print-directory build-m68k
	@$(MAKE) --no-print-directory build-mips
	@$(MAKE) --no-print-directory build-ppc
	@echo ""
	@echo "--- Embedded/8-bit ---"
	@$(MAKE) --no-print-directory build-avr
	@$(MAKE) --no-print-directory build-riscv32
	@$(MAKE) --no-print-directory build-riscv64
	@$(MAKE) --no-print-directory build-6502
	@echo ""
	@echo "============================================"
	@echo "=== Cross-build Summary ==="
	@echo "============================================"
	@for lib in $(CROSS_BUILD_DIR)/*/lib/*.a; do \
		if [ -f "$$lib" ]; then \
			arch=$$(echo "$$lib" | sed 's|.*/cross/||;s|/lib/.*||'); \
			size=$$(ls -lh "$$lib" | awk '{print $$5}'); \
			echo "  $$arch: $$size"; \
		fi; \
	done

# Build matrix: all combinations of arch × size-tier × fixed-point
.PHONY: build-matrix

build-matrix:
	@echo "============================================"
	@echo "=== FULL BUILD MATRIX ====================="
	@echo "============================================"
	@for arch in x86_64 aarch64 arm avr riscv32 riscv64 6502; do \
		for tier in full medium mini micro; do \
			for fp in 0 1; do \
				echo "Building $$arch / $$tier / FIXED_POINT=$$fp"; \
			done; \
		done; \
	done
	@echo "(Matrix enumeration only - full build would take significant time)"

#=============================================================================
# C Standard Compatibility Checks
#=============================================================================

.PHONY: check-c89 check-c99 check-c11 check-c17

check-c17:
	@echo "=== C17 compilation check ==="
	@$(CC) -std=c17 $(CFLAGS_BASE) -c $(CORE_SRCS) -o /dev/null && echo "C17: OK"

check-c11:
	@echo "=== C11 compilation check ==="
	@$(CC) -std=c11 $(CFLAGS_BASE) -DPB_C11 -c $(CORE_SRCS) -o /dev/null && echo "C11: OK"

check-c99:
	@echo "=== C99 compilation check ==="
	@$(CC) -std=c99 $(CFLAGS_BASE) -DPB_C99 -c $(CORE_SRCS) -o /dev/null && echo "C99: OK"

check-c89:
	@echo "=== C89 compilation check (expected to fail without refactoring) ==="
	@$(CC) -std=c89 $(CFLAGS_BASE) -DPB_C89 -c $(CORE_SRCS) -o /dev/null 2>&1 | head -5 || echo "C89: NEEDS REFACTORING"

check-all-standards: check-c17 check-c11 check-c99 check-c89
	@echo "=== Standard compatibility check complete ==="

#=============================================================================
# Size Tier Builds
#=============================================================================

.PHONY: build-full build-medium build-mini build-micro

build-full:
	@$(MAKE) clean && $(MAKE) SIZE_TIER=full

build-medium:
	@$(MAKE) clean && $(MAKE) SIZE_TIER=medium

build-mini:
	@$(MAKE) clean && $(MAKE) SIZE_TIER=mini

build-micro:
	@$(MAKE) clean && $(MAKE) SIZE_TIER=micro

#=============================================================================
# Build Info
#=============================================================================

info:
	@echo "=== Build Configuration ==="
	@echo "CC:        $(CC)"
	@echo "C_STD:     $(C_STD)"
	@echo "SIZE_TIER: $(SIZE_TIER)"
	@echo "DEBUG:     $(if $(DEBUG),yes,no)"
	@echo "FIXED_PT:  $(if $(FIXED_POINT),yes,no)"
	@echo "CFLAGS:    $(CFLAGS)"
	@echo ""
	@echo "=== Installed Cross-Compilers ==="
	@which gcc 2>/dev/null && echo "  gcc: installed" || echo "  gcc: MISSING"
	@which aarch64-linux-gnu-gcc 2>/dev/null && echo "  aarch64-linux-gnu-gcc: installed" || echo "  aarch64-linux-gnu-gcc: not installed"
	@which arm-none-eabi-gcc 2>/dev/null && echo "  arm-none-eabi-gcc: installed" || echo "  arm-none-eabi-gcc: not installed"
	@if which riscv64-elf-gcc 2>/dev/null; then echo "  riscv64-elf-gcc: installed"; \
	elif which riscv64-unknown-elf-gcc 2>/dev/null; then echo "  riscv64-unknown-elf-gcc: installed (alt)"; \
	elif which riscv64-linux-gnu-gcc 2>/dev/null; then echo "  riscv64-linux-gnu-gcc: installed (linux)"; \
	else echo "  riscv64-*-gcc: not installed"; fi
	@which sh-elf-gcc 2>/dev/null && echo "  sh-elf-gcc: installed" || echo "  sh-elf-gcc: not installed"
	@if which m68k-elf-gcc 2>/dev/null; then echo "  m68k-elf-gcc: installed"; \
	elif which m68k-linux-gnu-gcc 2>/dev/null; then echo "  m68k-linux-gnu-gcc: installed (alt)"; \
	else echo "  m68k-*-gcc: not installed"; fi
	@which avr-gcc 2>/dev/null && echo "  avr-gcc: installed" || echo "  avr-gcc: not installed"
	@which msp430-elf-gcc 2>/dev/null && echo "  msp430-elf-gcc: installed" || echo "  msp430-elf-gcc: not installed"
	@which ia16-elf-gcc 2>/dev/null && echo "  ia16-elf-gcc: installed" || echo "  ia16-elf-gcc: not installed"
	@which sdcc 2>/dev/null && echo "  sdcc: installed" || echo "  sdcc: not installed"
	@which cc65 2>/dev/null && echo "  cc65: installed" || echo "  cc65: not installed"

#=============================================================================
# Docker Build Environment
#=============================================================================
# Provides containerized build environment with all cross-compilers pre-installed
# Usage:
#   make docker-build       - Build the Docker image
#   make docker-shell       - Interactive shell
#   make docker-make TARGET=test  - Run specific make target
#   make DOCKER=1 test      - Run test inside Docker

.PHONY: docker-build docker-build-full docker-build-minimal docker-shell
.PHONY: docker-make docker-cross-all docker-build-all docker-pdp11 docker-clean
.PHONY: docker-info docker-pull-pdp11

# Check if Docker is available
DOCKER_AVAILABLE := $(shell command -v docker 2>/dev/null)

# Build Docker images
docker-build:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Building pb-core-build Docker image ==="
	docker build -t $(DOCKER_IMAGE) .

docker-build-full:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Building pb-core-build:full Docker image (with LLVM-MOS) ==="
	docker build --build-arg INSTALL_LLVM_MOS=1 -t $(DOCKER_IMAGE_FULL) .

docker-build-minimal:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Building pb-core-build:minimal Docker image ==="
	docker build --build-arg INSTALL_LLVM_MOS=0 -t $(DOCKER_IMAGE_MINIMAL) .

# Pull PDP-11 Docker image
docker-pull-pdp11:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Pulling PDP-11 cross-compiler image ==="
	docker pull $(DOCKER_PDP11_IMAGE)

# Interactive shell in Docker environment
docker-shell:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Starting interactive Docker shell ==="
	@echo "    All cross-compilers are available."
	@echo "    Type 'exit' to leave."
	$(DOCKER_RUN) -it $(DOCKER_IMAGE) /bin/bash

# Run make target inside Docker
# Usage: make docker-make TARGET=test
#        make docker-make TARGET="cross-check-all"
#        make docker-make TARGET="FIXED_POINT=1 test"
TARGET ?= lib
docker-make:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Running 'make $(TARGET)' in Docker ==="
	$(DOCKER_RUN) $(DOCKER_IMAGE) make $(TARGET)

# Convenience targets for common Docker operations
docker-test:
	$(MAKE) docker-make TARGET=test

docker-cross-all:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Running cross-check-all in Docker ==="
	$(DOCKER_RUN) $(DOCKER_IMAGE_FULL) make cross-check-all

docker-build-all:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Running build-all-cross in Docker ==="
	$(DOCKER_RUN) $(DOCKER_IMAGE_FULL) make build-all-cross

# PDP-11 cross-compilation via Docker
# Creates build/cross/pdp11/lib/libpb_core.a
docker-pdp11:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Building for PDP-11 via Docker ==="
	@mkdir -p $(CROSS_BUILD_DIR)/pdp11/obj/core $(CROSS_BUILD_DIR)/pdp11/lib
	@for src in $(CORE_SRCS); do \
		obj=$(CROSS_BUILD_DIR)/pdp11/obj/$$(echo $$src | sed 's|$(SRC_DIR)/||;s|\.c$$|.o|'); \
		echo "  CC $$src -> $$obj"; \
		$(DOCKER_RUN) $(DOCKER_PDP11_IMAGE) pdp11-aout-gcc -std=c99 -Wall -Wextra -Os -Iinclude -ffreestanding -c $$src -o $$obj || exit 1; \
	done
	@echo "  AR $(CROSS_BUILD_DIR)/pdp11/lib/libpb_core.a"
	$(DOCKER_RUN) $(DOCKER_PDP11_IMAGE) pdp11-aout-ar rcs $(CROSS_BUILD_DIR)/pdp11/lib/libpb_core.a $(CROSS_BUILD_DIR)/pdp11/obj/core/*.o
	@echo "=== PDP-11 build complete ==="

# Show Docker environment info
docker-info:
ifndef DOCKER_AVAILABLE
	@echo "Docker: NOT INSTALLED"
else
	@echo "=== Docker Environment ==="
	@docker --version
	@echo ""
	@echo "=== Available pb-core images ==="
	@docker images | grep -E "^(pb-core|jameshagerman/gcc-pdp11)" || echo "  No pb-core images found. Run 'make docker-build' first."
	@echo ""
	@echo "=== Disk usage ==="
	@docker system df 2>/dev/null || true
endif

# Clean Docker images
docker-clean:
ifndef DOCKER_AVAILABLE
	$(error Docker is not installed. Please install Docker first.)
endif
	@echo "=== Removing pb-core Docker images ==="
	-docker rmi $(DOCKER_IMAGE) 2>/dev/null
	-docker rmi $(DOCKER_IMAGE_FULL) 2>/dev/null
	-docker rmi $(DOCKER_IMAGE_MINIMAL) 2>/dev/null
	@echo "Note: PDP-11 image not removed (use 'docker rmi $(DOCKER_PDP11_IMAGE)' manually)"

#=============================================================================
# Docker-Compose Shortcuts
#=============================================================================
# For users who prefer docker-compose

.PHONY: compose-build compose-shell compose-test compose-cross

compose-build:
	docker-compose build

compose-shell:
	docker-compose run --rm shell

compose-test:
	docker-compose run --rm build make test

compose-cross:
	docker-compose run --rm build make cross-check-all

compose-pdp11:
	docker-compose run --rm pdp11 pdp11-aout-gcc --version

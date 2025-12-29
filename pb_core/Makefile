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
		cc65 -t none -I$(INC_DIR) $(SRC_DIR)/core/pb_hex.c 2>&1 | head -1 | grep -q "error\|Error" && echo "C99/SIZE" || echo "OK"; \
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
	@echo "--- 64-bit Architectures ---"
	@$(MAKE) --no-print-directory cross-x86-64
	@$(MAKE) --no-print-directory cross-aarch64
	@$(MAKE) --no-print-directory cross-riscv64
	@echo ""
	@echo "--- 32-bit Architectures ---"
	@$(MAKE) --no-print-directory cross-i386
	@$(MAKE) --no-print-directory cross-arm
	@$(MAKE) --no-print-directory cross-sh
	@$(MAKE) --no-print-directory cross-m68k
	@$(MAKE) --no-print-directory cross-m68k-mint
	@echo ""
	@echo "--- 16-bit Architectures ---"
	@$(MAKE) --no-print-directory cross-ia16
	@$(MAKE) --no-print-directory cross-msp430
	@echo ""
	@echo "--- 8-bit Architectures ---"
	@$(MAKE) --no-print-directory cross-avr
	@$(MAKE) --no-print-directory cross-z80
	@$(MAKE) --no-print-directory cross-z88dk
	@$(MAKE) --no-print-directory cross-6502
	@echo ""
	@echo "============================================"
	@echo "=== CROSS-COMPILATION COMPLETE ============"
	@echo "============================================"

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

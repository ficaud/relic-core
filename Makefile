# SPDX-License-Identifier: GPL-3.0-or-later
#
# Makefile — Relic Core development tasks.
#
# Orchestrates the Zephyr firmware builds, the native unit tests, the WASM demo
# build and the JTAG flash step. The firmware itself is built by `west build`;
# this file only centralizes the board → build-directory convention (build-*)
# so the same commands work from the CLI, from Neovim (overseer auto-discovers
# these targets) and from the devcontainer.
#
# Usage:
#   make help                     list available targets
#   make build                    build the default board (esp32s3)
#   make build BOARD=wroom        build a specific board
#   make pristine BOARD=xiao      pristine build
#   make test                     build + run native unit tests
#   make flash-jtag               flash ESP32-S3 via remote OpenOCD (RPi)
#   make wasm                     build the WASM demo
#   make clean                    remove all build-* directories

# Board selection: esp32s3 | wroom | xiao
BOARD ?= esp32s3

# Zephyr workspace / SDK locations (devcontainer defaults).
ZEPHYR_BASE ?= $(CURDIR)/zephyr
ZEPHYR_SDK_INSTALL_DIR ?= /opt/zephyr-sdk

export ZEPHYR_BASE
export ZEPHYR_SDK_INSTALL_DIR

# Resolve toolchain binaries to absolute paths. `/opt/emsdk/upstream/emscripten`
# is on PATH and ships a `cmake/` directory (Emscripten CMake modules) that
# would otherwise shadow the real `cmake` executable during make's direct exec,
# producing "make: cmake: Permission denied". The shell skips non-executable
# PATH entries, so `command -v` returns the real binary.
CMAKE := $(shell command -v cmake 2>/dev/null || echo cmake)
CTEST := $(shell command -v ctest 2>/dev/null || echo ctest)

# Board → west target + build directory.
ifeq ($(BOARD),esp32s3)
WEST_TARGET := esp32s3_devkitc/esp32s3/procpu
BUILD_DIR := build-esp32s3
else ifeq ($(BOARD),wroom)
WEST_TARGET := doit_esp32_devkit_v1/esp32/procpu
BUILD_DIR := build-esp32wroom
else ifeq ($(BOARD),xiao)
WEST_TARGET := xiao_esp32s3/esp32s3/procpu
BUILD_DIR := build-xiao
else
$(error Unknown BOARD "$(BOARD)"; expected esp32s3, wroom or xiao)
endif

.PHONY: help build pristine _build _pristine test flash-jtag wasm clean \
	build-esp32s3 build-wroom build-xiao

help:
	@echo "Relic Core development tasks"
	@echo ""
	@echo "  make build [BOARD=esp32s3|wroom|xiao]   build firmware (default: esp32s3)"
	@echo "  make pristine [BOARD=...]               pristine build (west -p always)"
	@echo "  make build-esp32s3 / build-wroom / build-xiao"
	@echo "  make test                               build + run native unit tests"
	@echo "  make flash-jtag                         flash ESP32-S3 via remote OpenOCD"
	@echo "  make wasm                               build the WASM demo"
	@echo "  make clean                              remove all build-* dirs"

build: _build

_build:
	west build -b $(WEST_TARGET) -d $(BUILD_DIR) .
	ln -sfn $(BUILD_DIR)/compile_commands.json compile_commands.json

build-esp32s3:
	$(MAKE) _build BOARD=esp32s3

build-wroom:
	$(MAKE) _build BOARD=wroom

build-xiao:
	$(MAKE) _build BOARD=xiao

pristine: _pristine

_pristine:
	west build -b $(WEST_TARGET) -d $(BUILD_DIR) -p always .
	ln -sfn $(BUILD_DIR)/compile_commands.json compile_commands.json

test:
	$(CMAKE) --preset tests
	$(CMAKE) --build --preset tests
	$(CTEST) --test-dir build/tests --output-on-failure

flash-jtag:
	./tools/flash-esp32s3-jtag.sh

wasm:
	$(MAKE) -C demo

clean:
	rm -rf build-*/
	rm -f compile_commands.json

# Convenience targets for rttyPad (CMake remains the build system).
# Requires CMake >= 3.21 and Qt6 Widgets/WebSockets >= 6.4.

CMAKE ?= cmake
BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= 4

.PHONY: all configure build debug release run clean dist distclean help

all: build

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(JOBS)"

release:
	$(MAKE) build BUILD_TYPE=Release

debug:
	$(MAKE) build BUILD_TYPE=Debug

run: build
ifeq ($(OS),Windows_NT)
	"$(BUILD_DIR)/rttyPad.exe"
else ifeq ($(shell uname -s),Darwin)
	open "$(BUILD_DIR)/rttyPad.app"
else
	"$(BUILD_DIR)/rttyPad"
endif

clean:
	@if [ -d "$(BUILD_DIR)" ]; then $(CMAKE) --build "$(BUILD_DIR)" --target clean; fi

dist:
	bash ./dist-win-rttyPad.sh

distclean:
	rm -rf "$(BUILD_DIR)"

help:
	@printf '%s\n' 'make / make build    Build rttyPad (Release by default)' 'make debug           Build with debug symbols' 'make run             Build and start rttyPad' 'make clean           Clean compiled objects' 'make distclean       Remove the build directory' 'make dist            Package Windows release (MSYS2 UCRT64)' 'make JOBS=8         Set parallel build jobs' 'make BUILD_DIR=...   Use a different build directory'

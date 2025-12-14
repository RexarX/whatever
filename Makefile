# Root Makefile
# Builds both client and embedded subprojects
# Use `make help` to see available commands

# ============================================================================
# Configuration
# ============================================================================

# Python interpreter
PYTHON := python

# Subproject directories
CLIENT_DIR := client
EMBEDDED_DIR := embedded

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	DETECTED_PLATFORM := linux
endif
ifeq ($(UNAME_S),Darwin)
	DETECTED_PLATFORM := macos
endif
ifeq ($(OS),Windows_NT)
	DETECTED_PLATFORM := windows
endif

PLATFORM ?= $(DETECTED_PLATFORM)

# ============================================================================
# Phony Targets
# ============================================================================

.PHONY: all client embedded clean help
.PHONY: client-build client-configure client-clean client-test
.PHONY: client-format client-lint client-install-deps
.PHONY: client-android-build client-android-build-debug client-android-build-release
.PHONY: embedded-build embedded-flash embedded-monitor embedded-clean
.PHONY: embedded-menuconfig embedded-format embedded-test embedded-test-unit
.PHONY: embedded-test-integration embedded-test-clean
.PHONY: check
.PHONY: client-docker-build client-docker-run client-docker-shell client-docker-clean
.PHONY: client-docker-rebuild client-docker-test client-docker-configure
.PHONY: client-docker-format client-docker-lint
.PHONY: client-docker-compose-up client-docker-compose-down

# ============================================================================
# Combined Targets
# ============================================================================

# Default target - show help
all: help

# Build both projects (client only - embedded requires ESP-IDF setup)
build-all: client-build
	@echo ""
	@echo "Note: Embedded project requires ESP-IDF environment."
	@echo "To build embedded: cd $(EMBEDDED_DIR) && make build"

# Clean all projects
clean: client-clean embedded-clean
	@echo "✓ All projects cleaned"

# ============================================================================
# Client Targets
# ============================================================================

# Build client (default)
client: client-build

# Build client project
client-build:
	@echo "Building client..."
	@cd $(CLIENT_DIR) && $(MAKE) build $(MAKEFLAGS)

# Configure client project
client-configure:
	@echo "Configuring client..."
	@cd $(CLIENT_DIR) && $(MAKE) configure $(MAKEFLAGS)

# Clean client project
client-clean:
	@echo "Cleaning client..."
	@cd $(CLIENT_DIR) && $(MAKE) clean

# Run client tests
client-test:
	@echo "Running client tests..."
	@cd $(CLIENT_DIR) && $(MAKE) test $(MAKEFLAGS)

# Format client code
client-format:
	@echo "Formatting client code..."
	@cd $(CLIENT_DIR) && $(MAKE) format

# Lint client code
client-lint:
	@echo "Linting client code..."
	@cd $(CLIENT_DIR) && $(MAKE) lint $(MAKEFLAGS)

# Install client dependencies
client-install-deps:
	@echo "Installing client dependencies..."
	@cd $(CLIENT_DIR) && $(MAKE) install-deps

# Install client dependencies with Conan
client-install-deps-conan:
	@echo "Installing client dependencies via Conan..."
	@cd $(CLIENT_DIR) && $(MAKE) install-deps-conan

# ============================================================================

# Android Client Targets
# ============================================================================

# Build Android client (defaults to Release, arm64)
client-android-build: client-android-build-release

# Build Android client (Debug, arm64)
client-android-build-debug:
	@echo "Building Android client (Debug, arm64)..."
	@cd $(CLIENT_DIR) && $(MAKE) android-build-debug $(MAKEFLAGS)

# Build Android client (Release, arm64) - APK packaging is automatic
client-android-build-release:
	@echo "Building Android client (Release, arm64) with automatic APK packaging..."
	@cd $(CLIENT_DIR) && $(MAKE) android-build-release $(MAKEFLAGS)

# ============================================================================

# Client Docker Targets
# ============================================================================

# Build client Docker image
client-docker-build:
	@echo "Building client Docker image..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-build

# Build client in Docker container
client-docker-run:
	@echo "Building client in Docker..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-run $(MAKEFLAGS)

# Open interactive shell in client Docker container
client-docker-shell:
	@echo "Opening client Docker shell..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-shell

# Clean client Docker artifacts
client-docker-clean:
	@echo "Cleaning client Docker..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-clean

# Rebuild client Docker image (no cache)
client-docker-rebuild:
	@echo "Rebuilding client Docker image..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-rebuild

# Run client tests in Docker
client-docker-test:
	@echo "Running client tests in Docker..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-test

# Configure client in Docker
client-docker-configure:
	@echo "Configuring client in Docker..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-configure $(MAKEFLAGS)

# Format client code in Docker
client-docker-format:
	@echo "Formatting client code in Docker..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-format

# Lint client code in Docker
client-docker-lint:
	@echo "Linting client code in Docker..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-lint

# Start client Docker Compose services
client-docker-compose-up:
	@echo "Starting client Docker Compose services..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-compose-up

# Stop client Docker Compose services
client-docker-compose-down:
	@echo "Stopping client Docker Compose services..."
	@cd $(CLIENT_DIR) && $(MAKE) docker-compose-down

# ============================================================================
# Embedded Targets
# ============================================================================

# Build embedded (default)
embedded: embedded-build

# Build embedded project
embedded-build:
	@echo "Building embedded..."
	@cd $(EMBEDDED_DIR) && $(MAKE) build $(MAKEFLAGS)

# Flash embedded firmware
embedded-flash:
	@echo "Flashing embedded..."
	@cd $(EMBEDDED_DIR) && $(MAKE) flash $(MAKEFLAGS)

# Monitor embedded serial output
embedded-monitor:
	@echo "Starting embedded monitor..."
	@cd $(EMBEDDED_DIR) && $(MAKE) monitor $(MAKEFLAGS)

# Build, flash, and monitor embedded
embedded-flash-monitor:
	@echo "Building, flashing, and monitoring embedded..."
	@cd $(EMBEDDED_DIR) && $(MAKE) flash-monitor $(MAKEFLAGS)

# Clean embedded project
embedded-clean:
	@echo "Cleaning embedded..."
	@cd $(EMBEDDED_DIR) && $(MAKE) clean

# Open embedded configuration menu
embedded-menuconfig:
	@echo "Opening embedded menuconfig..."
	@cd $(EMBEDDED_DIR) && $(MAKE) menuconfig

# Format embedded code
embedded-format:
	@echo "Formatting embedded code..."
	@cd $(EMBEDDED_DIR) && $(MAKE) format

# Set embedded target chip
embedded-set-target:
	@echo "Setting embedded target..."
	@cd $(EMBEDDED_DIR) && $(MAKE) set-target $(MAKEFLAGS)

# Run embedded tests (host-based)
embedded-test:
	@echo "Running embedded tests..."
	@cd $(EMBEDDED_DIR) && $(MAKE) test $(MAKEFLAGS)

# Run embedded unit tests only
embedded-test-unit:
	@echo "Running embedded unit tests..."
	@cd $(EMBEDDED_DIR) && $(MAKE) test-unit $(MAKEFLAGS)

# Run embedded integration tests only
embedded-test-integration:
	@echo "Running embedded integration tests..."
	@cd $(EMBEDDED_DIR) && $(MAKE) test-integration $(MAKEFLAGS)

# Clean embedded tests
embedded-test-clean:
	@echo "Cleaning embedded tests..."
	@cd $(EMBEDDED_DIR) && $(MAKE) test-clean

# ============================================================================
# Utilities
# ============================================================================

# Check prerequisites for both projects
check:
	@echo "Checking prerequisites..."
	@echo ""
	@echo "=== General ==="
	@command -v cmake >/dev/null 2>&1 && echo "✓ CMake found" || echo "✗ CMake not found"
	@command -v $(PYTHON) >/dev/null 2>&1 && echo "✓ Python found" || echo "✗ Python not found"
	@command -v git >/dev/null 2>&1 && echo "✓ Git found" || echo "✗ Git not found"
	@command -v ninja >/dev/null 2>&1 && echo "✓ Ninja found" || echo "  Ninja not found (optional)"
	@echo ""
	@echo "=== Client (Qt + OpenCV) ==="
	@command -v gcc >/dev/null 2>&1 && echo "✓ GCC found" || echo "  GCC not found"
	@command -v clang >/dev/null 2>&1 && echo "✓ Clang found" || echo "  Clang not found"
	@command -v conan >/dev/null 2>&1 && echo "✓ Conan found" || echo "  Conan not found (optional)"
	@command -v qmake >/dev/null 2>&1 && echo "✓ Qt found (qmake)" || echo "  Qt not found via qmake"
	@pkg-config --exists opencv4 2>/dev/null && echo "✓ OpenCV4 found" || echo "  OpenCV4 not found via pkg-config"
	@echo ""
	@echo "=== Embedded (ESP-IDF) ==="
	@if [ -n "$$IDF_PATH" ]; then \
		echo "✓ IDF_PATH: $$IDF_PATH"; \
	else \
		echo "✗ IDF_PATH not set (run: . $$IDF_PATH/export.sh)"; \
	fi
	@command -v idf.py >/dev/null 2>&1 && echo "✓ idf.py found" || echo "✗ idf.py not found"
	@echo ""

# Format all code
format: client-format embedded-format
	@echo "✓ All code formatted"

# ============================================================================
# Help
# ============================================================================

help:
	@echo "Root Makefile"
	@echo "============="
	@echo ""
	@echo "This project consists of two subprojects:"
	@echo "  - client/    Qt + OpenCV desktop application for face tracking"
	@echo "  - embedded/  ESP32 firmware for servo control"
	@echo ""
	@echo "Usage:"
	@echo "  make <target> [VARIABLE=value ...]"
	@echo ""
	@echo "Client Targets:"
	@echo "  client                 Build client project (alias for client-build)"
	@echo "  client-build           Build the client application"
	@echo "  client-configure       Configure client CMake"
	@echo "  client-clean           Clean client build files"
	@echo "  client-test            Run client tests"
	@echo "  client-format          Format client code"
	@echo "  client-lint            Lint client code"
	@echo "  client-install-deps    Install client dependencies"
	@echo "  client-install-deps-conan  Install client deps via Conan"
	@echo ""
	@echo "Client Docker Targets (via Docker Compose):"
	@echo "  client-docker-build    Build client Docker image"
	@echo "  client-docker-run      Build client in Docker container"
	@echo "  client-docker-shell    Open interactive shell in client Docker"
	@echo "  client-docker-clean    Clean client Docker artifacts"
	@echo "  client-docker-rebuild  Rebuild client Docker image (no cache)"
	@echo "  client-docker-test     Run client tests in Docker"
	@echo "  client-docker-configure  Configure client in Docker"
	@echo "  client-docker-format   Format client code in Docker"
	@echo "  client-docker-lint     Lint client code in Docker"
	@echo "  client-docker-compose-up    Start Docker Compose services"
	@echo "  client-docker-compose-down  Stop Docker Compose services"
	@echo ""
	@echo "Android Client Targets (APK packaging is automatic):"
	@echo "  client-android-build         Build Android client + APK (Release, arm64)"
	@echo "  client-android-build-debug   Build Android client + APK (Debug, arm64)"
	@echo "  client-android-build-release Build Android client + APK (Release, arm64)"
	@echo ""
	@echo "Embedded Targets:"
	@echo "  embedded               Build embedded project (alias for embedded-build)"
	@echo "  embedded-build         Build the ESP32 firmware"
	@echo "  embedded-flash         Flash firmware to device"
	@echo "  embedded-monitor       Open serial monitor"
	@echo "  embedded-flash-monitor Build, flash, and monitor"
	@echo "  embedded-clean         Clean embedded build files"
	@echo "  embedded-menuconfig    Open ESP-IDF configuration menu"
	@echo "  embedded-format        Format embedded code"
	@echo "  embedded-set-target    Set ESP32 target chip"
	@echo "  embedded-test          Run embedded tests (host-based)"
	@echo "  embedded-test-unit     Run embedded unit tests only"
	@echo "  embedded-test-integration  Run embedded integration tests only"
	@echo "  embedded-test-clean    Clean embedded test builds"
	@echo ""
	@echo "Combined Targets:"
	@echo "  clean                  Clean all projects"
	@echo "  format                 Format all code"
	@echo "  check                  Check all prerequisites"
	@echo "  help                   Show this help"
	@echo ""
	@echo "Variables (passed to subprojects):"
	@echo "  BUILD_TYPE     Build type (Debug, Release, RelWithDebInfo)"
	@echo "  COMPILER       Compiler (gcc, clang, msvc)"
	@echo "  PORT           Serial port for embedded (e.g., /dev/ttyUSB0)"
	@echo "  IDF_TARGET     ESP32 target chip (esp32, esp32s3, etc.)"
	@echo ""
	@echo "Examples:"
	@echo "  make client                              # Build client"
	@echo "  make client BUILD_TYPE=Debug             # Debug build"
	@echo "  make client-install-deps-conan           # Install deps via Conan"
	@echo "  make client-docker-build                 # Build client Docker image"
	@echo "  make client-docker-run                   # Build client in Docker"
	@echo "  make client-docker-run BUILD_TYPE=Debug  # Debug build in Docker"
	@echo "  make client-docker-shell                 # Interactive shell in Docker"
	@echo "  make client-docker-test                  # Run tests in Docker"
	@echo "  make client-docker-compose-up            # Start Docker Compose"
	@echo "  make client-docker-compose-down          # Stop Docker Compose"
	@echo "  make embedded                            # Build ESP32 firmware"
	@echo "  make embedded-flash PORT=/dev/ttyUSB0    # Flash to device"
	@echo "  make embedded-test                       # Run embedded tests"
	@echo "  make check                               # Check prerequisites"
	@echo ""
	@echo "Getting Started:"
	@echo "  1. Check prerequisites:     make check"
	@echo "  2. Install client deps:     make client-install-deps"
	@echo "  3. Build client:            make client"
	@echo "  4. Setup ESP-IDF:           . \$$IDF_PATH/export.sh"
	@echo "  5. Build embedded:          make embedded"
	@echo "  6. Flash embedded:          make embedded-flash"
	@echo ""

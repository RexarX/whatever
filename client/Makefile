# Client Makefile
# Simple wrapper around Python build scripts
# Use `make help` to see available commands

# ============================================================================
# Configuration
# ============================================================================

# Python interpreter
PYTHON := python3

# Script paths
SCRIPTS_DIR := scripts
BUILD_SCRIPT := $(SCRIPTS_DIR)/build.py
CONFIGURE_SCRIPT := $(SCRIPTS_DIR)/configure.py
INSTALL_DEPS_SCRIPT := $(SCRIPTS_DIR)/install_deps.py
LINT_SCRIPT := $(SCRIPTS_DIR)/lint.py
FORMAT_SCRIPT := $(SCRIPTS_DIR)/format.py
ANDROID_SCRIPT := $(SCRIPTS_DIR)/android.py

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

# Build configuration
PLATFORM ?= $(DETECTED_PLATFORM)

# Build type (must be set explicitly or inherited)
# BUILD_TYPE is not set by default

# Build directory (nested structure: build/<type>/<platform>/)
BUILD_DIR := build/$(BUILD_TYPE)/$(PLATFORM)

# User can provide variables via command line:
# make build BUILD_TYPE=Release COMPILER=clang
BUILD_OPTS :=
ifdef BUILD_TYPE
	BUILD_OPTS += --type $(BUILD_TYPE)
endif
ifdef COMPILER
	BUILD_OPTS += --compiler $(COMPILER)
endif
ifdef BUILD_SYSTEM
	BUILD_OPTS += --build-system "$(BUILD_SYSTEM)"
endif
ifdef JOBS
	BUILD_OPTS += --jobs $(JOBS)
endif
ifdef TESTS
	BUILD_OPTS += --tests
endif
ifdef NO_TESTS
	BUILD_OPTS += --no-tests
endif
ifdef USE_CONAN
	BUILD_OPTS += --use-conan
endif
ifdef NO_CONAN
	BUILD_OPTS += --no-conan
endif

# ============================================================================
# Phony Targets
# ============================================================================

.PHONY: all build configure clean rebuild test help
.PHONY: format format-check lint
.PHONY: install-deps check
.PHONY: docker-build docker-run docker-shell docker-clean docker-rebuild
.PHONY: docker-test docker-configure docker-format docker-lint
.PHONY: android-build android-build-debug android-build-release

# ============================================================================
# Main Targets
# ============================================================================

# Default target
all: build

# Configure the project
configure:
	@$(PYTHON) $(CONFIGURE_SCRIPT) $(BUILD_OPTS) --no-interactive

# Build the project
build:
	@$(PYTHON) $(BUILD_SCRIPT) $(BUILD_OPTS) --no-interactive

# Android (Qt/NDK) builds via scripts/android.py
# Note: APK packaging is automatic via CMake's qt_finalize_target() + androiddeployqt
android-build: android-build-release

android-build-release:
	@echo "Building Android Release (includes automatic APK packaging)..."
	@$(PYTHON) $(ANDROID_SCRIPT) --preset android-qt-release-arm64

android-build-debug:
	@echo "Building Android Debug (includes automatic APK packaging)..."
	@$(PYTHON) $(ANDROID_SCRIPT) --preset android-qt-debug-arm64

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@rm -rf build/
	@rm -rf bin/
	@rm -rf .cpm_cache/
	@echo "✓ Build directory cleaned"

# Full rebuild
rebuild: clean build

# Run tests
test:
	@if [ -z "$(BUILD_TYPE)" ]; then \
		echo "Error: BUILD_TYPE is not set."; \
		echo "Usage: make test BUILD_TYPE=release PLATFORM=linux"; \
		exit 1; \
	fi; \
	echo "Running tests in $(BUILD_DIR)..."; \
	if [ -d "$(BUILD_DIR)" ]; then \
		cd $(BUILD_DIR) && ctest --output-on-failure; \
	else \
		echo "Error: Build directory not found at $(BUILD_DIR)"; \
		echo "Run 'make build BUILD_TYPE=$(BUILD_TYPE) PLATFORM=$(PLATFORM)' first."; \
		exit 1; \
	fi

# ============================================================================
# Dependency Management
# ============================================================================

# Install dependencies
install-deps:
	@$(PYTHON) $(INSTALL_DEPS_SCRIPT)

# Install dependencies with Conan
install-deps-conan:
	@$(PYTHON) $(INSTALL_DEPS_SCRIPT) --use-conan

# ============================================================================
# Code Quality
# ============================================================================

# Format code
format:
	@if [ -f "$(FORMAT_SCRIPT)" ]; then \
		$(PYTHON) $(FORMAT_SCRIPT); \
	else \
		echo "Warning: $(FORMAT_SCRIPT) not found"; \
	fi

# Check code formatting
format-check:
	@if [ -f "$(FORMAT_SCRIPT)" ]; then \
		$(PYTHON) $(FORMAT_SCRIPT) --check; \
	else \
		echo "Warning: $(FORMAT_SCRIPT) not found"; \
	fi

# Run linter
lint:
	@if [ -z "$(BUILD_TYPE)" ]; then \
		echo "Error: BUILD_TYPE is not set."; \
		echo "Usage: make lint BUILD_TYPE=release PLATFORM=linux"; \
		exit 1; \
	fi; \
	if [ -f "$(LINT_SCRIPT)" ]; then \
		$(PYTHON) $(LINT_SCRIPT) --platform $(PLATFORM) --type $(BUILD_TYPE); \
	else \
		echo "Warning: $(LINT_SCRIPT) not found"; \
	fi

# ============================================================================
# Utilities
# ============================================================================

# Check prerequisites
check:
	@echo "Checking prerequisites..."
	@echo ""
	@command -v cmake >/dev/null 2>&1 && echo "✓ CMake found" || echo "✗ CMake not found"
	@command -v $(PYTHON) >/dev/null 2>&1 && echo "✓ Python found" || echo "✗ Python not found"
	@command -v ninja >/dev/null 2>&1 && echo "✓ Ninja found" || echo "  Ninja not found (optional)"
	@command -v make >/dev/null 2>&1 && echo "✓ Make found" || echo "  Make not found (optional)"
	@command -v gcc >/dev/null 2>&1 && echo "✓ GCC found" || echo "  GCC not found (optional)"
	@command -v clang >/dev/null 2>&1 && echo "✓ Clang found" || echo "  Clang not found (optional)"
	@command -v conan >/dev/null 2>&1 && echo "✓ Conan found" || echo "  Conan not found (optional)"
	@command -v qmake >/dev/null 2>&1 && echo "✓ Qt found" || echo "  Qt not found (qmake)"
	@pkg-config --exists libboost 2>/dev/null && echo "✓ Boost found" || echo "  Boost not found via pkg-config"
	@pkg-config --exists spdlog 2>/dev/null && echo "✓ spdlog found" || echo "  spdlog not found via pkg-config"
	@pkg-config --exists opencv4 2>/dev/null && echo "✓ OpenCV4 found" || echo "  OpenCV4 not found via pkg-config"
	@pkg-config --exists protobuf 2>/dev/null && echo "✓ protobuf found" || echo "  protobuf not found via pkg-config"
	@echo ""

# ============================================================================
# Docker Targets (via Docker Compose)
# ============================================================================

# Check if docker-compose is available
HAS_DOCKER_COMPOSE := $(shell command -v docker-compose 2>/dev/null)

# Build Docker image using Docker Compose
docker-build:
	@echo "Building Docker image via Docker Compose..."
	@docker-compose build
	@echo "✓ Docker image built successfully"

# Build project using Docker Compose
docker-run:
	@echo "Building project in Docker container..."
	@docker-compose run --rm client make build $(MAKEFLAGS)

# Open interactive shell in Docker container
docker-shell:
	@echo "Opening interactive shell in Docker container..."
	@docker-compose run --rm -it client /bin/bash

# Clean Docker build artifacts
docker-clean:
	@echo "Cleaning Docker containers and volumes..."
	@docker-compose down -v 2>/dev/null || true
	@echo "✓ Docker cleanup complete"

# Rebuild Docker image (no cache)
docker-rebuild:
	@echo "Rebuilding Docker image (no cache)..."
	@docker-compose build --no-cache
	@echo "✓ Docker image rebuilt successfully"

# Run tests in Docker container
docker-test:
	@echo "Running tests in Docker container..."
	@docker-compose run --rm client make test BUILD_TYPE=Release PLATFORM=linux

# Configure project in Docker container
docker-configure:
	@echo "Configuring project in Docker container..."
	@docker-compose run --rm client make configure $(MAKEFLAGS)

# Format code in Docker container
docker-format:
	@echo "Formatting code in Docker container..."
	@docker-compose run --rm client make format

# Run linter in Docker container
docker-lint:
	@echo "Running linter in Docker container..."
	@docker-compose run --rm client make lint BUILD_TYPE=Release PLATFORM=linux

# Start Docker Compose services
docker-compose-up:
	@echo "Starting Docker Compose services..."
	@docker-compose up

# Stop Docker Compose services
docker-compose-down:
	@echo "Stopping Docker Compose services..."
	@docker-compose down

# ============================================================================
# Help
# ============================================================================

# Help message
help:
	@echo "Phone Holder Client Makefile"
	@echo "============================="
	@echo ""
	@echo "Usage:"
	@echo "  make <target> [VARIABLE=value ...]"
	@echo ""
	@echo "Main Targets:"
	@echo "  configure        Configure CMake (calls configure.py)"
	@echo "  build            Build the project (calls build.py, which configures if needed)"
	@echo "  clean            Clean build directory"
	@echo "  rebuild          Clean and rebuild"
	@echo "  test             Run tests"
	@echo "  install-deps     Install dependencies (system packages)"
	@echo "  install-deps-conan  Install dependencies via Conan"
	@echo ""
	@echo "Docker Targets (via Docker Compose):"
	@echo "  docker-build     Build Docker image via Docker Compose"
	@echo "  docker-run       Build project in Docker container"
	@echo "  docker-shell     Open interactive shell in Docker"
	@echo "  docker-clean     Clean Docker containers and volumes"
	@echo "  docker-rebuild   Rebuild Docker image (no cache)"
	@echo "  docker-test      Run tests in Docker container"
	@echo "  docker-configure Configure project in Docker"
	@echo "  docker-format    Format code in Docker"
	@echo "  docker-lint      Run linter in Docker"
	@echo "  docker-compose-up   Start Docker Compose services"
	@echo "  docker-compose-down Stop Docker Compose services"
	@echo ""
	@echo "Code Quality:"
	@echo "  format           Format code"
	@echo "  format-check     Check code formatting"
	@echo "  lint             Run linter (respects PLATFORM and BUILD_TYPE)"
	@echo ""
	@echo "Utilities:"
	@echo "  check            Check build prerequisites"
	@echo "  help             Show this help"
	@echo ""
	@echo "Optional Variables:"
	@echo "  BUILD_TYPE       Build type (Debug, Release, RelWithDebInfo)"
	@echo "  PLATFORM         Target platform (linux, macos, windows) - default: auto-detected"
	@echo "  COMPILER         Compiler (gcc, clang, clang-cl, msvc)"
	@echo "  BUILD_SYSTEM     Build system (Ninja, Unix Makefiles, Visual Studio 17 2022)"
	@echo "  JOBS             Number of parallel jobs"
	@echo "  TESTS            Enable tests (any value enables)"
	@echo "  NO_TESTS         Disable tests (any value enables)"
	@echo "  USE_CONAN        Use Conan for dependencies (any value enables)"
	@echo "  NO_CONAN         Don't use Conan (any value enables)"
	@echo ""
	@echo "Examples:"
	@echo "  make install-deps                           # Check/install system dependencies"
	@echo "  make install-deps-conan                     # Install dependencies via Conan"
	@echo "  make configure                              # Configure CMake"
	@echo "  make build                                  # Build (configures first if needed)"
	@echo "  make build BUILD_TYPE=Debug                 # Debug build"
	@echo "  make build BUILD_TYPE=Release COMPILER=clang # Release with Clang"
	@echo "  make build USE_CONAN=1                      # Build with Conan dependencies"
	@echo "  make test BUILD_TYPE=Debug                  # Run tests"
	@echo "  make lint PLATFORM=linux BUILD_TYPE=Debug   # Lint for Linux debug build"
	@echo "  make clean build                            # Clean and rebuild"
	@echo ""
	@echo "Docker Examples:"
	@echo "  make docker-build                           # Build Docker image"
	@echo "  make docker-run                             # Build project in Docker"
	@echo "  make docker-run BUILD_TYPE=Debug            # Debug build in Docker"
	@echo "  make docker-shell                           # Interactive shell in Docker"
	@echo "  make docker-test                            # Run tests in Docker"
	@echo "  make docker-compose-up                      # Start Docker Compose services"
	@echo "  make docker-compose-down                    # Stop Docker Compose services"
	@echo ""
	@echo "For more options, use the scripts directly:"
	@echo "  python $(INSTALL_DEPS_SCRIPT) --help"
	@echo "  python $(CONFIGURE_SCRIPT) --help"
	@echo "  python $(BUILD_SCRIPT) --help"
	@echo ""

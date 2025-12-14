# Embedded Makefile
# Wrapper around ESP-IDF build system (idf.py)
# Use `make help` to see available commands

# ============================================================================
# Configuration
# ============================================================================

# Python interpreter
PYTHON := python3

# Script paths
SCRIPTS_DIR := scripts
INSTALL_DEPS_SCRIPT := $(SCRIPTS_DIR)/install_deps.py
FORMAT_SCRIPT := $(SCRIPTS_DIR)/format.py
LINT_SCRIPT := $(SCRIPTS_DIR)/lint.py

# Test directory
TESTS_DIR := tests
TESTS_BUILD_DIR := $(TESTS_DIR)/build

# ESP-IDF target chip (can be overridden)
# Supported: esp32, esp32s2, esp32s3, esp32c3, esp32c6, esp32h2
IDF_TARGET ?= esp32

# Serial port for flashing (auto-detected if not specified)
# Examples: /dev/ttyUSB0, /dev/ttyACM0, COM3
PORT ?=

# Baud rate for flashing
BAUD ?= 460800

# Number of parallel jobs
JOBS ?=

# Build options
PORT_OPTS :=
BAUD_OPTS :=
JOBS_OPTS :=

ifdef PORT
	PORT_OPTS := -p $(PORT)
endif

ifdef BAUD
	BAUD_OPTS := -b $(BAUD)
endif

ifdef JOBS
	JOBS_OPTS := -j $(JOBS)
endif

# ============================================================================
# Phony Targets
# ============================================================================

.PHONY: all build flash monitor flash-monitor clean fullclean reconfigure
.PHONY: menuconfig set-target app app-flash partition-table
.PHONY: size size-components size-files
.PHONY: format format-check lint install-deps
.PHONY: erase-flash read-flash bootloader
.PHONY: test test-configure test-build test-clean test-help
.PHONY: check help

# ============================================================================
# Main Targets
# ============================================================================

# Default target - build the project
all: build

# Build the project
build:
	idf.py $(JOBS_OPTS) build

# Build only the app (skip bootloader and partition table if already built)
app:
	idf.py $(JOBS_OPTS) app

# Flash firmware to device
flash:
	idf.py $(PORT_OPTS) $(BAUD_OPTS) flash

# Flash only the app (faster for iterative development)
app-flash:
	idf.py $(PORT_OPTS) $(BAUD_OPTS) app-flash

# Build and flash
build-flash: build flash

# Open serial monitor
monitor:
	idf.py $(PORT_OPTS) monitor

# Build, flash, and monitor in one command
flash-monitor:
	idf.py $(PORT_OPTS) $(BAUD_OPTS) $(JOBS_OPTS) build flash monitor

# Build and monitor (without flashing)
build-monitor: build monitor

# Clean build directory
clean:
	idf.py fullclean
	@echo "✓ Build directory cleaned"

# Full clean (removes sdkconfig and build directory)
fullclean:
	idf.py fullclean
	@rm -rf build/
	@rm -rf sdkconfig sdkconfig.old
	@echo "✓ Full clean completed"

# Reconfigure (useful after changing sdkconfig manually)
reconfigure:
	idf.py reconfigure

# ============================================================================
# Configuration
# ============================================================================

# Open ESP-IDF configuration menu
menuconfig:
	idf.py menuconfig

# Set target chip
set-target:
	idf.py set-target $(IDF_TARGET)
	@echo "✓ Target set to $(IDF_TARGET)"

# ============================================================================
# Build Components
# ============================================================================

# Build partition table only
partition-table:
	idf.py partition-table

# Build bootloader only
bootloader:
	idf.py bootloader

# ============================================================================
# Size Information
# ============================================================================

# Show firmware size
size:
	idf.py size

# Show component size breakdown
size-components:
	idf.py size-components

# Show per-file size breakdown
size-files:
	idf.py size-files

# ============================================================================
# Flash Operations
# ============================================================================

# Erase entire flash
erase-flash:
	idf.py $(PORT_OPTS) erase-flash
	@echo "✓ Flash erased"

# Read flash contents
read-flash:
	idf.py $(PORT_OPTS) read-flash

# Flash bootloader only
flash-bootloader:
	idf.py $(PORT_OPTS) $(BAUD_OPTS) bootloader-flash

# ============================================================================
# Dependency Management
# ============================================================================

# Install dependencies (ESP-IDF, nanopb, etc.)
install-deps:
	@if [ -f "$(INSTALL_DEPS_SCRIPT)" ]; then \
		$(PYTHON) $(INSTALL_DEPS_SCRIPT); \
	else \
		echo "Error: $(INSTALL_DEPS_SCRIPT) not found"; \
		exit 1; \
	fi

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
	@if [ -f "$(LINT_SCRIPT)" ]; then \
		$(PYTHON) $(LINT_SCRIPT); \
	else \
		echo "Warning: $(LINT_SCRIPT) not found"; \
	fi

# ============================================================================
# Tests
# ============================================================================

# Configure tests
test-configure:
	@echo "Configuring tests..."
	@cd $(TESTS_DIR) && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build tests
test-build: test-configure
	@echo "Building tests..."
	@cd $(TESTS_DIR) && cmake --build build

# Run tests
test: test-build
	@echo "Running tests..."
	@cd $(TESTS_DIR) && ctest --test-dir build --output-on-failure

# Run unit tests only
test-unit: test-build
	@echo "Running unit tests..."
	@cd $(TESTS_DIR) && ctest --test-dir build -L unit --output-on-failure

# Run integration tests only
test-integration: test-build
	@echo "Running integration tests..."
	@cd $(TESTS_DIR) && ctest --test-dir build -L integration --output-on-failure

# Clean tests
test-clean:
	@echo "Cleaning tests..."
	@rm -rf $(TESTS_BUILD_DIR)
	@echo "✓ Tests cleaned"

# Show test help
test-help:
	@cd $(TESTS_DIR) && cmake --build build --target test_help 2>/dev/null || \
		echo "Run 'make test-configure' first to see test help"

# ============================================================================
# Utilities
# ============================================================================

# Check ESP-IDF environment and tools
check:
	@echo "Checking ESP-IDF environment..."
	@echo ""
	@if [ -n "$$IDF_PATH" ]; then \
		echo "✓ IDF_PATH: $$IDF_PATH"; \
	else \
		echo "✗ IDF_PATH not set"; \
		echo "  Please run: . \$$HOME/esp/esp-idf/export.sh"; \
		echo "  (or your ESP-IDF installation path)"; \
	fi
	@command -v idf.py >/dev/null 2>&1 && echo "✓ idf.py found" || echo "✗ idf.py not found"
	@command -v $(PYTHON) >/dev/null 2>&1 && echo "✓ Python found" || echo "✗ Python not found"
	@command -v cmake >/dev/null 2>&1 && echo "✓ CMake found" || echo "✗ CMake not found"
	@command -v ninja >/dev/null 2>&1 && echo "✓ Ninja found" || echo "  Ninja not found (optional)"
	@command -v esptool.py >/dev/null 2>&1 && echo "✓ esptool.py found" || echo "  esptool.py not found"
	@echo ""
	@if [ -f "sdkconfig" ]; then \
		echo "Current configuration:"; \
		grep "^CONFIG_IDF_TARGET=" sdkconfig || echo "  Target: not configured"; \
	else \
		echo "No sdkconfig found - project not configured yet"; \
		echo "Run 'make menuconfig' or 'make set-target IDF_TARGET=esp32'"; \
	fi
	@echo ""

# Show ESP-IDF version
version:
	@idf.py --version

# ============================================================================
# Help
# ============================================================================

help:
	@echo "Embedded Makefile (ESP-IDF)"
	@echo "==========================="
	@echo ""
	@echo "Usage:"
	@echo "  make <target> [VARIABLE=value ...]"
	@echo ""
	@echo "Build Targets:"
	@echo "  build            Build the project (default)"
	@echo "  app              Build only the app (faster incremental builds)"
	@echo "  clean            Clean build directory"
	@echo "  fullclean        Full clean (removes sdkconfig too)"
	@echo "  reconfigure      Reconfigure CMake"
	@echo ""
	@echo "Flash Targets:"
	@echo "  flash            Flash firmware to device"
	@echo "  app-flash        Flash only the app (faster)"
	@echo "  build-flash      Build and flash"
	@echo "  flash-monitor    Build, flash, and monitor"
	@echo "  erase-flash      Erase device flash memory"
	@echo ""
	@echo "Monitor:"
	@echo "  monitor          Open serial monitor (Ctrl+] to exit)"
	@echo "  build-monitor    Build and monitor"
	@echo ""
	@echo "Configuration:"
	@echo "  menuconfig       Open ESP-IDF configuration menu"
	@echo "  set-target       Set target chip (use IDF_TARGET variable)"
	@echo ""
	@echo "Components:"
	@echo "  bootloader       Build bootloader only"
	@echo "  partition-table  Build partition table only"
	@echo ""
	@echo "Size Information:"
	@echo "  size             Show firmware size"
	@echo "  size-components  Show component size breakdown"
	@echo "  size-files       Show per-file size breakdown"
	@echo ""
	@echo "Dependencies:"
	@echo "  install-deps     Install project dependencies (nanopb, etc.)"
	@echo ""
	@echo "Code Quality:"
	@echo "  format           Format code using clang-format"
	@echo "  format-check     Check code formatting"
	@echo "  lint             Run static analysis using clang-tidy"
	@echo ""
	@echo "Utilities:"
	@echo "  check            Check ESP-IDF environment and configuration"
	@echo "  version          Show ESP-IDF version"
	@echo "  help             Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  IDF_TARGET       Target chip (esp32, esp32s2, esp32s3, esp32c3, esp32c6, esp32h2)"
	@echo "                   Default: esp32"
	@echo "  PORT             Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0, COM3)"
	@echo "                   Default: auto-detected"
	@echo "  BAUD             Baud rate for flashing"
	@echo "                   Default: 460800"
	@echo "  JOBS             Number of parallel build jobs"
	@echo "                   Default: auto-detected"
	@echo ""
	@echo "Examples:"
	@echo "  make                                    # Build project"
	@echo "  make flash PORT=/dev/ttyUSB0           # Flash to specific port"
	@echo "  make flash-monitor PORT=/dev/ttyUSB0   # Build, flash, and monitor"
	@echo "  make app-flash                          # Quick flash (app only)"
	@echo "  make set-target IDF_TARGET=esp32s3     # Change target chip"
	@echo "  make menuconfig                         # Configure project"
	@echo "  make size-components                    # Analyze binary size"
	@echo "  make format                             # Format all code"
	@echo "  make lint                               # Run static analysis"
	@echo "  make clean build                        # Clean and rebuild"
	@echo "  make test                               # Run all tests"
	@echo "  make test-unit                          # Run unit tests only"
	@echo ""
	@echo "Setup ESP-IDF environment first:"
	@echo "  . \$$HOME/esp/esp-idf/export.sh"
	@echo ""
	@echo "Or add an alias to your ~/.bashrc or ~/.zshrc:"
	@echo "  alias get_idf='. \$$HOME/esp/esp-idf/export.sh'"
	@echo ""

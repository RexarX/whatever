# Face Tracking Servo Control System

A project for creating an embedded device that follows the user's face using servos. The actual face detection computation happens on a connected desktop/mobile client using its camera, and the client sends commands to the embedded device to move servos accordingly.

[[_TOC_]]

## Features

- **Client Application**
  - Qt6 GUI for user interface
  - OpenCV for face detection and tracking
  - Eigen3 for efficient linear algebra
  - Manual servo control via sliders
  - Bluetooth/WiFi communication with embedded device (TODO)
  - Protocol Buffers for message serialization
  - Docker support for consistent cross-platform builds
  - Android builds (Qt for Android via CMake presets + helper script)

- **Embedded Firmware**
  - ESP32 support (ESP-IDF framework)
  - Dual servo control (pan/tilt)
  - Bluetooth and WiFi connectivity (TODO)
  - Nanopb for lightweight protobuf on embedded
  - Configuration persistence via NVS

## System Requirements

### Supported Platforms

- Linux (Ubuntu 20.04+, Arch, Fedora)
- macOS (Intel/Apple Silicon)
- Windows 11 (WSL2 or Docker Desktop)

### Hardware Requirements

**Client**: Intel/AMD x86_64 or ARM64 (Apple Silicon), 4GB+ RAM, USB camera  
**Embedded**: ESP32 development board, USB serial cable, 2x servo motors, 5V power supply

## Installation

### Option 1: Docker (Recommended)

```bash
# Ubuntu/Debian
sudo apt-get install docker.io docker-compose
sudo systemctl start docker
sudo usermod -aG docker $USER

# Arch
sudo pacman -S docker docker-compose
sudo systemctl start docker
sudo usermod -aG docker $USER

# Fedora
sudo dnf install docker docker-compose
sudo systemctl start docker
sudo usermod -aG docker $USER

# macOS (Homebrew)
brew install docker docker-compose
# Start Docker Desktop
```

Quick start:

```bash
make client-docker-build
make client-docker-run
make client-docker-test
```

## Android Build (Debug/Release) + Debugging

The Qt client supports Android builds using CMake presets with built-in Qt deployment. The build process automatically handles all library dependencies and APK packaging. From the repository root you can use the following targets (these forward to `client/Makefile`):

```bash
make client-android-build-debug      # Debug build (arm64-v8a) - builds APK automatically
make client-android-build-release    # Release build (arm64-v8a) - builds APK automatically
make client-android-build            # Alias for release
```

Or directly using the helper script:

```bash
cd client
python3 scripts/android.py --preset android-qt-relwithdebinfo-arm64
```

### Prerequisites (Android)

You must have:

- Android SDK installed and `ANDROID_SDK_ROOT` set
- Android NDK installed and `ANDROID_NDK_ROOT` set
- Qt for Android installed and `QT_ANDROID_DIR` set (Qt 6.x, arm64-v8a ABI)
- Qt host tools installed and `QT_HOST_DIR` set (Qt 6.x, Linux desktop - contains androiddeployqt)
- OpenCV Android SDK and `OPENCV_ANDROID_SDK` set

The CMake presets expect:

- `QT_ANDROID_DIR` to contain `lib/cmake/Qt6/qt.toolchain.cmake` (Android ABI libraries)
- `QT_HOST_DIR` to contain `bin/androiddeployqt` or `libexec/androiddeployqt` (host tools)
- `OPENCV_ANDROID_SDK` to contain `sdk/native/jni` (used as `OpenCV_DIR`)

**Important:** Qt for Android requires both the Android ABI libraries (`QT_ANDROID_DIR`) and the host desktop tools (`QT_HOST_DIR`).

### Build APK and run on device

1. Build the APK (this automatically runs CMake, builds native code, and packages APK):

```bash
make client-android-build-debug
# Or for release:
make client-android-build-release
```

The APK will be generated at:

- Debug: `client/build/debug/android-arm64/android-build/build/outputs/apk/debug/android-build-debug.apk`
- Release: `client/build/release/android-arm64/android-build/build/outputs/apk/release/android-build-release-unsigned.apk`

2. **Option A:** Install directly to device:

```bash
adb install client/build/debug/android-arm64/android-build/build/outputs/apk/debug/android-build-debug.apk
```

3. **Option B:** Open in Android Studio for debugging:

- Open Android Studio
- `File -> Open...`
- Select: `client/build/debug/android-arm64/android-build/`
- Choose your device (USB or emulator)
- Press **Run** (or **Debug** to attach debugger)
- Use **Logcat** to see crashes and logs

### Environment variables

Required environment variables (export them in your shell or set via CLI flags):

- `ANDROID_SDK_ROOT` - Path to Android SDK
- `ANDROID_NDK_ROOT` - Path to Android NDK
- `QT_ANDROID_DIR` - Path to Qt for Android (arm64-v8a), e.g., `/path/to/Qt/6.x.x/android_arm64_v8a`
- `QT_HOST_DIR` - Path to Qt host tools (Linux desktop), e.g., `/path/to/Qt/6.x.x/gcc_64`
- `OPENCV_ANDROID_SDK` - Path to OpenCV Android SDK

You can override them via CLI flags:

```bash
cd client
python3 scripts/android.py \
    --preset android-qt-debug-arm64 \
    --sdk /path/to/android/sdk \
    --ndk /path/to/android/ndk \
    --qt /path/to/Qt/6.x.x/android_arm64_v8a \
    --qt-host /path/to/Qt/6.x.x/gcc_64 \
    --opencv /path/to/opencv-android-sdk
```

### How it works (CMake-based deployment)

The build now uses Qt6's built-in Android deployment via `qt_finalize_target()`:

1. **Configure:** CMake generates the build system with Qt's Android toolchain
2. **Build:** Compiles native code and generates `android-<target>-deployment-settings.json`
3. **Deploy:** `androiddeployqt` automatically runs (via POST_BUILD command) to:
   - Collect all Qt dependencies (libraries, plugins, QML modules)
   - Bundle extra libraries specified in `QT_ANDROID_EXTRA_LIBS` (OpenCV, custom libs)
   - Generate Gradle project with proper `local.properties`
   - Build APK using Gradle

**No manual library copying needed!** Everything is handled automatically by Qt's deployment system.

### Native Installation

```bash
python scripts/install_deps.py
```

## Dependencies

### Client Dependencies

| Dependency   | Min Version | Purpose               |
| ------------ | ----------- | --------------------- |
| CMake        | 3.25+       | Build system          |
| C++ Compiler | C++23       | GCC 13+, Clang 16+    |
| Boost        | 1.82+       | Utilities             |
| OpenCV       | 4.8+        | Face detection        |
| Qt6          | 6.0+        | GUI framework         |
| Protobuf     | 3.21+       | Message serialization |
| Python 3     | 3.8+        | Build scripts         |

### Embedded Dependencies

- ESP-IDF 4.4+ (tested with 5.x)
- Python 3 (for tools)
- ESP32 development board

## Build Scripts

The project includes Python build scripts in `client/scripts/`. They automate configuration, building, testing, and dependency management.

### install_deps.py - Install Dependencies

Check what dependencies are missing:

```bash
cd client
python3 scripts/install_deps.py
```

Install system dependencies:

```bash
cd client
python3 scripts/install_deps.py
# Follow the apt-get/pacman/dnf command shown, or use Make:
make client-install-deps
```

Install via Conan (automatic package manager):

```bash
cd client
python3 scripts/install_deps.py --use-conan

# Or via Make
make client-install-deps-conan
```

Full workflow with Conan:

```bash
cd client

# Install Conan dependencies
python3 scripts/install_deps.py --use-conan

# Configure
python3 scripts/configure.py --use-conan

# Build
python3 scripts/build.py --use-conan

# Test
cd build/release/linux && ctest --output-on-failure
```

### configure.py - Configure CMake

```bash
cd client

# Simple configuration
python3 scripts/configure.py

# Debug build
python3 scripts/configure.py --type Debug

# Release with Clang
python3 scripts/configure.py --type Release --compiler clang

# Or via Make
make client-configure BUILD_TYPE=release COMPILER=clang
```

### build.py - Build Project

```bash
cd client

# Simple build
python3 scripts/build.py

# Release build
python3 scripts/build.py --type Release

# Parallel build (8 jobs)
python3 scripts/build.py --jobs 8

# Or via Make
make client-build BUILD_TYPE=release JOBS=8
```

### format.py and lint.py - Code Quality

```bash
cd client

# Format code
python3 scripts/format.py

# Check formatting without modifying
python3 scripts/format.py --check

# Run linter
python3 scripts/lint.py --type Release

# Or via Make
make client-format
make client-lint BUILD_TYPE=release
```

### Script Help

Each script has built-in help:

```bash
cd client
python3 scripts/install_deps.py --help
python3 scripts/configure.py --help
python3 scripts/build.py --help
python3 scripts/format.py --help
python3 scripts/lint.py --help
```

## Quick Start - Development Workflow

### Setup

```bash
# Install dependencies
make client-install-deps

# Configure
make client-configure BUILD_TYPE=debug

# Build
make client-build BUILD_TYPE=debug

# Test
make client-test BUILD_TYPE=debug
```

### Development

```bash
# Edit code
$EDITOR src/main.cpp

# Rebuild (incremental)
make client-build BUILD_TYPE=debug

# Run tests
make client-test BUILD_TYPE=debug

# Format code
make client-format

# Lint code
make client-lint BUILD_TYPE=debug
```

### Release Build

```bash
# Install dependencies
make client-install-deps

# Build release
make client-build BUILD_TYPE=release

# Test
make client-test BUILD_TYPE=release

# With Clang compiler
make client-build BUILD_TYPE=release COMPILER=clang
```

## Docker Development

```bash
# Build Docker image
make client-docker-build

# Build project in Docker
make client-docker-run

# Run tests
make client-docker-test

# Debug build
make client-docker-run BUILD_TYPE=debug

# Interactive shell
make client-docker-shell

# Format code
make client-docker-format

# Lint code
make client-docker-lint

# Cleanup
make client-docker-clean
```

## Embedded Development (ESP32)

### ESP-IDF Setup

```bash
# Clone ESP-IDF
git clone -b v5.5.1 --depth 1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install
./install.sh

# Setup environment (run every session)
. export.sh
```

### Build and Flash

```bash
# Build firmware
make embedded-build

# Flash to device
make embedded-flash PORT=/dev/ttyUSB0

# Monitor serial output
make embedded-monitor PORT=/dev/ttyUSB0

# All at once
make embedded-flash-monitor PORT=/dev/ttyUSB0

# Configure options
make embedded-menuconfig

# Set target device
make embedded-set-target IDF_TARGET=esp32s3
```

## Make Targets

### Client Targets

From project root or `client/` directory:

```bash
make help                          # Show all targets
make check                         # Check prerequisites
make client-install-deps           # Install dependencies
make client-install-deps-conan     # Install via Conan
make client-configure              # Configure CMake
make client-build BUILD_TYPE=release # Build project
make client-test BUILD_TYPE=release  # Run tests
make client-format                 # Format code
make client-lint BUILD_TYPE=release  # Run linter
make client-clean                  # Clean build files
```

### Docker Targets

```bash
make client-docker-build           # Build image
make client-docker-run             # Build in Docker
make client-docker-test            # Test in Docker
make client-docker-shell           # Interactive shell
make client-docker-format          # Format in Docker
make client-docker-lint            # Lint in Docker
make client-docker-clean           # Cleanup
make client-docker-compose-up      # Start services
make client-docker-compose-down    # Stop services
```

### Embedded Targets

```bash
make embedded-build                # Build firmware
make embedded-flash PORT=/dev/ttyUSB0 # Flash
make embedded-monitor PORT=/dev/ttyUSB0 # Monitor
make embedded-flash-monitor PORT=/dev/ttyUSB0 # All
make embedded-clean                # Clean
make embedded-menuconfig           # Configuration
```

## Build Variables

Pass variables to Make targets:

```bash
# Build type
make client-build BUILD_TYPE=debug          # Debug
make client-build BUILD_TYPE=release        # Release (default)
make client-build BUILD_TYPE=relwithdebinfo # Release with debug info

# Compiler
make client-build COMPILER=gcc              # GCC (default)
make client-build COMPILER=clang            # Clang

# Parallel jobs
make client-build JOBS=8                    # Use 8 jobs

# Combined
make client-build BUILD_TYPE=release COMPILER=clang JOBS=8

# Use Conan
make client-build BUILD_TYPE=release USE_CONAN=1

# Embedded port
make embedded-flash PORT=/dev/ttyUSB0       # Specify port
```

## Protocol Buffers

Communication uses Protocol Buffers defined in `proto/messages.proto`.

Message types:

- **Command** - Client to embedded (move, home, calibrate, stop)
- **Response** - Embedded to client (status, errors)
- **Handshake** - Connection establishment

## Development

### Code Formatting

```bash
make client-format              # Format all code
cd client && make format-check  # Check without modifying
```

### Linting

```bash
make client-lint BUILD_TYPE=release  # Run static analysis
```

### Running Tests

```bash
make client-test BUILD_TYPE=release       # Test
make client-docker-test                   # Test in Docker
```

## Troubleshooting

### ESP-IDF not found

```bash
. $IDF_PATH/export.sh
idf.py --version
```

### Device not found during flash

```bash
# Check available ports
ls /dev/tty* | grep -E "(USB|ACM|AMA)"

# May need to grant permissions
sudo usermod -aG dialout $USER
# Log out and back in
```

### Docker daemon not running

```bash
sudo systemctl start docker
```

### Permission denied with Docker

```bash
sudo usermod -aG docker $USER
# Log out and back in
```

### Docker image build fails

```bash
# Rebuild without cache
make client-docker-rebuild
```

## License

MIT License - see `LICENSE` file for details.

## Additional Documentation

Run `make help` for all available commands.

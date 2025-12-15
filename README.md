# Face Tracking Servo Control System

A cross-platform project for building an embedded device that tracks a user's face using servos. Face detection and tracking are performed on a desktop or mobile client, which communicates with the embedded device to control servo movement.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [System Requirements](#system-requirements)
- [Installation & Setup](#installation--setup)
  - [Client Setup](#client-setup)
  - [Embedded Setup (ESP32)](#embedded-setup-esp32)
- [Build & Development Workflow](#build--development-workflow)
  - [Client Build](#client-build)
  - [Embedded Build](#embedded-build)
  - [Docker Workflow](#docker-workflow)
  - [Android Build](#android-build)
- [Dependencies](#dependencies)
- [Build Scripts](#build-scripts)
- [Make Targets & Build Variables](#make-targets--build-variables)
- [Protocol Buffers](#protocol-buffers)
- [Troubleshooting](#troubleshooting)
- [License](#license)
- [Additional Documentation](#additional-documentation)

---

## Overview

This project enables real-time face tracking using a combination of a desktop/mobile client and an embedded ESP32 device. The client application detects and tracks faces using a camera, then sends movement commands to the embedded device, which controls two servos (pan/tilt) to follow the user's face.

---

## Features

### Client Application

- Qt6 GUI for user interface
- OpenCV for face detection and tracking
- Manual servo control via sliders
- Bluetooth/WiFi communication with embedded device (planned)
- Protocol Buffers for message serialization
- Docker support for cross-platform builds
- Android builds (via Qt for Android and CMake presets)

### Embedded Firmware

- ESP32 support (ESP-IDF framework)
- Dual servo control (pan/tilt)
- Bluetooth and WiFi connectivity (planned)
- Nanopb for lightweight protobuf
- Configuration persistence via NVS

---

## System Requirements

### Supported Platforms

- Linux (Ubuntu 20.04+, Arch, Fedora)
- macOS (Intel/Apple Silicon)
- Windows 11 (WSL2 or Docker Desktop)

### Hardware Requirements

- **Client:** Intel/AMD x86_64 or ARM64 (Apple Silicon), 4GB+ RAM, USB camera
- **Embedded:** ESP32 development board, USB serial cable, 2x servo motors, 5V power supply

---

## Installation & Setup

### Client Setup

#### Native Installation

```bash
python scripts/install_deps.py
```

#### Docker Installation (Recommended)

Install Docker:

- **Ubuntu/Debian:**  
  `sudo apt-get install docker.io docker-compose`
- **Arch:**  
  `sudo pacman -S docker docker-compose`
- **Fedora:**  
  `sudo dnf install docker docker-compose`
- **macOS (Homebrew):**  
  `brew install docker docker-compose`

Start Docker and add your user to the docker group if needed.

Quick start:

```bash
make client-docker-build
make client-docker-run
make client-docker-test
```

### Embedded Setup (ESP32)

#### ESP-IDF Installation

```bash
git clone -b v5.5.1 --depth 1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
. export.sh
```

#### Build and Flash

```bash
make embedded-build
make embedded-flash PORT=/dev/ttyUSB0
make embedded-monitor PORT=/dev/ttyUSB0
```

---

## Build & Development Workflow

### Client Build

```bash
make client-install-deps
make client-configure BUILD_TYPE=Release
make client-build BUILD_TYPE=Release
make client-test BUILD_TYPE=Release
```

#### Development Cycle

- Edit code in `src/`
- Rebuild: `make client-build BUILD_TYPE=Release`
- Run tests: `make client-test BUILD_TYPE=Release`
- Format: `make client-format`
- Lint: `make client-lint BUILD_TYPE=Release`

#### Release Build

```bash
make client-build BUILD_TYPE=Release
make client-test BUILD_TYPE=Release
```

### Embedded Build

```bash
make embedded-build
make embedded-flash PORT=/dev/ttyUSB0
make embedded-monitor PORT=/dev/ttyUSB0
```

### Docker Workflow

```bash
make client-docker-build
make client-docker-run
make client-docker-test
make client-docker-shell
make client-docker-format
make client-docker-lint
make client-docker-clean
```

### Android Build

The Qt client supports Android builds using CMake presets and a helper script.

#### Prerequisites

- Android SDK (`ANDROID_SDK_ROOT`)
- Android NDK (`ANDROID_NDK_ROOT`)
- Qt for Android (`QT_ANDROID_DIR`)
- Qt host tools (`QT_HOST_DIR`)
- OpenCV Android SDK (`OPENCV_ANDROID_SDK`)

#### Build APK

```bash
make client-android-build-debug      # Debug build
make client-android-build-release    # Release build
```

Or directly:

```bash
cd client
python3 scripts/android.py --preset android-qt-relwithdebinfo-arm64
```

#### Install APK

```bash
adb install client/build/debug/android-arm64/android-build/build/outputs/apk/debug/android-build-debug.apk
```

---

## Dependencies

### Client

| Dependency   | Min Version | Purpose               |
| ------------ | ----------- | --------------------- |
| CMake        | 3.25+       | Build system          |
| C++ Compiler | C++23       | GCC 13+, Clang 16+    |
| Boost        | 1.82+       | Utilities             |
| OpenCV       | 4.8+        | Face detection        |
| Qt6          | 6.0+        | GUI framework         |
| Protobuf     | 3.21+       | Message serialization |
| Python 3     | 3.8+        | Build scripts         |

### Embedded

- ESP-IDF 4.4+ (tested with 5.x)
- Python 3 (for tools)
- ESP32 development board

---

## Build Scripts

Python scripts in `client/scripts/` automate configuration, building, testing, and dependency management.

- `install_deps.py` – Install/check dependencies
- `configure.py` – Configure CMake
- `build.py` – Build project
- `format.py` – Format code
- `lint.py` – Run linter

Each script supports `--help` for usage details.

### Interactive Usage Examples

All Python scripts are interactive by default. When you run them without extra arguments, they will prompt you for missing information, confirm actions, and guide you through the process step by step.

#### Example: Installing Dependencies

```bash
cd client
python3 scripts/install_deps.py
```

- The script will check for missing dependencies and interactively ask if you want to install them.
- If system packages are missing, it will show the required commands and ask for confirmation before proceeding.

#### Example: Configuring the Project

```bash
cd client
python3 scripts/configure.py
```

- You will be prompted to select build type (Debug/Release), compiler, and other options if not specified via command line.
- The script will display detected options and ask for confirmation before generating build files.

#### Example: Building the Project

```bash
cd client
python3 scripts/build.py
```

- If you haven't configured the project yet, the script will offer to run the configuration step for you.
- You can specify build type or number of jobs interactively, or pass them as arguments.

#### Example: Formatting and Linting

```bash
cd client
python3 scripts/format.py
python3 scripts/lint.py
```

- These scripts will ask for confirmation before making changes (formatting) or let you choose the build type for linting.

#### Getting Help

All scripts provide detailed help and usage instructions:

```bash
python3 scripts/install_deps.py --help
python3 scripts/configure.py --help
python3 scripts/build.py --help
python3 scripts/format.py --help
python3 scripts/lint.py --help
```

You can always run a script without arguments to be guided interactively, or provide command-line flags for automation.

---

## Make Targets & Build Variables

### Common Make Targets

- `make help` – Show all targets
- `make check` – Check prerequisites

#### Client

- `make client-install-deps`
- `make client-configure`
- `make client-build BUILD_TYPE=Release`
- `make client-test BUILD_TYPE=Release`
- `make client-format`
- `make client-lint BUILD_TYPE=Release`
- `make client-clean`

#### Docker

- `make client-docker-build`
- `make client-docker-run`
- `make client-docker-test`
- `make client-docker-shell`
- `make client-docker-format`
- `make client-docker-lint`
- `make client-docker-clean`
- `make client-docker-compose-up`
- `make client-docker-compose-down`

#### Embedded

- `make embedded-build`
- `make embedded-flash PORT=/dev/ttyUSB0`
- `make embedded-monitor PORT=/dev/ttyUSB0`
- `make embedded-flash-monitor PORT=/dev/ttyUSB0`
- `make embedded-clean`
- `make embedded-menuconfig`

### Build Variables

- `BUILD_TYPE=Release|Release|RelWithDebInfo`
- `COMPILER=gcc|clang`
- `JOBS=8` (parallel build)
- `USE_CONAN=1` (use Conan for dependencies)
- `PORT=/dev/ttyUSB0` (embedded flash/monitor)

---

## Protocol Buffers

Communication uses Protocol Buffers defined in `proto/messages.proto`:

- **Command:** Client to embedded (move, home, calibrate, stop)
- **Response:** Embedded to client (status, errors)
- **Handshake:** Connection establishment

---

## Troubleshooting

### ESP-IDF Not Found

```bash
. $IDF_PATH/export.sh
idf.py --version
```

### Device Not Found During Flash

```bash
ls /dev/tty* | grep -E "(USB|ACM|AMA)"
sudo usermod -aG dialout $USER
# Log out and back in
```

### Docker Daemon Not Running

```bash
sudo systemctl start docker
```

### Permission Denied with Docker

```bash
sudo usermod -aG docker $USER
# Log out and back in
```

### Docker Image Build Fails

```bash
make client-docker-rebuild
```

---

## License

MIT License – see `LICENSE` file for details.

---

## Additional Documentation

Run `make help` for all available commands and further documentation.

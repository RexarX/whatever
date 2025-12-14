# ESP32 Firmware
# ESP-IDF project for controlling servos based on commands from the client

# Minimum CMake version required by ESP-IDF
cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
# ESP-IDF must be installed and IDF_PATH environment variable must be set
# The project() call comes after including ESP-IDF tools
if(DEFINED ENV{IDF_PATH})
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
else()
    message(FATAL_ERROR
        "ESP-IDF not found!\n"
        "Please install ESP-IDF and set the IDF_PATH environment variable.\n"
        "\n"
        "Installation instructions:\n"
        "  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/\n"
        "\n"
        "Quick start:\n"
        "  git clone --recursive https://github.com/espressif/esp-idf.git\n"
        "  cd esp-idf\n"
        "  ./install.sh\n"
        "  . ./export.sh\n"
    )
endif()

# Set the target chip (can be overridden via menuconfig or command line)
# Supported values: esp32, esp32s2, esp32s3, esp32c3, esp32c6, esp32h2
if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "esp32" CACHE STRING "ESP-IDF target chip")
endif()

# Project name and version
project(embedded
    VERSION 0.1.0
    DESCRIPTION "ESP32 firmware for servo control"
    LANGUAGES CXX
)

# C++23 standard for the project
# ESP-IDF v5.x has GCC 12+ which supports C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add extra components path
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")

# Project-specific configuration
# These will be available in the code via sdkconfig.h

# Build options
option(EMBEDDED_BUILD_TESTS "Build embedded tests" ${PROJECT_IS_TOP_LEVEL})
option(EMBEDDED_ALLOW_CPM_DOWNLOADS "Allow automatic download of missing dependencies via CPM" ON)

# Enable testing if requested
if(EMBEDDED_BUILD_TESTS)
    enable_testing()
    message(STATUS "Testing enabled")
endif()

# Print project information
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Embedded Firmware")
message(STATUS "========================================")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  Target:      ${IDF_TARGET}")
message(STATUS "  IDF Path:    $ENV{IDF_PATH}")
message(STATUS "  C++ Std:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Language:    C++")
message(STATUS "  Build Tests: ${EMBEDDED_BUILD_TESTS}")
message(STATUS "========================================")
message(STATUS "")

# Custom components can be added in the components/ directory
# ESP-IDF will automatically find and build them

# Main application code is in the main/ directory
# ESP-IDF convention: main/CMakeLists.txt defines the main component

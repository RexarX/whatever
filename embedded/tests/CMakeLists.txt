# Embedded Tests Root Configuration
# NOTE: These tests run on the host system (not on ESP32)
# They test business logic and algorithms that don't require hardware

cmake_minimum_required(VERSION 3.25)

# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

project(EmbeddedTests
    VERSION 0.1.0
    DESCRIPTION "Host-based tests for embedded firmware"
    LANGUAGES CXX
)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set project root
set(EMBEDDED_ROOT_DIR "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Embedded project root directory")
set(PROJECT_ROOT_DIR "${EMBEDDED_ROOT_DIR}/.." CACHE PATH "Project root directory")

# Add root third_party directory for doctest
if(EXISTS "${PROJECT_ROOT_DIR}/third_party/CMakeLists.txt")
    add_subdirectory("${PROJECT_ROOT_DIR}/third_party" "${CMAKE_BINARY_DIR}/third_party")
else()
    message(FATAL_ERROR "Root third_party directory not found at: ${PROJECT_ROOT_DIR}/third_party")
endif()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${EMBEDDED_ROOT_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${EMBEDDED_ROOT_DIR}/cmake/helpers")

# Include helper modules
include(TargetUtils)
include(TestHelper)
include(Dependencies)

# Setup test dependencies (doctest, FFF)
embedded_setup_test_dependencies()

# Enable testing
enable_testing()

# Create custom build targets for different test categories
add_custom_target(all_tests)
add_custom_target(unit_tests)
add_custom_target(integration_tests)

# Module-specific test targets
add_custom_target(core_tests)

# Help target for test commands
add_custom_target(test_help
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Test Commands ==="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Run all tests:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest                           # All tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest --parallel 4              # All tests in parallel"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Run by test type:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L unit                   # All unit tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L integration            # All integration tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Run by module:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L core                   # All core tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build specific test targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target unit_tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target integration_tests"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Useful options:"
    COMMAND ${CMAKE_COMMAND} -E echo "  --verbose                       # Verbose output"
    COMMAND ${CMAKE_COMMAND} -E echo "  --output-on-failure             # Show output only on failure"
    COMMAND ${CMAKE_COMMAND} -E echo "  --stop-on-failure               # Stop on first failure"
    COMMAND ${CMAKE_COMMAND} -E echo "  --timeout <seconds>             # Set test timeout"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Examples:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L unit --output-on-failure"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -R core --parallel 4"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)

# Add subdirectories
add_subdirectory(unit)
add_subdirectory(integration)

# Print test configuration summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Embedded Tests Configuration")
message(STATUS "========================================")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Test Framework: doctest")
message(STATUS "  Mock Framework: FFF")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Build tests:")
message(STATUS "  cmake --build <build-dir>")
message(STATUS "")
message(STATUS "Run tests:")
message(STATUS "  ctest --test-dir <build-dir>")
message(STATUS "  cmake --build <build-dir> --target test")
message(STATUS "")

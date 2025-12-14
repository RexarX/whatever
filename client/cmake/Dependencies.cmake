# Main dependency configuration for Phone Holder Client
#
# This file orchestrates all dependency finding and configuration.
# Each dependency is defined in its own file in cmake/dependencies/
#
# Strategy: Conan (if available) -> System packages -> CPM fallback

include_guard(GLOBAL)

# Load dependency management helpers
include(DependencyFinder)
include(DownloadUsingCPM)

# Print configuration header
message(STATUS "")
message(STATUS "========== Client Dependency Configuration ==========")
message(STATUS "Using Conan: ${CLIENT_USE_CONAN}")
message(STATUS "Force Conan priority: ${CLIENT_FORCE_CONAN}")
if(CLIENT_FORCE_CONAN)
    message(STATUS "  → Conan packages will be checked FIRST")
    message(STATUS "  → System packages used as fallback")
    message(STATUS "  → CPM downloads for missing dependencies")
else()
    message(STATUS "  → System packages will be checked FIRST")
    message(STATUS "  → Conan used for missing dependencies (if available)")
    message(STATUS "  → CPM downloads as last resort")
endif()
message(STATUS "Allow CPM downloads: ${CLIENT_DOWNLOAD_PACKAGES}")
message(STATUS "Qt provider: external install (Linux system packages or Windows Qt SDK); Conan Qt is disabled")
message(STATUS "=====================================================")
message(STATUS "")

# ============================================================================
# Core Dependencies (always required)
# ============================================================================

message(STATUS "Finding Core Dependencies...")
message(STATUS "")

# Boost for stacktrace support (conditional - only if std::stacktrace not available)
include(cmake/dependencies/Boost.cmake)

# Android: Load Protobuf BEFORE OpenCV to avoid target conflicts
# OpenCV Android SDK bundles protobuf and creates libprotobuf IMPORTED target,
# which conflicts with CPM trying to build protobuf from source.
# By loading Protobuf first, we ensure our protobuf targets are created first.
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Protobuf for serialization and communication protocol
    include(cmake/dependencies/Protobuf.cmake)

    # OpenCV for face detection and tracking
    include(cmake/dependencies/OpenCV.cmake)

    # Qt6 for GUI
    include(cmake/dependencies/Qt.cmake)
else()
    # OpenCV for face detection and tracking
    include(cmake/dependencies/OpenCV.cmake)

    # Qt6 for GUI
    include(cmake/dependencies/Qt.cmake)

    # Protobuf for serialization and communication protocol
    include(cmake/dependencies/Protobuf.cmake)
endif()

message(STATUS "")

# ============================================================================
# Summary
# ============================================================================

client_print_dependencies()
if(CPM_PACKAGES)
    client_print_cpm_packages()
endif()
message(STATUS "")

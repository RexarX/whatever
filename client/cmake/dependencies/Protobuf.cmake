# This module handles finding Protobuf from multiple sources:
# 1. Conan (if using Conan)
# 2. System packages (apt, pacman, brew, etc.)
# 3. CPM download (fallback)
#
# Special handling for Android cross-compilation:
# - Skip pkg-config (would find host libraries which are unusable)
# - Force CPM download of protobuf source for the *target*
# - Use host protoc for code generation
#
# This file intentionally does NOT special-case OpenCV's bundled protobuf.
# For Android we always build/link protobuf via CPM.

include_guard(GLOBAL)

message(STATUS "Configuring Protobuf dependency...")

# -----------------------------------------------------------------------------
# Android: use OpenCV's bundled protobuf
# -----------------------------------------------------------------------------

if(ANDROID)
    message(STATUS "  Android build detected - using CPM protobuf 3.21.12 for target and protoc")

    # Always download protobuf via CPM for Android to ensure headers are available
    # Note: Dependencies.cmake loads Protobuf BEFORE OpenCV on Android to prevent
    # OpenCV's bundled libprotobuf IMPORTED target from conflicting with our CPM build
    set(CLIENT_FORCE_DOWNLOAD_PROTOBUF ON CACHE BOOL "Force download protobuf for Android" FORCE)
    set(CLIENT_DOWNLOAD_PROTOBUF ON CACHE BOOL "Download protobuf for Android" FORCE)

    client_dep_begin(
        NAME Protobuf
        VERSION 3.21.12
        CPM_NAME protobuf
        CPM_VERSION 3.21.12
        CPM_GITHUB_REPOSITORY protocolbuffers/protobuf
        CPM_GIT_TAG v21.12
        CPM_SOURCE_SUBDIR cmake
        CPM_OPTIONS
            "protobuf_BUILD_TESTS OFF"
            "protobuf_BUILD_EXAMPLES OFF"
            "protobuf_BUILD_PROTOBUF_BINARIES ON"
            "protobuf_BUILD_PROTOC_BINARIES ON"
            "protobuf_BUILD_LIBPROTOC ON"
            "protobuf_DISABLE_RTTI OFF"
            "protobuf_WITH_ZLIB OFF"
            "protobuf_MSVC_STATIC_RUNTIME OFF"
            "ABSL_PROPAGATE_CXX_STD ON"
    )

    client_dep_end()

    # For Android cross-compilation, we MUST use a host (x86_64) protoc binary
    # because the protoc built by CPM is for Android (ARM) and can't run on the host
    # Always download the prebuilt host protoc binary
    set(_HOST_PROTOC_URL "https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protoc-21.12-linux-x86_64.zip")
    set(_HOST_PROTOC_ARCHIVE "${CMAKE_BINARY_DIR}/protoc-21.12-linux-x86_64.zip")
    set(_HOST_PROTOC_DIR "${CMAKE_BINARY_DIR}/protoc-21.12-host")

    if(NOT EXISTS "${_HOST_PROTOC_DIR}/bin/protoc")
        message(STATUS "  Downloading host protoc binary for Android cross-compilation...")
        if(NOT EXISTS "${_HOST_PROTOC_ARCHIVE}")
            file(DOWNLOAD "${_HOST_PROTOC_URL}" "${_HOST_PROTOC_ARCHIVE}" SHOW_PROGRESS)
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_HOST_PROTOC_ARCHIVE}" DESTINATION "${_HOST_PROTOC_DIR}")
    endif()

    if(EXISTS "${_HOST_PROTOC_DIR}/bin/protoc")
        set(Protobuf_PROTOC_EXECUTABLE "${_HOST_PROTOC_DIR}/bin/protoc" CACHE FILEPATH "Protoc executable" FORCE)
        message(STATUS "  Using host protoc for Android: ${Protobuf_PROTOC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "  Failed to download host protoc for Android build")
    endif()

else()
    # -----------------------------------------------------------------------------
    # Non-Android: use normal dependency resolution (system/Conan/CPM fallback)
    # -----------------------------------------------------------------------------
    client_dep_begin(
        NAME Protobuf
        VERSION 3.21
        DEBIAN_NAMES libprotobuf-dev protobuf-compiler
        RPM_NAMES protobuf-devel protobuf-compiler
        PACMAN_NAMES protobuf
        BREW_NAMES protobuf
        PKG_CONFIG_NAMES protobuf
        CPM_NAME protobuf
        CPM_VERSION 27.3
        CPM_GITHUB_REPOSITORY protocolbuffers/protobuf
        CPM_GIT_TAG v27.3
        CPM_SOURCE_SUBDIR cmake
        CPM_OPTIONS
            "protobuf_BUILD_TESTS OFF"
            "protobuf_BUILD_EXAMPLES OFF"
            "protobuf_BUILD_PROTOBUF_BINARIES ON"
            "protobuf_BUILD_PROTOC_BINARIES ON"
            "protobuf_BUILD_LIBPROTOC ON"
            "protobuf_DISABLE_RTTI OFF"
            "protobuf_WITH_ZLIB OFF"
            "protobuf_MSVC_STATIC_RUNTIME OFF"
            "ABSL_PROPAGATE_CXX_STD ON"
    )

    client_dep_end()
endif()

# -----------------------------------------------------------------------------
# Create universal client::protobuf alias
# -----------------------------------------------------------------------------

if(NOT TARGET client::protobuf)
    add_library(protobuf_interface INTERFACE)
    add_library(client::protobuf ALIAS protobuf_interface)

    # Link Abseil targets if present (protobuf headers and/or libs rely on them for newer versions).
    if(TARGET absl::base)
        target_link_libraries(protobuf_interface INTERFACE
            absl::base
            absl::log
            absl::status
            absl::strings
            absl::span
        )
    endif()

    # Link to the protobuf runtime library target.
    if(TARGET protobuf::libprotobuf)
        target_link_libraries(protobuf_interface INTERFACE protobuf::libprotobuf)
        message(STATUS "  ✓ client::protobuf linked to protobuf::libprotobuf")
    elseif(TARGET libprotobuf)
        target_link_libraries(protobuf_interface INTERFACE libprotobuf)
        message(STATUS "  ✓ client::protobuf linked to libprotobuf (CPM)")
        # Link utf8_range for Android builds (required by protobuf 3.21+)
        if(ANDROID AND TARGET utf8_range)
            target_link_libraries(protobuf_interface INTERFACE utf8_range)
            message(STATUS "    ✓ Linked utf8_range for Android")
        elseif(ANDROID AND TARGET utf8_validity)
            target_link_libraries(protobuf_interface INTERFACE utf8_validity)
            message(STATUS "    ✓ Linked utf8_validity for Android")
        endif()
    elseif(TARGET protobuf::protobuf)
        target_link_libraries(protobuf_interface INTERFACE protobuf::protobuf)
        message(STATUS "  ✓ client::protobuf linked to protobuf::protobuf")
    else()
        message(WARNING "  ✗ No protobuf library target found")
    endif()

    # Provide include directories.
    #
    # Prefer include dirs from the protobuf target if available.
    if(TARGET protobuf::libprotobuf)
        get_target_property(_protobuf_iface_inc protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
        if(_protobuf_iface_inc)
            target_include_directories(protobuf_interface INTERFACE ${_protobuf_iface_inc})
        endif()
    elseif(TARGET libprotobuf)
        get_target_property(_protobuf_iface_inc libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
        if(_protobuf_iface_inc)
            target_include_directories(protobuf_interface INTERFACE ${_protobuf_iface_inc})
        endif()
    endif()

    # Add CPM protobuf headers explicitly when available.
    if(DEFINED _PROTOBUF_CPM_SOURCE_DIR AND EXISTS "${_PROTOBUF_CPM_SOURCE_DIR}/src")
        target_include_directories(protobuf_interface INTERFACE "${_PROTOBUF_CPM_SOURCE_DIR}/src")
        message(STATUS "    Added CPM protobuf include dir: ${_PROTOBUF_CPM_SOURCE_DIR}/src")
    elseif(DEFINED protobuf_SOURCE_DIR AND EXISTS "${protobuf_SOURCE_DIR}/src")
        target_include_directories(protobuf_interface INTERFACE "${protobuf_SOURCE_DIR}/src")
        message(STATUS "    Added protobuf include dir: ${protobuf_SOURCE_DIR}/src")
    elseif(DEFINED CPM_PACKAGE_protobuf_SOURCE_DIR AND EXISTS "${CPM_PACKAGE_protobuf_SOURCE_DIR}/src")
        target_include_directories(protobuf_interface INTERFACE "${CPM_PACKAGE_protobuf_SOURCE_DIR}/src")
        message(STATUS "    Added protobuf include dir: ${CPM_PACKAGE_protobuf_SOURCE_DIR}/src")
    endif()

    # Protobuf also includes "utf8_validity.h" from utf8_range in newer versions.
    # Add it when it exists under the protobuf source tree.
    if(DEFINED _PROTOBUF_CPM_SOURCE_DIR AND EXISTS "${_PROTOBUF_CPM_SOURCE_DIR}/third_party/utf8_range")
        target_include_directories(protobuf_interface INTERFACE "${_PROTOBUF_CPM_SOURCE_DIR}/third_party/utf8_range")
        message(STATUS "    Added utf8_range include dir: ${_PROTOBUF_CPM_SOURCE_DIR}/third_party/utf8_range")
    elseif(DEFINED protobuf_SOURCE_DIR AND EXISTS "${protobuf_SOURCE_DIR}/third_party/utf8_range")
        target_include_directories(protobuf_interface INTERFACE "${protobuf_SOURCE_DIR}/third_party/utf8_range")
        message(STATUS "    Added utf8_range include dir: ${protobuf_SOURCE_DIR}/third_party/utf8_range")
    elseif(DEFINED CPM_PACKAGE_protobuf_SOURCE_DIR AND EXISTS "${CPM_PACKAGE_protobuf_SOURCE_DIR}/third_party/utf8_range")
        target_include_directories(protobuf_interface INTERFACE "${CPM_PACKAGE_protobuf_SOURCE_DIR}/third_party/utf8_range")
        message(STATUS "    Added utf8_range include dir: ${CPM_PACKAGE_protobuf_SOURCE_DIR}/third_party/utf8_range")
    endif()

    # For non-Android system protobuf, pkg-config can supply extra transitive deps.
    if(NOT ANDROID)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PROTOBUF_PC QUIET IMPORTED_TARGET protobuf)
            if(PROTOBUF_PC_FOUND)
                target_link_libraries(protobuf_interface INTERFACE PkgConfig::PROTOBUF_PC)
                if(PROTOBUF_PC_INCLUDE_DIRS)
                    target_include_directories(protobuf_interface INTERFACE ${PROTOBUF_PC_INCLUDE_DIRS})
                endif()
            endif()
        endif()
    endif()

    # Fallback include directory search.
    if(DEFINED Protobuf_INCLUDE_DIRS)
        target_include_directories(protobuf_interface INTERFACE ${Protobuf_INCLUDE_DIRS})
    elseif(DEFINED protobuf_INCLUDE_DIRS)
        target_include_directories(protobuf_interface INTERFACE ${protobuf_INCLUDE_DIRS})
    else()
        find_path(PROTOBUF_FALLBACK_INCLUDE_DIR google/protobuf/message.h
            HINTS
                "${protobuf_SOURCE_DIR}/src"
                "${CMAKE_BINARY_DIR}/_deps/protobuf-src/src"
        )
        if(PROTOBUF_FALLBACK_INCLUDE_DIR)
            target_include_directories(protobuf_interface INTERFACE ${PROTOBUF_FALLBACK_INCLUDE_DIR})
            message(STATUS "    Using fallback protobuf include: ${PROTOBUF_FALLBACK_INCLUDE_DIR}")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Configure protoc executable for code generation
# -----------------------------------------------------------------------------

if(NOT Protobuf_PROTOC_EXECUTABLE)
    if(TARGET protobuf::protoc)
        # Use generator expression to get the executable path
        set(Protobuf_PROTOC_EXECUTABLE "$<TARGET_FILE:protobuf::protoc>")
    elseif(TARGET protoc)
        set(Protobuf_PROTOC_EXECUTABLE "$<TARGET_FILE:protoc>")
    else()
        find_program(Protobuf_PROTOC_EXECUTABLE NAMES protoc)
    endif()
endif()

if(Protobuf_PROTOC_EXECUTABLE)
    message(STATUS "  Protoc executable: ${Protobuf_PROTOC_EXECUTABLE}")
else()
    message(WARNING "  ✗ protoc executable not found - proto compilation will fail")
endif()

# Also create client::protoc alias for the compiler (host only)
# Note: protobuf::protoc might itself be an ALIAS, check first
if(NOT TARGET client::protoc)
    if(TARGET protobuf::protoc)
        get_target_property(_protoc_aliased protobuf::protoc ALIASED_TARGET)
        if(_protoc_aliased)
            # protobuf::protoc is an alias, create our alias to the real target
            add_executable(client::protoc ALIAS ${_protoc_aliased})
        else()
            add_executable(client::protoc ALIAS protobuf::protoc)
        endif()
        message(STATUS "  ✓ protoc compiler configured")
    elseif(TARGET protoc)
        get_target_property(_protoc_aliased protoc ALIASED_TARGET)
        if(_protoc_aliased)
            add_executable(client::protoc ALIAS ${_protoc_aliased})
        else()
            add_executable(client::protoc ALIAS protoc)
        endif()
        message(STATUS "  ✓ protoc compiler configured (CPM fallback)")
    endif()
endif()

# Fallback for non-namespaced targets (older Protobuf versions)
if(TARGET libprotobuf AND NOT TARGET client::protobuf)
    add_library(client::protobuf ALIAS libprotobuf)
    message(STATUS "  ✓ protobuf configured (fallback)")
endif()
if(TARGET libprotobuf-lite AND NOT TARGET client::protobuf-lite)
    add_library(client::protobuf-lite ALIAS libprotobuf-lite)
    message(STATUS "  ✓ protobuf-lite configured (fallback)")
endif()

# Set up Protobuf_LIBS variable for easy linking
set(Protobuf_LIBS "")
if(TARGET protobuf::libprotobuf)
    list(APPEND Protobuf_LIBS protobuf::libprotobuf)
    if(NOT ANDROID AND TARGET PkgConfig::PROTOBUF_PC)
        list(APPEND Protobuf_LIBS PkgConfig::PROTOBUF_PC)
    endif()
elseif(TARGET libprotobuf)
    list(APPEND Protobuf_LIBS libprotobuf)
elseif(DEFINED Protobuf_LIBRARIES)
    list(APPEND Protobuf_LIBS ${Protobuf_LIBRARIES})
endif()

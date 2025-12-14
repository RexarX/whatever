# Dependencies.cmake
# Manages external dependencies for embedded tests

include_guard(GLOBAL)

# ============================================================================
# CPM Package Manager
# ============================================================================

# Download CPM.cmake if not already available
set(CPM_DOWNLOAD_VERSION 0.38.6)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}...")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        EXPECTED_HASH SHA256=11c3fa5f1ba14f15d31c2fb63dbc8628ee133d81c8d764caad9a8db9e0bacb07
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

# ============================================================================
# Test Framework - doctest
# ============================================================================

function(embedded_add_doctest)
    # doctest is provided by root third_party directory
    # It should be added before embedded tests via add_subdirectory

    if(TARGET embedded::doctest)
        message(STATUS "doctest already configured")
        return()
    endif()

    # Try to find the actual doctest target (not alias)
    if(TARGET doctest_with_main)
        # Create embedded alias from the actual target
        add_library(embedded::doctest ALIAS doctest_with_main)
        message(STATUS "doctest configured from third_party (doctest_with_main)")
        return()
    endif()

    if(TARGET doctest)
        # Create embedded alias from the actual target
        add_library(embedded::doctest ALIAS doctest)
        message(STATUS "doctest configured from third_party (doctest)")
        return()
    endif()

    message(FATAL_ERROR "doctest not found. Make sure root third_party is added before embedded/tests.")
endfunction()

# ============================================================================
# Protobuf - nanopb (for embedded)
# ============================================================================

function(embedded_add_nanopb)
    if(TARGET embedded::nanopb)
        message(STATUS "nanopb already available")
        return()
    endif()

    # nanopb is typically provided as ESP-IDF component
    # Check if it's available in components directory
    set(NANOPB_COMPONENT_DIR "${CMAKE_SOURCE_DIR}/components/nanopb")

    if(EXISTS "${NANOPB_COMPONENT_DIR}")
        message(STATUS "nanopb component found at: ${NANOPB_COMPONENT_DIR}")
        # Create interface library for nanopb (if not provided by ESP-IDF)
        if(NOT TARGET nanopb)
            add_library(nanopb INTERFACE)
            target_include_directories(nanopb INTERFACE "${NANOPB_COMPONENT_DIR}")
        endif()
        if(NOT TARGET embedded::nanopb)
            add_library(embedded::nanopb ALIAS nanopb)
        endif()
        # ESP-IDF will handle this as a component
    else()
        message(STATUS "nanopb not found (optional). Run 'make install-deps' to install it if needed.")
    endif()
endfunction()

# ============================================================================
# Mock Framework - FFF (Fake Function Framework)
# ============================================================================

function(embedded_add_fff)
    if(TARGET embedded::fff)
        message(STATUS "FFF (Fake Function Framework) already available")
        return()
    endif()

    if(NOT EMBEDDED_ALLOW_CPM_DOWNLOADS)
        message(WARNING "FFF (Fake Function Framework) not found and EMBEDDED_ALLOW_CPM_DOWNLOADS is OFF")
        return()
    endif()

    message(STATUS "Adding FFF (Fake Function Framework)...")

    CPMAddPackage(
        NAME fff
        GITHUB_REPOSITORY meekrosoft/fff
        VERSION 1.1
        DOWNLOAD_ONLY YES
    )

    if(fff_ADDED)
        # FFF is header-only, create interface library
        add_library(fff INTERFACE)
        add_library(embedded::fff ALIAS fff)

        target_include_directories(fff INTERFACE "${fff_SOURCE_DIR}")

        message(STATUS "FFF added successfully")
    else()
        # Create embedded alias if fff exists
        if(TARGET fff AND NOT TARGET embedded::fff)
            add_library(embedded::fff ALIAS fff)
        endif()
        message(STATUS "Using existing FFF")
    endif()
endfunction()

# ============================================================================
# Utility Functions
# ============================================================================

# Add all test dependencies
function(embedded_add_test_dependencies)
    embedded_add_doctest()
    embedded_add_fff()
    # nanopb is optional for tests - only needed for protobuf message tests
    # Don't fail if not available
    if(EXISTS "${CMAKE_SOURCE_DIR}/../components/nanopb")
        embedded_add_nanopb()
    else()
        message(STATUS "nanopb not available (optional for tests)")
    endif()
endfunction()

# ============================================================================
# Main Entry Point
# ============================================================================

# Call this function to set up all test dependencies
macro(embedded_setup_test_dependencies)
    message(STATUS "Setting up embedded test dependencies...")
    embedded_add_test_dependencies()
    message(STATUS "Embedded test dependencies configured")
endmacro()

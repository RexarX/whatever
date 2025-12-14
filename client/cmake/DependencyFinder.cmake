# Helper macros for finding and configuring external dependencies
#
# This file provides utilities for finding packages from multiple sources:
# 1. Conan packages (if using Conan toolchain)
# 2. System packages (find_package, pkg-config)
# 3. CPM automatic download (as last resort)

include_guard(GLOBAL)

cmake_policy(SET CMP0054 NEW)

# Initialize global variables for dependency tracking
if(NOT DEFINED _CLIENT_DEPENDENCIES_FOUND)
    set(_CLIENT_DEPENDENCIES_FOUND "" CACHE INTERNAL "List of found dependencies")
endif()

# Detect if using Conan - check multiple possible locations
if(DEFINED CLIENT_USE_CONAN)
    # Already set, use it
elseif(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
    set(CLIENT_USE_CONAN TRUE CACHE BOOL "Using Conan for dependencies")
elseif(DEFINED CONAN_TOOLCHAIN)
    set(CLIENT_USE_CONAN TRUE CACHE BOOL "Using Conan for dependencies")
else()
    set(CLIENT_USE_CONAN FALSE CACHE BOOL "Using Conan for dependencies")
endif()

# Options for package management
option(CLIENT_DOWNLOAD_PACKAGES "Download missing packages using CPM" ON)
option(CLIENT_FORCE_DOWNLOAD_PACKAGES "Force download all packages even if system version exists" OFF)
option(CLIENT_CHECK_PACKAGE_VERSIONS "Check and enforce package version requirements" ON)
option(CLIENT_FORCE_CONAN "Force Conan packages first, use system only as fallback" OFF)

# Macro to begin package search
# Usage: client_dep_begin(
#     NAME <name>
#     [VERSION <version>]
#     [DEBIAN_NAMES <pkg1> <pkg2> ...]
#     [RPM_NAMES <pkg1> <pkg2> ...]
#     [PACMAN_NAMES <pkg1> <pkg2> ...]
#     [BREW_NAMES <pkg1> <pkg2> ...]
#     [PKG_CONFIG_NAMES <pkg1> <pkg2> ...]
#     [CPM_NAME <name>]
#     [CPM_VERSION <version>]
#     [CPM_GITHUB_REPOSITORY <repo>]
#     [CPM_URL <url>]
#     [CPM_OPTIONS <opt1> <opt2> ...]
#     [CPM_DOWNLOAD_ONLY]
# )
macro(client_dep_begin)
    set(options CPM_DOWNLOAD_ONLY)
    set(oneValueArgs NAME VERSION CPM_NAME CPM_VERSION CPM_GITHUB_REPOSITORY CPM_URL CPM_GIT_TAG CPM_SOURCE_SUBDIR)
    set(multiValueArgs DEBIAN_NAMES RPM_NAMES PACMAN_NAMES BREW_NAMES PKG_CONFIG_NAMES CPM_OPTIONS)

    cmake_parse_arguments(CLIENT_PKG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_PKG_NAME "${CLIENT_PKG_NAME}")

    # Set up CPM name if not provided
    if(NOT CLIENT_PKG_CPM_NAME)
        set(CLIENT_PKG_CPM_NAME "${CLIENT_PKG_NAME}")
    endif()

    string(TOUPPER "${CLIENT_PKG_CPM_NAME}" _PKG_CPM_NAME_UPPER)
    string(REPLACE "-" "_" _PKG_CPM_NAME_UPPER "${_PKG_CPM_NAME_UPPER}")

    # Create download options for this package
    option(
        CLIENT_DOWNLOAD_${_PKG_CPM_NAME_UPPER}
        "Download and setup ${CLIENT_PKG_CPM_NAME} if not found"
        ${CLIENT_DOWNLOAD_PACKAGES}
    )
    option(
        CLIENT_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER}
        "Force download ${CLIENT_PKG_CPM_NAME} even if system package exists"
        ${CLIENT_FORCE_DOWNLOAD_PACKAGES}
    )

    # Set version requirement
    if(CLIENT_PKG_VERSION)
        if(NOT ${_PKG_NAME}_FIND_VERSION OR "${${_PKG_NAME}_FIND_VERSION}" VERSION_LESS "${CLIENT_PKG_VERSION}")
            set("${_PKG_NAME}_FIND_VERSION" "${CLIENT_PKG_VERSION}")
        endif()
    endif()

    # Skip version checks if disabled
    if(NOT CLIENT_CHECK_PACKAGE_VERSIONS)
        unset("${_PKG_NAME}_FIND_VERSION")
    endif()

    # Check if already found
    if(TARGET ${_PKG_NAME} OR TARGET client::${_PKG_NAME})
        if(NOT ${_PKG_NAME}_FIND_VERSION)
            set("${_PKG_NAME}_FOUND" ON)
            set("${_PKG_NAME}_SKIP_CLIENT_FIND" ON)
            return()
        endif()

        if(${_PKG_NAME}_VERSION)
            if(${_PKG_NAME}_FIND_VERSION VERSION_LESS_EQUAL ${_PKG_NAME}_VERSION)
                set("${_PKG_NAME}_FOUND" ON)
                set("${_PKG_NAME}_SKIP_CLIENT_FIND" ON)
                return()
            else()
                message(FATAL_ERROR
                    "Already using version ${${_PKG_NAME}_VERSION} of ${_PKG_NAME} "
                    "when version ${${_PKG_NAME}_FIND_VERSION} was requested."
                )
            endif()
        endif()
    endif()

    # Build error message for missing packages
    set(_ERROR_MESSAGE "Could not find `${_PKG_NAME}` package.")
    if(CLIENT_PKG_DEBIAN_NAMES)
        list(JOIN CLIENT_PKG_DEBIAN_NAMES " " _pkg_names)
        string(APPEND _ERROR_MESSAGE "\n\tDebian/Ubuntu: sudo apt install ${_pkg_names}")
    endif()
    if(CLIENT_PKG_RPM_NAMES)
        list(JOIN CLIENT_PKG_RPM_NAMES " " _pkg_names)
        string(APPEND _ERROR_MESSAGE "\n\tFedora/RHEL: sudo dnf install ${_pkg_names}")
    endif()
    if(CLIENT_PKG_PACMAN_NAMES)
        list(JOIN CLIENT_PKG_PACMAN_NAMES " " _pkg_names)
        string(APPEND _ERROR_MESSAGE "\n\tArch Linux: sudo pacman -S ${_pkg_names}")
    endif()
    if(CLIENT_PKG_BREW_NAMES)
        list(JOIN CLIENT_PKG_BREW_NAMES " " _pkg_names)
        string(APPEND _ERROR_MESSAGE "\n\tmacOS: brew install ${_pkg_names}")
    endif()
    string(APPEND _ERROR_MESSAGE "\n")

    # Initialize search result variables
    set("${_PKG_NAME}_LIBRARIES")
    set("${_PKG_NAME}_INCLUDE_DIRS")
    set("${_PKG_NAME}_EXECUTABLE")
    set("${_PKG_NAME}_FOUND" FALSE)
endmacro()

# Helper to search for package components
macro(_client_dep_find_part)
    set(options)
    set(oneValueArgs PART_TYPE)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(PART "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(${_PKG_NAME}_SKIP_CLIENT_FIND)
        return()
    endif()

    # Determine variable name based on type
    if("${PART_PART_TYPE}" STREQUAL "library")
        set(_variable "${_PKG_NAME}_LIBRARIES")
        find_library(${_PKG_NAME}_LIBRARIES_${PART_NAMES}
            NAMES ${PART_NAMES}
            PATHS ${PART_PATHS}
            PATH_SUFFIXES ${PART_PATH_SUFFIXES}
        )
        list(APPEND "${_variable}" "${${_PKG_NAME}_LIBRARIES_${PART_NAMES}}")
    elseif("${PART_PART_TYPE}" STREQUAL "include")
        set(_variable "${_PKG_NAME}_INCLUDE_DIRS")
        find_path(${_PKG_NAME}_INCLUDE_DIRS_${PART_NAMES}
            NAMES ${PART_NAMES}
            PATHS ${PART_PATHS}
            PATH_SUFFIXES ${PART_PATH_SUFFIXES}
        )
        list(APPEND "${_variable}" "${${_PKG_NAME}_INCLUDE_DIRS_${PART_NAMES}}")
    elseif("${PART_PART_TYPE}" STREQUAL "program")
        set(_variable "${_PKG_NAME}_EXECUTABLE")
        find_program(${_PKG_NAME}_EXECUTABLE_${PART_NAMES}
            NAMES ${PART_NAMES}
            PATHS ${PART_PATHS}
            PATH_SUFFIXES ${PART_PATH_SUFFIXES}
        )
        list(APPEND "${_variable}" "${${_PKG_NAME}_EXECUTABLE_${PART_NAMES}}")
    else()
        message(FATAL_ERROR "Invalid PART_TYPE: ${PART_PART_TYPE}")
    endif()
endmacro()

# Find library component
macro(client_dep_find_library)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _client_dep_find_part(
        PART_TYPE library
        NAMES ${LIB_NAMES}
        PATHS ${LIB_PATHS}
        PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
    )
endmacro()

# Find include component
macro(client_dep_find_include)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(INC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _client_dep_find_part(
        PART_TYPE include
        NAMES ${INC_NAMES}
        PATHS ${INC_PATHS}
        PATH_SUFFIXES ${INC_PATH_SUFFIXES}
    )
endmacro()

# Find program component
macro(client_dep_find_program)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(PROG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _client_dep_find_part(
        PART_TYPE program
        NAMES ${PROG_NAMES}
        PATHS ${PROG_PATHS}
        PATH_SUFFIXES ${PROG_PATH_SUFFIXES}
    )
endmacro()

# Finalize package search
macro(client_dep_end)
    if(${_PKG_NAME}_SKIP_CLIENT_FIND)
        return()
    endif()

    # Try to find via standard mechanisms first
    set(_FOUND_VIA "")

    # Determine search order based on preferences
    set(_tried_system FALSE)
    set(_tried_conan FALSE)

    # If CLIENT_FORCE_CONAN is ON, try Conan first
    if(CLIENT_FORCE_CONAN AND CLIENT_USE_CONAN AND NOT CLIENT_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER})
        find_package(${_PKG_NAME} ${${_PKG_NAME}_FIND_VERSION} CONFIG QUIET)
        if(${_PKG_NAME}_FOUND)
            set(_FOUND_VIA "Conan")
        endif()
        set(_tried_conan TRUE)
    endif()

    # Try system packages (either first by default, or second if forcing Conan)
    if(NOT ${_PKG_NAME}_FOUND AND NOT CLIENT_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER})
        # Try CONFIG mode first (for CMake-aware packages)
        find_package(${_PKG_NAME} ${${_PKG_NAME}_FIND_VERSION} CONFIG QUIET)
        if(${_PKG_NAME}_FOUND)
            set(_FOUND_VIA "system (CONFIG)")
        else()
            # Try MODULE mode (for Find*.cmake files)
            find_package(${_PKG_NAME} ${${_PKG_NAME}_FIND_VERSION} MODULE QUIET)
            if(${_PKG_NAME}_FOUND)
                set(_FOUND_VIA "system (MODULE)")
            endif()
        endif()
        set(_tried_system TRUE)
    endif()

    # Try Conan if not found yet and we didn't try it earlier
    if(NOT ${_PKG_NAME}_FOUND AND NOT _tried_conan AND CLIENT_USE_CONAN AND NOT CLIENT_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER})
        find_package(${_PKG_NAME} ${${_PKG_NAME}_FIND_VERSION} CONFIG QUIET)
        if(${_PKG_NAME}_FOUND)
            set(_FOUND_VIA "Conan")
        endif()
        set(_tried_conan TRUE)
    endif()

    # Try pkg-config
    if(NOT ${_PKG_NAME}_FOUND AND CLIENT_PKG_PKG_CONFIG_NAMES)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            list(GET CLIENT_PKG_PKG_CONFIG_NAMES 0 _pkg_config_name)
            pkg_check_modules(${_PKG_NAME}_PC QUIET IMPORTED_TARGET ${_pkg_config_name})
            if(${_PKG_NAME}_PC_FOUND)
                set(${_PKG_NAME}_FOUND TRUE)
                set(_FOUND_VIA "pkg-config")
                if(NOT TARGET ${_PKG_NAME})
                    add_library(${_PKG_NAME} INTERFACE IMPORTED)
                    target_link_libraries(${_PKG_NAME} INTERFACE PkgConfig::${_PKG_NAME}_PC)
                endif()
            endif()
        endif()
    endif()

    # Try manual search if we have search criteria
    set(_required_vars)
    if(NOT "${${_PKG_NAME}_LIBRARIES}" STREQUAL "")
        list(APPEND _required_vars "${_PKG_NAME}_LIBRARIES")
    endif()
    if(NOT "${${_PKG_NAME}_INCLUDE_DIRS}" STREQUAL "")
        list(APPEND _required_vars "${_PKG_NAME}_INCLUDE_DIRS")
    endif()
    if(NOT "${${_PKG_NAME}_EXECUTABLE}" STREQUAL "")
        list(APPEND _required_vars "${_PKG_NAME}_EXECUTABLE")
    endif()

    if(_required_vars AND NOT ${_PKG_NAME}_FOUND)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(
            ${_PKG_NAME}
            REQUIRED_VARS ${_required_vars}
            VERSION_VAR ${_PKG_NAME}_VERSION
            FAIL_MESSAGE "${_ERROR_MESSAGE}"
        )
        if(${_PKG_NAME}_FOUND)
            set(_FOUND_VIA "manual search")
        endif()
    endif()

    # Try CPM as last resort
    if(NOT ${_PKG_NAME}_FOUND AND CLIENT_DOWNLOAD_${_PKG_CPM_NAME_UPPER})
        if(CLIENT_PKG_CPM_GITHUB_REPOSITORY OR CLIENT_PKG_CPM_URL)
            include(DownloadUsingCPM)
            # Build CPM arguments from parsed values
            set(_cpm_args NAME ${CLIENT_PKG_CPM_NAME})
            if(CLIENT_PKG_CPM_VERSION)
                list(APPEND _cpm_args VERSION ${CLIENT_PKG_CPM_VERSION})
            endif()
            if(CLIENT_PKG_CPM_GIT_TAG)
                list(APPEND _cpm_args GIT_TAG ${CLIENT_PKG_CPM_GIT_TAG})
            endif()
            if(CLIENT_PKG_CPM_GITHUB_REPOSITORY)
                list(APPEND _cpm_args GITHUB_REPOSITORY ${CLIENT_PKG_CPM_GITHUB_REPOSITORY})
            endif()
            if(CLIENT_PKG_CPM_URL)
                list(APPEND _cpm_args URL ${CLIENT_PKG_CPM_URL})
            endif()
            if(CLIENT_PKG_CPM_SOURCE_SUBDIR)
                list(APPEND _cpm_args SOURCE_SUBDIR ${CLIENT_PKG_CPM_SOURCE_SUBDIR})
            endif()
            if(CLIENT_PKG_CPM_OPTIONS)
                list(APPEND _cpm_args OPTIONS ${CLIENT_PKG_CPM_OPTIONS})
            endif()
            if(CLIENT_PKG_CPM_DOWNLOAD_ONLY)
                list(APPEND _cpm_args DOWNLOAD_ONLY YES)
            endif()

            client_cpm_add_package(${_cpm_args})

            if(${CLIENT_PKG_CPM_NAME}_ADDED OR TARGET ${_PKG_NAME})
                set(${_PKG_NAME}_FOUND TRUE)
                set(_FOUND_VIA "CPM download")
            endif()
        endif()
    endif()

    # Record the result
    if(${_PKG_NAME}_FOUND)
        list(FIND _CLIENT_DEPENDENCIES_FOUND "${_PKG_NAME}:${_FOUND_VIA}" _dep_index)
        if(_dep_index EQUAL -1)
            list(APPEND _CLIENT_DEPENDENCIES_FOUND "${_PKG_NAME}:${_FOUND_VIA}")
            set(_CLIENT_DEPENDENCIES_FOUND "${_CLIENT_DEPENDENCIES_FOUND}" CACHE INTERNAL "List of found dependencies")
        endif()
        message(STATUS "  ✓ ${_PKG_NAME} found via ${_FOUND_VIA}")
    else()
        message(FATAL_ERROR "${_ERROR_MESSAGE}")
    endif()
endmacro()

# Print summary of all found dependencies
function(client_print_dependencies)
    message(STATUS "")
    message(STATUS "========== Dependency Summary ==========")
    if(_CLIENT_DEPENDENCIES_FOUND)
        # Deduplicate dependencies before printing
        list(REMOVE_DUPLICATES _CLIENT_DEPENDENCIES_FOUND)
        foreach(_dep ${_CLIENT_DEPENDENCIES_FOUND})
            string(REPLACE ":" " via " _dep_msg "${_dep}")
            message(STATUS "  ✓ ${_dep_msg}")
        endforeach()
    else()
        message(STATUS "  No dependencies found yet")
    endif()
    message(STATUS "=========================================")
endfunction()

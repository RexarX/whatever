# IdfUtils.cmake
# Helper functions for configuring ESP-IDF components with Google C++ Style Guide compliance

include_guard(GLOBAL)

# ============================================================================
# Warning Configuration
# ============================================================================

# Apply standard compiler warnings to an ESP-IDF component
# Usage: embedded_idf_set_warnings(<component_target>)
function(embedded_idf_set_warnings COMPONENT_TARGET)
    set(GCC_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    # ESP-IDF might have some warnings we want to suppress for external headers
    set(GCC_SUPPRESSIONS
        -Wno-error=deprecated-declarations
    )

    target_compile_options(${COMPONENT_TARGET} PRIVATE
        ${GCC_WARNINGS}
        ${GCC_SUPPRESSIONS}
    )
endfunction()

# ============================================================================
# Optimization Configuration
# ============================================================================

# Apply standard optimization flags to an ESP-IDF component
# Usage: embedded_idf_set_optimization(<component_target>)
function(embedded_idf_set_optimization COMPONENT_TARGET)
    target_compile_options(${COMPONENT_TARGET} PRIVATE
        $<$<CONFIG:Debug>:-Og -g3 -ggdb>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:Release>:-O3>
    )
endfunction()

# ============================================================================
# C++ Standard Configuration
# ============================================================================

# Set C++23 standard for an ESP-IDF component
# Usage: embedded_idf_set_cxx_standard(<component_target> [STANDARD 23])
function(embedded_idf_set_cxx_standard COMPONENT_TARGET)
    cmake_parse_arguments(ARG "" "STANDARD" "" ${ARGN})

    if(NOT ARG_STANDARD)
        set(ARG_STANDARD 23)
    endif()

    set_property(TARGET ${COMPONENT_TARGET} PROPERTY CXX_STANDARD ${ARG_STANDARD})
    set_property(TARGET ${COMPONENT_TARGET} PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET ${COMPONENT_TARGET} PROPERTY CXX_EXTENSIONS OFF)

    # Also set via compiler flags for ESP-IDF compatibility
    target_compile_options(${COMPONENT_TARGET} PRIVATE -std=c++${ARG_STANDARD})
endfunction()

# ============================================================================
# Platform Configuration
# ============================================================================

# Apply platform-specific compile definitions to an ESP-IDF component
# Usage: embedded_idf_set_platform(<component_target>)
function(embedded_idf_set_platform COMPONENT_TARGET)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE
        $<$<CONFIG:Debug>:EMBEDDED_DEBUG_MODE>
        $<$<CONFIG:RelWithDebInfo>:EMBEDDED_RELEASE_WITH_DEBUG_INFO_MODE>
        $<$<CONFIG:Release>:EMBEDDED_RELEASE_MODE NDEBUG>
        EMBEDDED_PLATFORM_ESP32
    )
endfunction()

# ============================================================================
# Component Configuration Helper
# ============================================================================

# Apply all standard configurations to an ESP-IDF component
# Usage: embedded_idf_configure_component(<component_target>)
#
# This is a convenience function that applies:
# - C++23 standard
# - Google-style warnings
# - Optimization settings
# - Platform definitions
function(embedded_idf_configure_component COMPONENT_TARGET)
    cmake_parse_arguments(ARG "" "STANDARD" "" ${ARGN})

    if(NOT ARG_STANDARD)
        set(ARG_STANDARD 23)
    endif()

    embedded_idf_set_cxx_standard(${COMPONENT_TARGET} STANDARD ${ARG_STANDARD})
    embedded_idf_set_warnings(${COMPONENT_TARGET})
    embedded_idf_set_optimization(${COMPONENT_TARGET})
    embedded_idf_set_platform(${COMPONENT_TARGET})

    message(STATUS "Configured ESP-IDF component: ${COMPONENT_TARGET} (C++${ARG_STANDARD})")
endfunction()

# ============================================================================
# Nanopb Integration
# ============================================================================

# Link nanopb to an ESP-IDF component with proper alias
# Usage: embedded_idf_link_nanopb(<component_target>)
function(embedded_idf_link_nanopb COMPONENT_TARGET)
    # nanopb should be available as an ESP-IDF component
    if(TARGET nanopb)
        target_link_libraries(${COMPONENT_TARGET} PRIVATE nanopb)

        # Create embedded:: alias if it doesn't exist
        if(NOT TARGET embedded::nanopb)
            add_library(embedded::nanopb ALIAS nanopb)
        endif()

        message(STATUS "Linked nanopb to ${COMPONENT_TARGET}")
    else()
        message(WARNING "nanopb component not found for ${COMPONENT_TARGET}")
    endif()
endfunction()

# ============================================================================
# Component Requirements Helper
# ============================================================================

# Helper to verify component requirements are met
# Usage: embedded_idf_check_requirements(<component_target> REQUIRES comp1 comp2 ...)
function(embedded_idf_check_requirements COMPONENT_TARGET)
    cmake_parse_arguments(ARG "" "" "REQUIRES" ${ARGN})

    if(NOT ARG_REQUIRES)
        return()
    endif()

    foreach(_required ${ARG_REQUIRES})
        if(NOT TARGET ${_required})
            message(WARNING "Required component '${_required}' not found for ${COMPONENT_TARGET}")
        endif()
    endforeach()
endfunction()

# Provides helper functions for configuring build targets

include(CheckIPOSupported)

# ============================================================================
# Warning Configuration
# ============================================================================

# Apply standard compiler warnings to a target
function(client_target_set_warnings TARGET)
    set(MSVC_WARNINGS
        /W4              # High warning level
        /w14242          # Conversion warnings
        /w14254          # Operator conversion
        /w14263          # Member function doesn't override
        /w14265          # Virtual destructor warning
        /w14287          # Unsigned/negative constant mismatch
        /we4289          # Loop variable used outside scope
        /w14296          # Expression is always true/false
        /w14311          # Pointer truncation
        /w14545 /w14546 /w14547 /w14549 /w14555 # Suspicious operations
        /w14619          # Unknown warning number
        /w14640          # Thread-unsafe static initialization
        /w14826          # Wide character conversion
        /w14905 /w14906  # String literal casts
        /w14928          # Illegal copy initialization
    )

    set(CLANG_WARNINGS
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
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    if(CLIENT_ENABLE_WARNINGS_AS_ERRORS)
        list(APPEND MSVC_WARNINGS /WX)
        list(APPEND CLANG_WARNINGS -Werror)
        list(APPEND GCC_WARNINGS -Werror)
    endif()

    target_compile_options(${TARGET} PRIVATE
        # MSVC and clang-cl (MSVC frontend) use MSVC-style warnings
        $<$<OR:$<CXX_COMPILER_ID:MSVC>,$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Windows>>>:${MSVC_WARNINGS}>
        # Clang on Unix-like systems
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<PLATFORM_ID:Windows>>>:${CLANG_WARNINGS}>
        # GCC
        $<$<CXX_COMPILER_ID:GNU>:${GCC_WARNINGS}>
    )
endfunction()

# ============================================================================
# Optimization Configuration
# ============================================================================

# Apply standard optimization flags to a target
function(client_target_set_optimization TARGET)
    target_compile_options(${TARGET} PRIVATE
        # MSVC and clang-cl (MSVC frontend)
        $<$<OR:$<CXX_COMPILER_ID:MSVC>,$<AND:$<CXX_COMPILER_ID:Clang>,$<PLATFORM_ID:Windows>>>:
            /Zc:preprocessor
            $<$<CONFIG:Debug>:/Od /Zi /RTC1>
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi /DNDEBUG>
            $<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>
        >
        # GCC and Clang on Unix-like systems
        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<NOT:$<PLATFORM_ID:Windows>>>:
            $<$<CONFIG:Debug>:-O0 -g3 -ggdb>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
            $<$<CONFIG:Release>:-O3 -DNDEBUG>
        >
    )
endfunction()

# Enable Link-Time Optimization (LTO) for a target
function(client_target_enable_lto TARGET)
    if(${CLIENT_ENABLE_LTO})
        return()
    endif()

    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_error LANGUAGES CXX)
    if(_ipo_supported)
        set_target_properties(${TARGET} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
        )
    else()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(STATUS "LTO not supported for target ${TARGET} (not needed in Debug)")
        else()
            message(WARNING "LTO not supported for target ${TARGET}: ${_ipo_error}")
        endif()
    endif()
endfunction()

# ============================================================================
# Platform Configuration
# ============================================================================

# Apply platform-specific compile definitions to a target
function(client_target_set_platform TARGET)
    target_compile_definitions(${TARGET} PUBLIC
        $<$<PLATFORM_ID:Windows>:CLIENT_PLATFORM_WINDOWS>
        $<$<PLATFORM_ID:Linux>:CLIENT_PLATFORM_LINUX>
        $<$<PLATFORM_ID:Darwin>:CLIENT_PLATFORM_MACOS>
        $<$<PLATFORM_ID:Android>:CLIENT_PLATFORM_ANDROID>
        $<$<CONFIG:Debug>:CLIENT_DEBUG_MODE>
        $<$<CONFIG:RelWithDebInfo>:CLIENT_RELEASE_WITH_DEBUG_INFO_MODE>
        $<$<CONFIG:Release>:CLIENT_RELEASE_MODE NDEBUG>
    )

    if(WIN32)
        target_compile_definitions(${TARGET} PRIVATE
            UNICODE _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()
endfunction()

# ============================================================================
# Output Directory Configuration
# ============================================================================

# Set standard output directories for a target
function(client_target_set_output_dirs TARGET)
    cmake_parse_arguments(ARG "" "CUSTOM_FOLDER" "" ${ARGN})

    if(NOT ARG_CUSTOM_FOLDER)
        set(ARG_CUSTOM_FOLDER "")
    endif()

    if(ARG_CUSTOM_FOLDER)
        set(_output_dir "${CLIENT_ROOT_DIR}/bin/${ARG_CUSTOM_FOLDER}/$<LOWER_CASE:$<CONFIG>>-$<LOWER_CASE:${CMAKE_SYSTEM_NAME}>-$<LOWER_CASE:${CMAKE_SYSTEM_PROCESSOR}>")
    else()
        set(_output_dir "${CLIENT_ROOT_DIR}/bin/$<LOWER_CASE:$<CONFIG>>-$<LOWER_CASE:${CMAKE_SYSTEM_NAME}>-$<LOWER_CASE:${CMAKE_SYSTEM_PROCESSOR}>")
    endif()

    set_target_properties(${TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_output_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${_output_dir}"
        ARCHIVE_OUTPUT_DIRECTORY "${_output_dir}"
    )
endfunction()

# ============================================================================
# C++ Standard Configuration
# ============================================================================

# Set C++ standard for a target
function(client_target_set_cxx_standard TARGET)
    cmake_parse_arguments(ARG "" "STANDARD" "" ${ARGN})

    if(NOT ARG_STANDARD)
        set(ARG_STANDARD 23)
    endif()

    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD ${ARG_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

# ============================================================================
# IDE Organization
# ============================================================================

# Set IDE folder for a target
function(client_target_set_folder TARGET FOLDER_NAME)
    set_target_properties(${TARGET} PROPERTIES FOLDER ${FOLDER_NAME})
endfunction()

# ============================================================================
# External Dependency Configuration
# ============================================================================

# Link to external target with SYSTEM includes to suppress warnings
function(client_target_link_system_libraries TARGET VISIBILITY)
    cmake_parse_arguments(ARG "" "" "" ${ARGN})

    foreach(_lib ${ARG_UNPARSED_ARGUMENTS})
        if(TARGET ${_lib})
            get_target_property(_lib_includes ${_lib} INTERFACE_INCLUDE_DIRECTORIES)

            if(_lib_includes AND NOT _lib_includes STREQUAL "_lib_includes-NOTFOUND")
                set_target_properties(${_lib} PROPERTIES
                    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_lib_includes}"
                )
            endif()

            target_link_libraries(${TARGET} ${VISIBILITY} ${_lib})
        else()
            target_link_libraries(${TARGET} ${VISIBILITY} ${_lib})
        endif()
    endforeach()
endfunction()

# ============================================================================
# Precompiled Headers
# ============================================================================

# Add precompiled header to a target
# Usage: client_target_add_pch(TARGET target_name PCH_FILE path/to/pch.hpp [PUBLIC|INTERFACE])
function(client_target_add_pch TARGET PCH_FILE)
    cmake_parse_arguments(ARG "PUBLIC;INTERFACE" "" "" ${ARGN})

    set(VISIBILITY PRIVATE)
    if(ARG_PUBLIC)
        set(VISIBILITY PUBLIC)
    elseif(ARG_INTERFACE)
        set(VISIBILITY INTERFACE)
    endif()

    target_precompile_headers(${TARGET} ${VISIBILITY}
        $<$<COMPILE_LANGUAGE:CXX>:${PCH_FILE}>
    )
endfunction()

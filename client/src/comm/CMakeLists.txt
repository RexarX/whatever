# Client Communication Library
# Shared library containing protocol handling and protobuf serialization.
# This library wraps protobuf to isolate it from OpenCV's internal protobuf.
#
# Key design decisions:
# - SHARED library to create ABI boundary between our protobuf and OpenCV's
# - All protobuf symbols are hidden (not exported)
# - Only clean C++ types are exposed in the public API

cmake_minimum_required(VERSION 3.25)

# Proto file generation
set(COMM_PROTO_DIR "${CMAKE_SOURCE_DIR}/../proto")
set(COMM_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Ensure generated directory exists
file(MAKE_DIRECTORY "${COMM_PROTO_GENERATED_DIR}")

# Proto files
set(COMM_PROTO_FILES
    "${COMM_PROTO_DIR}/messages.proto"
)

# Generated source files
set(COMM_PROTO_GENERATED_SOURCES
    "${COMM_PROTO_GENERATED_DIR}/messages.pb.cc"
)
set(COMM_PROTO_GENERATED_HEADERS
    "${COMM_PROTO_GENERATED_DIR}/messages.pb.h"
)

# Generate protobuf sources
if(Protobuf_PROTOC_EXECUTABLE OR TARGET protobuf::protoc)
    if(NOT Protobuf_PROTOC_EXECUTABLE)
        if(TARGET protobuf::protoc)
            get_target_property(Protobuf_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
        endif()
        if(NOT Protobuf_PROTOC_EXECUTABLE)
            find_program(Protobuf_PROTOC_EXECUTABLE NAMES protoc)
        endif()
    endif()

    if(Protobuf_PROTOC_EXECUTABLE)
        add_custom_command(
            OUTPUT ${COMM_PROTO_GENERATED_SOURCES} ${COMM_PROTO_GENERATED_HEADERS}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                "-I${COMM_PROTO_DIR}"
                "--cpp_out=${COMM_PROTO_GENERATED_DIR}"
                ${COMM_PROTO_FILES}
            DEPENDS ${COMM_PROTO_FILES} ${Protobuf_PROTOC_EXECUTABLE}
            COMMENT "Generating protobuf sources for comm library"
            VERBATIM
        )

        add_custom_target(comm_generate_proto
            DEPENDS ${COMM_PROTO_GENERATED_SOURCES} ${COMM_PROTO_GENERATED_HEADERS}
        )

        message(STATUS "Comm library protobuf generation configured")
    else()
        message(WARNING "protoc not found - comm proto files will not be compiled")
        set(COMM_PROTO_GENERATED_SOURCES "")
        set(COMM_PROTO_GENERATED_HEADERS "")
    endif()
else()
    message(WARNING "Protobuf not configured - comm proto files will not be compiled")
    set(COMM_PROTO_GENERATED_SOURCES "")
    set(COMM_PROTO_GENERATED_HEADERS "")
endif()

# Comm library sources
set(CLIENT_COMM_SOURCES
    src/protocol.cpp
    src/bluetooth.cpp
    src/pch.cpp
    ${COMM_PROTO_GENERATED_SOURCES}
)

# Comm library headers (public API only - no protobuf headers exposed)
set(CLIENT_COMM_HEADERS
    include/client/comm/export.hpp
    include/client/comm/protocol.hpp
    include/client/comm/bluetooth.hpp
    include/client/comm/pch.hpp
)

# Create shared library for ABI isolation
add_library(client_comm SHARED
    ${CLIENT_COMM_SOURCES}
    ${CLIENT_COMM_HEADERS}
)
add_library(client::comm ALIAS client_comm)

# Ensure proto files are generated before building
if(TARGET comm_generate_proto)
    add_dependencies(client_comm comm_generate_proto)
endif()

# Set export macro for building the shared library
target_compile_definitions(client_comm PRIVATE
    CLIENT_COMM_BUILDING_SHARED
)

# Check for Qt Bluetooth availability and link if available
if(TARGET client::qt6::Bluetooth)
    message(STATUS "  ✓ client::qt6::Bluetooth found - Bluetooth support enabled")
    target_link_libraries(client_comm PRIVATE client::qt6::Bluetooth)
    target_compile_definitions(client_comm PRIVATE CLIENT_COMM_HAS_BLUETOOTH)
    set_target_properties(client_comm PROPERTIES AUTOMOC ON)
else()
    message(STATUS "  - client::qt6::Bluetooth not found - Bluetooth support disabled (stub implementation)")
endif()

# Public include directories (only exposes clean C++ API)
target_include_directories(client_comm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Generated protobuf headers are PRIVATE - not exposed to consumers
        ${COMM_PROTO_GENERATED_DIR}
        $<TARGET_PROPERTY:client::protobuf,INTERFACE_INCLUDE_DIRECTORIES>
)

# Link dependencies
# - core is linked PUBLIC (API uses core types)
# - protobuf is linked PRIVATE (implementation detail, not exposed)
target_link_libraries(client_comm
    PUBLIC
        client::core
    PRIVATE
        client::protobuf
)

# C++ standard and properties
set_target_properties(client_comm PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Symbol visibility: hide all symbols by default, only export marked ones
# This is CRITICAL for protobuf isolation - we don't want protobuf symbols leaking
set_target_properties(client_comm PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    C_VISIBILITY_PRESET hidden
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(client_comm PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Suppress warnings from generated protobuf code
if(COMM_PROTO_GENERATED_SOURCES)
    set_source_files_properties(${COMM_PROTO_GENERATED_SOURCES}
        PROPERTIES
            COMPILE_FLAGS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>"
            # Exclude from unity builds to avoid ODR violations
            SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

# Apply standard configurations if available
if(COMMAND client_target_set_warnings)
    client_target_set_warnings(client_comm)
endif()

if(COMMAND client_target_set_optimization)
    client_target_set_optimization(client_comm)
endif()

if(COMMAND client_target_set_output_dirs)
    client_target_set_output_dirs(client_comm)
endif()

# Add precompiled header
if(COMMAND client_target_add_pch)
    client_target_add_pch(client_comm ${CMAKE_CURRENT_SOURCE_DIR}/include/client/comm/pch.hpp PUBLIC)
endif()

# Set IDE folder
if(COMMAND client_target_set_folder)
    client_target_set_folder(client_comm "Libraries")
endif()

client_target_set_platform(client_comm)

# Export the consumer interface definition
target_compile_definitions(client_comm INTERFACE
    CLIENT_COMM_USING_SHARED
)

# Android-specific: ensure the shared library is packaged in the APK
if(ANDROID)
    set_target_properties(client_comm PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    )
endif()

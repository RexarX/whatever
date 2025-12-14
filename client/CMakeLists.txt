cmake_minimum_required(VERSION 3.25)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
    message(STATUS "Setting build type to 'Release' as none was specified.")
endif()

# Early Conan toolchain detection (before project())
if(DEFINED CACHE{CLIENT_USE_CONAN})
    message(STATUS "CLIENT_USE_CONAN explicitly set to: ${CLIENT_USE_CONAN}")
elseif(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" _platform_lower)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_type_lower)

    set(_CONAN_TOOLCHAIN "${CMAKE_SOURCE_DIR}/build/${_build_type_lower}/${_platform_lower}/conan_toolchain.cmake")

    if(EXISTS "${_CONAN_TOOLCHAIN}")
        set(CMAKE_TOOLCHAIN_FILE "${_CONAN_TOOLCHAIN}" CACHE FILEPATH "Conan toolchain file" FORCE)
        message(STATUS "Auto-detected Conan toolchain: ${_CONAN_TOOLCHAIN}")
        set(CLIENT_USE_CONAN TRUE CACHE BOOL "Using Conan for dependencies" FORCE)
    else()
        set(CLIENT_USE_CONAN FALSE CACHE BOOL "Using Conan for dependencies" FORCE)
    endif()
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "conan_toolchain.cmake")
    if(EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        message(STATUS "Using Conan toolchain: ${CMAKE_TOOLCHAIN_FILE}")
        if(NOT DEFINED CACHE{CLIENT_USE_CONAN})
            set(CLIENT_USE_CONAN TRUE CACHE BOOL "Using Conan for dependencies" FORCE)
        endif()
    else()
        message(WARNING "CMAKE_TOOLCHAIN_FILE is set to Conan toolchain but file doesn't exist: ${CMAKE_TOOLCHAIN_FILE}")
        set(CLIENT_USE_CONAN FALSE CACHE BOOL "Using Conan for dependencies" FORCE)
    endif()
elseif(NOT DEFINED CACHE{CLIENT_USE_CONAN})
    set(CLIENT_USE_CONAN FALSE CACHE BOOL "Using Conan for dependencies" FORCE)
endif()

# Ninja generator doesn't support CMAKE_GENERATOR_PLATFORM
if(CMAKE_GENERATOR MATCHES "Ninja")
    unset(CMAKE_GENERATOR_PLATFORM CACHE)
endif()

project(client_app
    VERSION 0.1.0
    DESCRIPTION "Face tracking client application"
    HOMEPAGE_URL "https://github.com/RexarX/whatever"
    LANGUAGES CXX
)

# Set archiver tools for Clang to support LTO
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        find_program(LLVM_LIB_EXECUTABLE
            NAMES llvm-lib llvm-lib.exe
            HINTS
                "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin"
                "$ENV{VCINSTALLDIR}/Tools/Llvm/bin"
                "C:/Program Files/LLVM/bin"
                "C:/Program Files (x86)/LLVM/bin"
        )
        find_program(MSVC_LIB_EXECUTABLE NAMES lib lib.exe)

        if(LLVM_LIB_EXECUTABLE)
            set(CMAKE_AR "${LLVM_LIB_EXECUTABLE}" CACHE FILEPATH "Archiver" FORCE)
            message(STATUS "Using LLVM archiver for clang-cl: ${LLVM_LIB_EXECUTABLE}")
        elseif(MSVC_LIB_EXECUTABLE)
            set(CMAKE_AR "${MSVC_LIB_EXECUTABLE}" CACHE FILEPATH "Archiver" FORCE)
            message(STATUS "Using MSVC archiver for clang-cl: ${MSVC_LIB_EXECUTABLE}")
        endif()
        set(CMAKE_RANLIB "" CACHE FILEPATH "Ranlib" FORCE)
    else()
        find_program(LLVM_AR_EXECUTABLE llvm-ar)
        find_program(LLVM_RANLIB_EXECUTABLE llvm-ranlib)
        if(LLVM_AR_EXECUTABLE)
            set(CMAKE_AR "${LLVM_AR_EXECUTABLE}" CACHE FILEPATH "Archiver" FORCE)
        endif()
        if(LLVM_RANLIB_EXECUTABLE)
            set(CMAKE_RANLIB "${LLVM_RANLIB_EXECUTABLE}" CACHE FILEPATH "Ranlib" FORCE)
        endif()
    endif()
endif()

# Only generate specific configs for multi-config generators
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)
endif()

# Modern CMake best practices
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt specific settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add third_party directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMakeLists.txt")
    add_subdirectory(third_party)
else()
    message(WARNING "Client third_party directory not found")
endif()

# Include utilities
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/helpers")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(BuildDirHelper)

client_print_build_dir_info()

# Print Conan status
message(STATUS "")
message(STATUS "========================================")
if(CLIENT_USE_CONAN)
    message(STATUS "Conan dependency management: ENABLED")
    client_get_conan_toolchain_path(_CONAN_TOOLCHAIN)
    if(EXISTS "${_CONAN_TOOLCHAIN}")
        message(STATUS "  Conan toolchain: ${_CONAN_TOOLCHAIN}")
    else()
        message(WARNING "Conan enabled but toolchain file not found!")
        set(CLIENT_USE_CONAN FALSE CACHE BOOL "Using Conan for dependencies" FORCE)
    endif()
else()
    message(STATUS "Conan dependency management: DISABLED")
endif()
message(STATUS "========================================")
message(STATUS "")

# Build options
option(CLIENT_FORCE_CONAN "Force Conan packages first, use system only as fallback" OFF)
option(CLIENT_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(CLIENT_ENABLE_LTO "Enable Link Time Optimization" ON)
option(CLIENT_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CLIENT_ALLOW_CPM_DOWNLOADS "Allow automatic download of missing dependencies via CPM" ON)

# Disable tests for Android builds
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(CLIENT_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
endif()

# LTO configuration
if(CLIENT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT CLIENT_IPO_SUPPORTED OUTPUT error)
    if(CLIENT_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "Link-Time Optimization (LTO) enabled for Release and RelWithDebInfo builds")
    else()
        message(STATUS "IPO/LTO not supported: ${error}")
        set(CLIENT_ENABLE_LTO OFF)
    endif()
endif()

# Set root directory for modules to reference
set(CLIENT_ROOT_DIR ${PROJECT_SOURCE_DIR})

# Include target utilities
include(cmake/TargetUtils.cmake)

set(CLIENT_DOWNLOAD_PACKAGES ${CLIENT_ALLOW_CPM_DOWNLOADS} CACHE BOOL "Download missing packages using CPM" FORCE)

# Dependency management
include(cmake/Dependencies.cmake)

# Qt standard project setup
if(QT_FOUND AND COMMAND qt_standard_project_setup)
    qt_standard_project_setup()

    if(COMMAND qt_policy)
        qt_policy(SET QTP0001 NEW)
        qt_policy(SET QTP0004 NEW)
    endif()
endif()

# ============================================================================
# Subdirectory Libraries
# ============================================================================

# Add core library (static, no Qt/OpenCV/protobuf dependencies)
add_subdirectory(src/core)

# Add comm library (shared, uses protobuf, isolated from OpenCV)
add_subdirectory(src/comm)

# Add runtime library (static, uses OpenCV/Qt)
add_subdirectory(src/runtime)

# ============================================================================
# Main Application Executable
# ============================================================================

set(CLIENT_APP_SOURCES
    src/main.cpp
)

# Create the executable
if(COMMAND qt_add_executable)
    qt_add_executable(${PROJECT_NAME}
        ${CLIENT_APP_SOURCES}
    )

    # Add QML module with Qt6's modern QML integration
    # This properly embeds QML files into the executable
    if(COMMAND qt_add_qml_module)
        qt_add_qml_module(${PROJECT_NAME}
            URI client
            VERSION 1.0
            QML_FILES
                qt/qml/Main.qml
            RESOURCE_PREFIX /
            NO_RESOURCE_TARGET_PATH
            OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml
        )
        # Define that we're using qt_add_qml_module
        target_compile_definitions(${PROJECT_NAME} PRIVATE CLIENT_QML_MODULE_ENABLED)
    else()
        # Fallback: use .qrc file directly
        target_sources(${PROJECT_NAME} PRIVATE qt/qml/qml.qrc)
        target_compile_definitions(${PROJECT_NAME} PRIVATE CLIENT_QML_QRC_FALLBACK)
    endif()

    # Android: add model resources
    if(ANDROID AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qt/resources/models.qrc")
        target_sources(${PROJECT_NAME} PRIVATE qt/resources/models.qrc)
    endif()
else()
    # Non-Qt fallback
    add_executable(${PROJECT_NAME}
        ${CLIENT_APP_SOURCES}
        qt/qml/qml.qrc
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE CLIENT_QML_QRC_FALLBACK)

    if(ANDROID AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qt/resources/models.qrc")
        target_sources(${PROJECT_NAME} PRIVATE qt/resources/models.qrc)
    endif()
endif()

# Apply standard configurations
if(COMMAND client_target_set_warnings)
    client_target_set_warnings(${PROJECT_NAME})
endif()
if(COMMAND client_target_set_optimization)
    client_target_set_optimization(${PROJECT_NAME})
endif()
if(COMMAND client_target_set_platform)
    client_target_set_platform(${PROJECT_NAME})
endif()
if(COMMAND client_target_set_output_dirs)
    client_target_set_output_dirs(${PROJECT_NAME})
endif()
if(COMMAND client_target_set_cxx_standard)
    client_target_set_cxx_standard(${PROJECT_NAME} STANDARD 23)
endif()

# Link to runtime library (which brings in core, comm, opencv, qt)
target_link_libraries(${PROJECT_NAME} PRIVATE
    client::runtime
)

# Enable LTO if requested
if(CLIENT_ENABLE_LTO AND COMMAND client_target_enable_lto)
    client_target_enable_lto(${PROJECT_NAME})
endif()

# Set IDE folder
if(COMMAND client_target_set_folder)
    client_target_set_folder(${PROJECT_NAME} "Application")
endif()

# ============================================================================
# Qt Deployment
# ============================================================================

include(cmake/QtDeploy.cmake)

if(QT_FOUND)
    if(WIN32)
        qt_deploy(${PROJECT_NAME})
    elseif(ANDROID)
        message(STATUS "Android: Configuring Qt6 built-in Android deployment")

        # ============================================================================
        # Qt6 Android Deployment - Proper Method
        # ============================================================================
        #
        # Qt6 provides built-in Android deployment via qt_finalize_target() which:
        # 1. Automatically collects Qt plugin dependencies
        # 2. Generates androiddeployqt JSON settings
        # 3. Stages all required Qt libraries and plugins
        # 4. Handles QML module dependencies
        # 5. Creates proper Gradle build structure
        #
        # We use QT_ANDROID_EXTRA_LIBS to add non-Qt libraries (OpenCV, etc.)
        #

        # Collect extra libraries that need to be bundled in the APK
        set(_android_extra_libs "")

        # Add OpenCV shared library
        if(EXISTS "$ENV{OPENCV_ANDROID_SDK}/sdk/native/libs/${ANDROID_ABI}/libopencv_java4.so")
            list(APPEND _android_extra_libs "$ENV{OPENCV_ANDROID_SDK}/sdk/native/libs/${ANDROID_ABI}/libopencv_java4.so")
            message(STATUS "  ✓ Added libopencv_java4.so to Android extra libs")
        else()
            message(WARNING "  ⚠ OpenCV Android library not found at expected path")
        endif()

        # Add C++ shared library (required by NDK)
        if(CMAKE_ANDROID_NDK AND EXISTS "${CMAKE_ANDROID_NDK}/sources/cxx-stl/llvm-libc++/libs/${ANDROID_ABI}/libc++_shared.so")
            list(APPEND _android_extra_libs "${CMAKE_ANDROID_NDK}/sources/cxx-stl/llvm-libc++/libs/${ANDROID_ABI}/libc++_shared.so")
            message(STATUS "  ✓ Added libc++_shared.so to Android extra libs")
        endif()

        # Set Android-specific properties on the target
        set_target_properties(${PROJECT_NAME} PROPERTIES
            QT_ANDROID_EXTRA_LIBS "${_android_extra_libs}"
            QT_ANDROID_MIN_SDK_VERSION 28
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_SDK_BUILD_TOOLS_REVISION "35.0.0"
            QT_ANDROID_NO_DEPLOY_QT_LIBS OFF
            QT_ANDROID_ABIS "${ANDROID_ABI}"
        )

        # Create Android package directory with AndroidManifest.xml for permissions
        # Pre-create android-build directory structure to prevent copy failures
        # This is especially important on NTFS filesystems
        set(_android_build_dir "${CMAKE_CURRENT_BINARY_DIR}/android-build")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_android_build_dir}/libs/${ANDROID_ABI}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_android_build_dir}/assets"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_android_build_dir}/res"
            # Copy main application binary to android-build/libs where androiddeployqt expects it
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${PROJECT_NAME}>"
                "${_android_build_dir}/libs/${ANDROID_ABI}/"
            # Copy client_comm shared library to android-build/libs
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:client_comm>"
                "${_android_build_dir}/libs/${ANDROID_ABI}/"
            COMMENT "Copying application and dependencies to android-build/libs"
            VERBATIM
        )

        # Finalize the target - this triggers Qt's Android deployment system
        # It will:
        # - Generate android-<target>-deployment-settings.json
        # - Set up androiddeployqt to run automatically
        # - Collect all Qt dependencies (including ffmpeg plugins for Multimedia)
        # - Stage everything in android-build/libs/${ANDROID_ABI}
        # - Build the APK using Gradle
        if(COMMAND qt_finalize_target)
            qt_finalize_target(${PROJECT_NAME})
            message(STATUS "  ✓ Qt Android deployment finalized for ${PROJECT_NAME}")
        else()
            message(WARNING "  ⚠ qt_finalize_target not available - deployment may be incomplete")
        endif()

        message(STATUS "Android deployment configured:")
        message(STATUS "  - Qt libraries and plugins: handled by qt_finalize_target")
        message(STATUS "  - Extra libraries: ${_android_extra_libs}")
        message(STATUS "  - APK will be built automatically during build step")
    endif()
endif()

# Copy models directory to output directories (non-Android only)
set(MODELS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/models")
if(EXISTS "${MODELS_SOURCE_DIR}" AND NOT ANDROID)
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME_LOWER)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR_LOWER)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MODELS_SOURCE_DIR}"
        "${CLIENT_ROOT_DIR}/bin/$<LOWER_CASE:$<CONFIG>>-${SYSTEM_NAME_LOWER}-${SYSTEM_PROCESSOR_LOWER}/models"
        COMMENT "Copying models to output directory"
        VERBATIM
    )
endif()

# ============================================================================
# Testing
# ============================================================================

if(CLIENT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Configuration Summary
# ============================================================================

message(STATUS "")
message(STATUS "========== Configuration Summary ==========")
message(STATUS "  Project:        ${PROJECT_NAME}")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  LTO Enabled:    ${CLIENT_ENABLE_LTO}")
message(STATUS "  Build Tests:    ${CLIENT_BUILD_TESTS}")
message(STATUS "  Libraries:")
message(STATUS "    - client_core (static)")
message(STATUS "    - client_comm (shared, protobuf isolated)")
message(STATUS "    - client_runtime (static, OpenCV/Qt)")
message(STATUS "============================================")
message(STATUS "")

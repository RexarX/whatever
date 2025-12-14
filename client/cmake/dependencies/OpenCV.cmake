# This module handles finding OpenCV from multiple sources:
# 1. Conan (if using Conan)
# 2. System packages (apt, pacman, brew, etc.)
# 3. CPM download (fallback - note: building OpenCV from source is slow)
#
# The module exposes:
# - client::opencv4 (umbrella target linking to all modules)
# - client::opencv4::core, client::opencv4::dnn, etc. (individual module targets)
#
# Android note:
# OpenCV Android SDK bundles protobuf as a 3rdparty library and its CMake config
# tries to import a libprotobuf target. Since we build protobuf via CPM first
# (to ensure headers are available), this causes a target conflict.
# Solution: On Android, bypass OpenCV's CMake config and manually create targets
# by directly linking to the prebuilt OpenCV libraries.

include_guard(GLOBAL)

message(STATUS "Configuring OpenCV dependency...")

# ============================================================================
# Android: Manual OpenCV import to avoid protobuf conflict
# ============================================================================

if(ANDROID)
    message(STATUS "  Android build: manually importing OpenCV to avoid protobuf conflict")

    # Set OpenCV paths from environment
    if(NOT DEFINED ENV{OPENCV_ANDROID_SDK})
        message(FATAL_ERROR "OPENCV_ANDROID_SDK environment variable not set")
    endif()

    set(OpenCV_INCLUDE_DIR "$ENV{OPENCV_ANDROID_SDK}/sdk/native/jni/include")
    set(OpenCV_LIB_DIR "$ENV{OPENCV_ANDROID_SDK}/sdk/native/libs/arm64-v8a")
    set(OpenCV_JAVA_LIB "${OpenCV_LIB_DIR}/libopencv_java4.so")

    if(NOT EXISTS "${OpenCV_INCLUDE_DIR}")
        message(FATAL_ERROR "OpenCV include directory not found: ${OpenCV_INCLUDE_DIR}")
    endif()

    if(NOT EXISTS "${OpenCV_JAVA_LIB}")
        message(FATAL_ERROR "OpenCV library not found: ${OpenCV_JAVA_LIB}")
    endif()

    # OpenCV Android SDK uses a single shared library (libopencv_java4.so) containing all modules
    # Create individual module targets as INTERFACE libraries that link to this shared library

    set(_opencv_modules core flann imgproc ml photo dnn features2d imgcodecs videoio calib3d highgui objdetect stitching video gapi)

    foreach(_module IN LISTS _opencv_modules)
        set(_lib_name "opencv_${_module}")

        # Create INTERFACE library that links to the shared OpenCV library
        add_library(${_lib_name} INTERFACE IMPORTED)
        set_target_properties(${_lib_name} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${OpenCV_JAVA_LIB}"
        )

        message(STATUS "  ✓ Created target: ${_lib_name}")
    endforeach()

    # Add system libraries needed by OpenCV on Android
    # Link these to opencv_core since other modules depend on it
    if(TARGET opencv_core)
        set_property(TARGET opencv_core APPEND PROPERTY INTERFACE_LINK_LIBRARIES
            z
            log
            dl
            m
        )
    endif()

    set(OpenCV_FOUND TRUE)
    set(OpenCV_VERSION "4.12.0")
    message(STATUS "  ✓ OpenCV found via manual import (Android SDK)")

else()
    # ============================================================================
    # Non-Android: Use normal dependency resolution
    # ============================================================================

    # OpenCV is complex to build from source, so we strongly prefer system/Conan packages
    # CPM download is provided as a last resort but will take a long time to build

    client_dep_begin(
        NAME OpenCV
        VERSION 4.8
        DEBIAN_NAMES libopencv-dev
        RPM_NAMES opencv-devel
        PACMAN_NAMES opencv
        BREW_NAMES opencv
        PKG_CONFIG_NAMES opencv4 opencv
        CPM_NAME opencv
        CPM_VERSION 4.12.0
        CPM_GITHUB_REPOSITORY opencv/opencv
        CPM_OPTIONS
            "BUILD_LIST=core,imgproc,objdetect,video,videoio,highgui,dnn"
            "BUILD_EXAMPLES=OFF"
            "BUILD_TESTS=OFF"
            "BUILD_PERF_TESTS=OFF"
            "BUILD_DOCS=OFF"
            "BUILD_opencv_apps=OFF"
            "BUILD_opencv_python3=OFF"
            "BUILD_opencv_python2=OFF"
            "WITH_FFMPEG=ON"
            "WITH_CUDA=OFF"
            "WITH_OPENCL=ON"
            "OPENCV_GENERATE_PKGCONFIG=OFF"
            "BUILD_PROTOBUF=OFF"
    )

    client_dep_end()
endif()

# ============================================================================
# Create individual component targets (client::opencv4::MODULE)
# ============================================================================

# List of OpenCV modules we support
set(_OPENCV_MODULES core imgproc objdetect video videoio highgui dnn)
set(_OPENCV_AVAILABLE_MODULES "")

# Create individual module targets and track available ones
foreach(_opencv_module IN LISTS _OPENCV_MODULES)
    if(TARGET opencv_${_opencv_module})
        list(APPEND _OPENCV_AVAILABLE_MODULES ${_opencv_module})

        # Create an interface library as an alias/wrapper around the actual OpenCV target
        if(NOT TARGET client::opencv4::${_opencv_module})
            add_library(client::opencv4::${_opencv_module} INTERFACE IMPORTED)
            target_link_libraries(client::opencv4::${_opencv_module} INTERFACE opencv_${_opencv_module})
            message(STATUS "  ✓ client::opencv4::${_opencv_module} created")
        endif()
    endif()
endforeach()

if(_OPENCV_AVAILABLE_MODULES)
    list(JOIN _OPENCV_AVAILABLE_MODULES ", " _modules_str)
    message(STATUS "  Available OpenCV modules: ${_modules_str}")
endif()

# ============================================================================
# Create umbrella target (client::opencv4) linking to ALL modules
# ============================================================================

if(NOT TARGET client::opencv4)
    add_library(opencv4 INTERFACE)
    add_library(client::opencv4 ALIAS opencv4)

    # Collect all OpenCV targets we need
    set(_opencv_targets "")

    # Add all available modules to the umbrella target
    foreach(_opencv_module IN LISTS _OPENCV_AVAILABLE_MODULES)
        if(TARGET opencv_${_opencv_module})
            list(APPEND _opencv_targets opencv_${_opencv_module})
        endif()
    endforeach()

    if(_opencv_targets)
        target_link_libraries(opencv4 INTERFACE ${_opencv_targets})
        message(STATUS "  ✓ client::opencv4 (umbrella) configured with all available modules")
    elseif(DEFINED OpenCV_LIBRARIES)
        # Fallback to OpenCV_LIBRARIES from find_package
        target_link_libraries(opencv4 INTERFACE ${OpenCV_LIBRARIES})
        message(STATUS "  ✓ client::opencv4 (umbrella) configured with libraries: ${OpenCV_LIBRARIES}")
    else()
        message(WARNING "  ✗ OpenCV libraries not properly configured")
    endif()

    # Propagate include directories if available
    if(DEFINED OpenCV_INCLUDE_DIRS)
        target_include_directories(opencv4 INTERFACE ${OpenCV_INCLUDE_DIRS})
    elseif(DEFINED OpenCV_INCLUDE_DIR)
        target_include_directories(opencv4 INTERFACE ${OpenCV_INCLUDE_DIR})
    endif()
endif()

# ============================================================================
# Clean up temporary variables
# ============================================================================

unset(_OPENCV_MODULES)
unset(_OPENCV_AVAILABLE_MODULES)
unset(_opencv_module)
unset(_opencv_targets)
unset(_modules_str)
unset(_opencv_modules)
unset(_opencv_3rdparty_libs)

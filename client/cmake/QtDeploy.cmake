# QtDeploy.cmake
# Cross-platform Qt deployment helper for copying Qt libraries and plugins
# Supports Windows (windeployqt), macOS (macdeployqt), and Linux (manual copy)

include_guard(GLOBAL)

# Find Qt deployment tools
function(_find_qt_deploy_tool)
    if(NOT QT_FOUND)
        message(WARNING "Qt not found, cannot configure deployment")
        return()
    endif()

    # Get qmake executable path to find deployment tools
    if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
        get_target_property(_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    elseif(TARGET Qt::qmake)
        get_target_property(_qmake_executable Qt::qmake IMPORTED_LOCATION)
    else()
        find_program(_qmake_executable NAMES qmake qmake6 qmake-qt6)
    endif()

    if(_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
        set(QT_BIN_DIR "${_qt_bin_dir}" PARENT_SCOPE)

        # Find platform-specific deployment tool
        if(WIN32)
            find_program(QT_DEPLOY_TOOL
                NAMES windeployqt windeployqt.exe
                HINTS "${_qt_bin_dir}"
                NO_DEFAULT_PATH
            )
        elseif(APPLE)
            find_program(QT_DEPLOY_TOOL
                NAMES macdeployqt
                HINTS "${_qt_bin_dir}"
                NO_DEFAULT_PATH
            )
        endif()

        if(QT_DEPLOY_TOOL)
            set(QT_DEPLOY_TOOL "${QT_DEPLOY_TOOL}" PARENT_SCOPE)
            message(STATUS "Found Qt deployment tool: ${QT_DEPLOY_TOOL}")
        else()
            if(WIN32)
                message(WARNING "windeployqt not found in ${_qt_bin_dir}")
            elseif(APPLE)
                message(WARNING "macdeployqt not found in ${_qt_bin_dir}")
            endif()
        endif()
    else()
        message(WARNING "qmake not found, cannot locate Qt deployment tools")
    endif()
endfunction()

# Deploy Qt libraries and plugins for a target (Windows using windeployqt)
function(_deploy_qt_windows target)
    if(NOT QT_DEPLOY_TOOL)
        message(WARNING "windeployqt not available, skipping Qt deployment for ${target}")
        return()
    endif()

    # Determine build configuration
    set(_config_arg)
    if(CMAKE_BUILD_TYPE MATCHES "^[Dd][Ee][Bb][Uu][Gg]")
        set(_config_arg --debug)
    else()
        set(_config_arg --release)
    endif()

    # Common windeployqt arguments
    set(_deploy_args
        ${_config_arg}
        --verbose 1
        --no-compiler-runtime
        --no-system-d3d-compiler
        --no-translations
    )

    # Add QML support if using Qt Quick
    if(TARGET Qt${QT_VERSION_MAJOR}::Quick OR TARGET Qt::Quick)
        list(APPEND _deploy_args --qmldir "${CMAKE_SOURCE_DIR}/qt/qml")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${QT_DEPLOY_TOOL}"
            ${_deploy_args}
            "$<TARGET_FILE:${target}>"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Deploying Qt libraries for ${target} using windeployqt..."
        VERBATIM
    )

    message(STATUS "Configured Qt deployment for ${target} (Windows)")
endfunction()

# Deploy Qt libraries and plugins for a target (macOS using macdeployqt)
function(_deploy_qt_macos target)
    if(NOT QT_DEPLOY_TOOL)
        message(WARNING "macdeployqt not available, skipping Qt deployment for ${target}")
        return()
    endif()

    # macdeployqt works on .app bundles
    # Ensure target creates a bundle
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE TRUE
    )

    set(_deploy_args)

    # Add QML support if using Qt Quick
    if(TARGET Qt${QT_VERSION_MAJOR}::Quick OR TARGET Qt::Quick)
        list(APPEND _deploy_args -qmldir="${CMAKE_SOURCE_DIR}/qt/qml")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${QT_DEPLOY_TOOL}"
            "$<TARGET_BUNDLE_DIR:${target}>"
            ${_deploy_args}
        COMMENT "Deploying Qt libraries for ${target} using macdeployqt..."
        VERBATIM
    )

    message(STATUS "Configured Qt deployment for ${target} (macOS)")
endfunction()

# Deploy Qt libraries for Linux (manual copy of shared libraries)
function(_deploy_qt_linux target)
    # On Linux, we typically rely on system Qt or use linuxdeployqt (external tool)
    # Here we provide a basic implementation that copies Qt libraries next to the executable

    if(NOT TARGET Qt${QT_VERSION_MAJOR}::Core AND NOT TARGET Qt::Core)
        message(WARNING "Qt::Core not found, cannot deploy Qt libraries")
        return()
    endif()

    # Get Qt library directory
    if(TARGET Qt${QT_VERSION_MAJOR}::Core)
        get_target_property(_qt_core_location Qt${QT_VERSION_MAJOR}::Core LOCATION)
    else()
        get_target_property(_qt_core_location Qt::Core LOCATION)
    endif()

    if(_qt_core_location)
        get_filename_component(_qt_lib_dir "${_qt_core_location}" DIRECTORY)

        message(STATUS "Qt library directory: ${_qt_lib_dir}")

        # Copy Qt libraries to output directory
        # This is a simplified version - for production, consider using linuxdeployqt or AppImage
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/qt_libs"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_qt_lib_dir}"
                "$<TARGET_FILE_DIR:${target}>/qt_libs"
            COMMENT "Copying Qt libraries for ${target} (Linux - basic deployment)..."
            VERBATIM
        )

        # Create a wrapper script that sets LD_LIBRARY_PATH
        set(_wrapper_script "${CMAKE_CURRENT_BINARY_DIR}/${target}_qt.sh")
        file(GENERATE OUTPUT "${_wrapper_script}"
            CONTENT "#!/bin/bash
SCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"
export LD_LIBRARY_PATH=\"\${SCRIPT_DIR}/qt_libs:\${LD_LIBRARY_PATH}\"
exec \"\${SCRIPT_DIR}/$<TARGET_FILE_NAME:${target}>\" \"$@\"
"
        )

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${_wrapper_script}" "$<TARGET_FILE_DIR:${target}>/${target}_qt.sh"
            COMMAND chmod +x "$<TARGET_FILE_DIR:${target}>/${target}_qt.sh"
            COMMENT "Creating Qt launcher script for ${target}..."
            VERBATIM
        )

        message(STATUS "Configured basic Qt deployment for ${target} (Linux)")
        message(STATUS "  Note: For production deployment, consider using linuxdeployqt or AppImage")
    else()
        message(WARNING "Could not determine Qt library location for Linux deployment")
    endif()
endfunction()

# Main function: Deploy Qt for a target (cross-platform)
function(deploy_qt_for_target target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Target '${target}' does not exist")
    endif()

    if(NOT QT_FOUND)
        message(WARNING "Qt not found, cannot deploy for ${target}")
        return()
    endif()

    # Find deployment tools
    _find_qt_deploy_tool()

    # Platform-specific deployment
    if(WIN32)
        _deploy_qt_windows(${target})
    elseif(APPLE)
        _deploy_qt_macos(${target})
    elseif(UNIX)
        _deploy_qt_linux(${target})
    else()
        message(WARNING "Unsupported platform for Qt deployment: ${CMAKE_SYSTEM_NAME}")
    endif()
endfunction()

# Convenience function with shorter name
function(qt_deploy target)
    deploy_qt_for_target(${target})
endfunction()

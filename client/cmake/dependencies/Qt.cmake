# This module handles finding Qt from multiple sources:
# 1. Conan (if using Conan)
# 2. System packages
# 3. Manual Qt installation
#
# The module exposes:
# - client::qt6 (umbrella target linking to all components)
# - client::qt6::core, client::qt6::gui, client::qt6::widgets, etc. (individual component targets)
#
# Example usage:
#   target_link_libraries(myapp client::qt6)           # All components
#   target_link_libraries(myapp client::qt6::core)     # Just Core
#   target_link_libraries(myapp client::qt6::gui)      # Just Gui
#   target_link_libraries(myapp client::qt6::qml)      # Just Qml

include_guard(GLOBAL)

message(STATUS "Configuring Qt dependency...")

# Qt is typically not available via CPM due to its size and complexity
# We rely on system installation or Conan

# Try to find Qt6 first, then Qt5 as fallback
set(QT_FOUND FALSE)

# Define Qt components we need
# Required components
set(QT_REQUIRED_COMPONENTS
    Core
    Gui
    Widgets
    Network
    Quick
    Qml
    QuickControls2
)

# Optional components (will be used if available)
set(QT_OPTIONAL_COMPONENTS
    Multimedia
    SerialPort
    Bluetooth
)

# All components combined for initial search
set(QT_ALL_COMPONENTS ${QT_REQUIRED_COMPONENTS} ${QT_OPTIONAL_COMPONENTS})

# Qt from Conan is intentionally disabled; prefer system/installed Qt
if(CLIENT_USE_CONAN)
    message(STATUS "  Conan is enabled, but Qt must be provided by system/installer (Linux) or Qt SDK (Windows). Skipping Conan Qt lookup.")
endif()

# ============================================================================
# Try to find Qt6 (preferred)
# ============================================================================

if(NOT QT_FOUND)
    find_package(Qt6 COMPONENTS ${QT_REQUIRED_COMPONENTS} CONFIG QUIET)
    if(Qt6_FOUND)
        set(QT_FOUND TRUE)
        set(QT_VERSION_MAJOR 6)
        set(QT_VERSION ${Qt6_VERSION})
        message(STATUS "  ✓ Qt6 ${Qt6_VERSION} found (system)")

        # Try to find optional components
        foreach(_opt_component ${QT_OPTIONAL_COMPONENTS})
            find_package(Qt6 COMPONENTS ${_opt_component} CONFIG QUIET)
            if(Qt6${_opt_component}_FOUND)
                message(STATUS "  ✓ Qt6::${_opt_component} found (optional)")
                list(APPEND QT_REQUIRED_COMPONENTS ${_opt_component})
            else()
                message(STATUS "  - Qt6::${_opt_component} not found (optional, skipped)")
            endif()
        endforeach()
    endif()
endif()

# ============================================================================
# Fallback to Qt5 (not recommended, but supported)
# ============================================================================

if(NOT QT_FOUND)
    find_package(Qt5 COMPONENTS ${QT_REQUIRED_COMPONENTS} CONFIG QUIET)
    if(Qt5_FOUND)
        set(QT_FOUND TRUE)
        set(QT_VERSION_MAJOR 5)
        set(QT_VERSION ${Qt5_VERSION})
        message(STATUS "  ✓ Qt5 ${Qt5_VERSION} found (system)")

        # Try to find optional components
        foreach(_opt_component ${QT_OPTIONAL_COMPONENTS})
            find_package(Qt5 COMPONENTS ${_opt_component} CONFIG QUIET)
            if(Qt5${_opt_component}_FOUND)
                message(STATUS "  ✓ Qt5::${_opt_component} found (optional)")
                list(APPEND QT_REQUIRED_COMPONENTS ${_opt_component})
            else()
                message(STATUS "  - Qt5::${_opt_component} not found (optional, skipped)")
            endif()
        endforeach()
    endif()
endif()

# ============================================================================
# Create individual component targets: client::qt6::COMPONENT
# ============================================================================

if(QT_FOUND)
    set(_QT_AVAILABLE_COMPONENTS "")

    foreach(_component ${QT_REQUIRED_COMPONENTS})
        set(_qt_target "Qt${QT_VERSION_MAJOR}::${_component}")

        # Check if the Qt target exists
        if(TARGET ${_qt_target})
            list(APPEND _QT_AVAILABLE_COMPONENTS ${_component})

            # Create a client::qt6::component target (or client::qt5::component for Qt5)
            set(_component_target "client::qt${QT_VERSION_MAJOR}::${_component}")

            if(NOT TARGET ${_component_target})
                # Create an interface library as an alias/wrapper around the Qt component target
                add_library(qt${QT_VERSION_MAJOR}_${_component} INTERFACE IMPORTED)
                target_link_libraries(qt${QT_VERSION_MAJOR}_${_component} INTERFACE ${_qt_target})

                # Create the namespaced alias
                add_library(${_component_target} ALIAS qt${QT_VERSION_MAJOR}_${_component})

                message(STATUS "  ✓ ${_component_target} created")
            endif()
        elseif(TARGET Qt::${_component})
            # Fallback for some Qt configurations
            list(APPEND _QT_AVAILABLE_COMPONENTS ${_component})
            set(_component_target "client::qt${QT_VERSION_MAJOR}::${_component}")

            if(NOT TARGET ${_component_target})
                add_library(qt${QT_VERSION_MAJOR}_${_component} INTERFACE IMPORTED)
                target_link_libraries(qt${QT_VERSION_MAJOR}_${_component} INTERFACE Qt::${_component})
                add_library(${_component_target} ALIAS qt${QT_VERSION_MAJOR}_${_component})

                message(STATUS "  ✓ ${_component_target} created (via Qt::)")
            endif()
        endif()
    endforeach()

    if(_QT_AVAILABLE_COMPONENTS)
        list(JOIN _QT_AVAILABLE_COMPONENTS ", " _components_str)
        message(STATUS "  ✓ Qt components available: ${_components_str}")
    endif()

    # =========================================================================
    # Create umbrella target (client::qt6) linking to ALL components
    # =========================================================================

    if(NOT TARGET client::qt6)
        add_library(qt6 INTERFACE)
        add_library(client::qt6 ALIAS qt6)

        # Collect all Qt targets to link
        set(_qt_targets "")

        foreach(_component ${QT_REQUIRED_COMPONENTS})
            set(_qt_target "Qt${QT_VERSION_MAJOR}::${_component}")
            if(TARGET ${_qt_target})
                list(APPEND _qt_targets ${_qt_target})
            elseif(TARGET Qt::${_component})
                list(APPEND _qt_targets Qt::${_component})
            endif()
        endforeach()

        # Link all Qt targets to the umbrella interface
        if(_qt_targets)
            target_link_libraries(qt6 INTERFACE ${_qt_targets})
        endif()

        # Set up Qt-specific compile definitions
        if(QT_VERSION_MAJOR EQUAL 6)
            target_compile_definitions(qt6 INTERFACE QT_VERSION_MAJOR=6)
        else()
            target_compile_definitions(qt6 INTERFACE QT_VERSION_MAJOR=5)
        endif()

        # Define macros for optional components that were found
        foreach(_component ${QT_OPTIONAL_COMPONENTS})
            string(TOUPPER ${_component} _component_upper)
            if(TARGET Qt${QT_VERSION_MAJOR}::${_component} OR TARGET Qt::${_component})
                target_compile_definitions(qt6 INTERFACE QT_HAS_${_component_upper})
            endif()
        endforeach()

        message(STATUS "  ✓ client::qt6 (umbrella) configured with Qt${QT_VERSION_MAJOR}.${QT_VERSION}")
        message(STATUS "    QML/Quick support: enabled")
    endif()

    # Track the dependency, but avoid duplicates
    list(FIND _CLIENT_DEPENDENCIES_FOUND "Qt${QT_VERSION_MAJOR}:${QT_VERSION}" _qt_dep_index)
    if(_qt_dep_index EQUAL -1)
        list(APPEND _CLIENT_DEPENDENCIES_FOUND "Qt${QT_VERSION_MAJOR}:${QT_VERSION}")
        set(_CLIENT_DEPENDENCIES_FOUND "${_CLIENT_DEPENDENCIES_FOUND}" CACHE INTERNAL "List of found dependencies")
    endif()

    # =========================================================================
    # Clean up temporary variables
    # =========================================================================

    unset(_QT_AVAILABLE_COMPONENTS)
    unset(_component)
    unset(_qt_target)
    unset(_component_target)
    unset(_components_str)
    unset(_qt_targets)

else()
    message(FATAL_ERROR
        "Qt not found!\n"
        "Please install Qt6 (or Qt5) with at least these components: ${QT_REQUIRED_COMPONENTS}\n"
        "\n"
        "Optional components (recommended): ${QT_OPTIONAL_COMPONENTS}\n"
    )
endif()

# Note: Variables are already set in the current scope and will be
# available to the parent via include() - no PARENT_SCOPE needed
# since this file is included, not added via add_subdirectory()

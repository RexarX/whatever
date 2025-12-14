# ESP-IDF main component CMakeLists.txt
# This file defines the main application component for the embedded firmware

# Include IDF utilities for component configuration
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/IdfUtils.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/IdfUtils.cmake")
endif()

idf_component_register(
    SRCS
        main.cpp
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        proto_nanopb
        bluetooth_spp
        servo
        bt
        esp_timer
        driver
        #esp_wifi
        #esp_event
)

# Apply standard configuration using IdfUtils
# This sets C++23, Google-style warnings, optimizations, and platform definitions
if(COMMAND embedded_idf_configure_component)
    embedded_idf_configure_component(${COMPONENT_LIB})
else()
    # Fallback if IdfUtils not available
    set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 23)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_EXTENSIONS OFF)

    target_compile_options(${COMPONENT_LIB} PRIVATE
        -std=c++23
        $<$<CONFIG:DEBUG>:-Og -g>
        $<$<CONFIG:RELEASE>:-O3>
    )
endif()

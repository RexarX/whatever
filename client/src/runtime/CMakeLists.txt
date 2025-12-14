# Client Runtime Library
# Static library containing the application logic, GUI, and face detection.
# Uses OpenCV and Qt, links to core (static) and comm (shared).

cmake_minimum_required(VERSION 3.25)

# Runtime library sources
set(CLIENT_RUNTIME_SOURCES
    src/app.cpp
    src/camera.cpp
    src/face_tracker.cpp
    src/frame.cpp
    src/gui_window.cpp
    src/settings_manager.cpp
    src/pch.cpp
)

# Runtime library headers
set(CLIENT_RUNTIME_HEADERS
    include/client/app/app.hpp
    include/client/app/app_return_code.hpp
    include/client/app/camera.hpp
    include/client/app/face_data.hpp
    include/client/app/face_tracker.hpp
    include/client/app/frame.hpp
    include/client/app/gui_window.hpp
    include/client/app/model_config.hpp
    include/client/app/settings_manager.hpp
    include/client/pch.hpp
)

# Create static library for runtime
add_library(client_runtime STATIC
    ${CLIENT_RUNTIME_SOURCES}
    ${CLIENT_RUNTIME_HEADERS}
)
add_library(client::runtime ALIAS client_runtime)

# Apply standard configurations
if(COMMAND client_target_set_warnings)
    client_target_set_warnings(client_runtime)
endif()
if(COMMAND client_target_set_optimization)
    client_target_set_optimization(client_runtime)
endif()
if(COMMAND client_target_set_platform)
    client_target_set_platform(client_runtime)
endif()
if(COMMAND client_target_set_output_dirs)
    client_target_set_output_dirs(client_runtime)
endif()
if(COMMAND client_target_set_cxx_standard)
    client_target_set_cxx_standard(client_runtime STANDARD 23)
endif()

# Link dependencies - runtime uses OpenCV, Qt, and the libraries
target_link_libraries(client_runtime
    PUBLIC
        client::core
        client::comm

        client::qt6::Multimedia

        client::opencv4::core
        client::opencv4::videoio

    PRIVATE
        client::qt6
        client::opencv4
        #client::qt6::Core
        #client::qt6::Gui
        #client::qt6::Widgets
        #client::qt6::Network
        #client::qt6::Quick
        #client::qt6::Qml
        #client::qt6::QuickControls2
        #client::qt6::SerialPort

        #client::opencv4::imgproc
        #client::opencv4::objdetect
        #client::opencv4::video
        #client::opencv4::highgui
        #client::opencv4::dnn
)

# Include directories
target_include_directories(client_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# C++ standard and properties
set_target_properties(client_runtime PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Add precompiled header
if(COMMAND client_target_add_pch)
    client_target_add_pch(client_runtime ${CMAKE_CURRENT_SOURCE_DIR}/include/client/pch.hpp PUBLIC)
endif()

# Enable LTO if requested
if(CLIENT_ENABLE_LTO AND COMMAND client_target_enable_lto)
    client_target_enable_lto(client_runtime)
endif()

# Set IDE folder
if(COMMAND client_target_set_folder)
    client_target_set_folder(client_runtime "Libraries")
endif()
